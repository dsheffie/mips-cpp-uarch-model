#include <cmath>
#include <map>
#include <set>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "globals.hh"
#include "riscv_op.hh"
#include "helper.hh"
#include "disassemble.hh"
#include "sim_parameters.hh"
#include "sim_cache.hh"
#include "machine_state.hh"


std::ostream &operator<<(std::ostream &out, const riscv_op &op) {
  out << std::hex << op.m->pc << std::dec
      << ":" << getAsmString(op.m->inst, op.m->pc) << ":"
      << op.m->fetch_cycle << ","
      << op.m->decode_cycle << ","
      << op.m->alloc_cycle << ","
      << op.m->ready_cycle << ","
      << op.m->dispatch_cycle << ","
      << op.m->complete_cycle;
  return out;
}

bool riscv_op::allocate(sim_state &machine_state) {
  die();
  return false;
}

void riscv_op::execute(sim_state &machine_state) {
  die();
}

void riscv_op::complete(sim_state &machine_state) {
  if(not(m->is_complete) and (get_curr_cycle() == m->complete_cycle)) {
    m->is_complete = true;
    if(m->prf_idx != -1) {
      machine_state.gpr_valid.set_bit(m->prf_idx);
    }
  }
}

bool riscv_op::retire(sim_state &machine_state) {
  log_retire(machine_state);

  return false;
}

bool riscv_op::ready(sim_state &machine_state) const {
  return false;
}

void riscv_op::rollback(sim_state &machine_state) {
  log_rollback(machine_state);
  die();
}

void riscv_op::log_retire(sim_state &machine_state) const {

  if((machine_state.icnt >= global::pipestart) and
     (machine_state.icnt < global::pipeend)) {

    machine_state.sim_records->append(m->alloc_id,
				      getAsmString(m->inst, m->pc),
				      m->pc,
				      static_cast<uint64_t>(m->fetch_cycle),
				      static_cast<uint64_t>(m->alloc_cycle),
				      static_cast<uint64_t>(m->complete_cycle),
				      static_cast<uint64_t>(m->retire_cycle),
				      false
				      );
  }
  std::cout << "RETIRED INSTRUCTION for PC " << std::hex << m->pc << std::dec << "\n";
}

void riscv_op::log_rollback(sim_state &machine_state) const {}

int64_t riscv_op::get_latency() const {
  return 1;
}

class nop_op : public riscv_op {
protected:
  bool bad_spec;
public:
  nop_op(sim_op op, bool bad_spec=false) : riscv_op(op), bad_spec(bad_spec) {
    this->op_class = oper_type::alu;
  }
  bool allocate(sim_state &machine_state) override {
    return true;
  }
  bool ready(sim_state &machine_state) const override  {
    return true;
  }
  void execute(sim_state &machine_state) override {
    m->complete_cycle = get_curr_cycle() + get_latency();
  }
  bool retire(sim_state &machine_state) override {
    assert(not(bad_spec));
    m->retire_cycle = get_curr_cycle();
    log_retire(machine_state);
    return true;
  }
  void rollback(sim_state &machine_state) override {
    log_rollback(machine_state);
  }
};

class gpr_dst_op : public riscv_op {
public:
  gpr_dst_op(sim_op op) : riscv_op(op) {}
  int get_dest() const override {
    return (m->inst>>7) & 31;    
  }
  bool retire(sim_state &machine_state) override {
    if(m->is_complete == false) {
      die();
    }
    machine_state.gpr_freevec.clear_bit(m->prev_prf_idx);
    machine_state.gpr_valid.clear_bit(m->prev_prf_idx);
    retired = true;
    machine_state.icnt++;
    machine_state.arch_grf[get_dest()] = machine_state.gpr_prf[m->prf_idx];
    machine_state.arch_grf_last_pc[get_dest()] = m->pc;
    machine_state.gpr_rat_retire[get_dest()] = m->prf_idx;
    machine_state.gpr_freevec_retire.set_bit(m->prf_idx);
    machine_state.gpr_freevec_retire.clear_bit(m->prev_prf_idx);
    m->retire_cycle = get_curr_cycle();
    log_retire(machine_state);
    return true;
  }
  void rollback(sim_state &machine_state) override {
    if(get_dest() > 0) {
      if(m->prev_prf_idx != -1) {
	machine_state.gpr_rat[get_dest()] = m->prev_prf_idx;
      }
      if(m->prf_idx != -1) {
	machine_state.gpr_freevec.clear_bit(m->prf_idx);
	machine_state.gpr_valid.clear_bit(m->prf_idx);
      }
    }
    log_rollback(machine_state);
  }  
};

class itype_op : public gpr_dst_op {
public:
  itype_op(sim_op op) : gpr_dst_op(op) {
    this->op_class = oper_type::alu;
  };
  int get_src0() const override {
    return di.i.rs1;
  }
  bool allocate(sim_state &machine_state) override {
    if(get_src0() != -1) {
      m->src0_prf = machine_state.gpr_rat[get_src0()];
    }
    if(get_dest() > 0) {
      m->prev_prf_idx = machine_state.gpr_rat[get_dest()];
      int64_t prf_id = machine_state.gpr_freevec.find_first_unset();
      if(prf_id == -1) {
	return false;
      }
      machine_state.gpr_freevec.set_bit(prf_id);
      machine_state.gpr_rat[get_dest()] = prf_id;
      m->prf_idx = prf_id;
      machine_state.gpr_valid.clear_bit(prf_id);
    }
    return true;
  }
  void execute(sim_state &machine_state) override {
    assert(di.i.rd);
    int32_t simm32 = (m->inst >> 20);
    simm32 |= ((m->inst>>31)&1) ? 0xfffff000 : 0x0;
    int64_t simm64 = simm32;
    simm64 = (simm64 <<32) >> 32;
    uint32_t subop =(m->inst>>12)&7;
    uint32_t shamt = (m->inst>>20) & 63;
    switch(di.i.sel)
      {
      case 0: {/* addi */
	machine_state.gpr_prf[m->prf_idx] = machine_state.gpr_prf[m->src0_prf] + simm64;
	break;
      }
      default:
	die();
      }
    m->complete_cycle = get_curr_cycle() + get_latency();
  }
  bool ready(sim_state &machine_state) const override  {
    if(m->src0_prf != -1 and not(machine_state.gpr_valid.get_bit(m->src0_prf))) {
      return false;
    }
    return true;
  }

};


class jalr_op : public riscv_op {
public:
  jalr_op(sim_op op) : riscv_op(op) {
    this->op_class = oper_type::jmp;
    op->could_cause_exception = true;
  };
  int get_src0() const override {
    return di.jj.rs1;
  }
  int get_dest() const override {
    return di.jj.rd;    
  }
  bool allocate(sim_state &machine_state) override {
    if(get_src0() != -1) {
      m->src0_prf = machine_state.gpr_rat[get_src0()];
    }
    if(get_dest() > 0) {
      m->prev_prf_idx = machine_state.gpr_rat[get_dest()];
      int64_t prf_id = machine_state.gpr_freevec.find_first_unset();
      if(prf_id == -1) {
	return false;
      }
      machine_state.gpr_freevec.set_bit(prf_id);
      machine_state.gpr_rat[get_dest()] = prf_id;
      m->prf_idx = prf_id;
      machine_state.gpr_valid.clear_bit(prf_id);
    }
    return true;
  }
  bool ready(sim_state &machine_state) const override  {
    if(m->src0_prf != -1 and not(machine_state.gpr_valid.get_bit(m->src0_prf))) {
      return false;
    }
    return true;
  }
  void execute(sim_state &machine_state) override {
    int32_t tgt = di.jj.imm11_0;
    tgt |= ((m->inst>>31)&1) ? 0xfffff000 : 0x0;
    int64_t tgt64 = tgt;
    tgt64 = (tgt64<<32)>>32;
    
    tgt64 += machine_state.gpr_prf[m->src0_prf];
    tgt64 &= ~(1UL);
    m->correct_pc = tgt64;
    //std::cout << "executing jalr, predicted target = " << std::hex << m->fetch_npc
    //<< ", actual target = " <<  m->correct_pc << std::dec << "\n";
    
    if(m->fetch_npc != m->correct_pc) {
      m->exception = exception_type::branch;
    }
    if(get_dest() != -1) {
      machine_state.gpr_prf[m->prf_idx] = m->pc + 4;
    }
    m->complete_cycle = get_curr_cycle() + get_latency();
  }
  bool retire(sim_state &machine_state) override {
    if(m->prev_prf_idx != -1) {
      machine_state.gpr_freevec.clear_bit(m->prev_prf_idx);
      machine_state.gpr_valid.clear_bit(m->prev_prf_idx);
      machine_state.arch_grf[get_dest()] = machine_state.gpr_prf[m->prf_idx];
      machine_state.arch_grf_last_pc[get_dest()] = m->pc;
      machine_state.gpr_rat_retire[get_dest()] = m->prf_idx;
      machine_state.gpr_freevec_retire.set_bit(m->prf_idx);
      machine_state.gpr_freevec_retire.clear_bit(m->prev_prf_idx);
    }
    retired = true;
    machine_state.icnt++;
    machine_state.n_jumps++;
    
    machine_state.branch_pred->update(m->pc, m->pht_idx, true);


    uint32_t bht_idx = (m->pc>>2) & (machine_state.bht.size()-1);
    machine_state.bht.at(bht_idx).shift_left(1);
    machine_state.bht.at(bht_idx).set_bit(0);
    
    machine_state.bhr.shift_left(1);
    machine_state.bhr.set_bit(0);

    if(m->exception==exception_type::branch) {
      machine_state.mispredicted_jumps++;
      machine_state.alloc_blocked = true;
    }
    m->retire_cycle = get_curr_cycle();
    //std::cout << "jalr retire..\n";
    log_retire(machine_state);
    return true;
  }
  void rollback(sim_state &machine_state) override {
    if(get_dest() != -1) {
      if(m->prev_prf_idx != -1) {
	machine_state.gpr_rat[get_dest()] = m->prev_prf_idx;
      }
      if(m->prf_idx != -1) {
	machine_state.gpr_freevec.clear_bit(m->prf_idx);
	machine_state.gpr_valid.clear_bit(m->prf_idx);
      }
    }
    //if( ((jt == jump_type::jal) or (jt == jump_type::jr)) and
    //(m->return_stack_idx != -1)) {
    //machine_state.return_stack.set_tos_idx(m->return_stack_idx);
    //}
    log_rollback(machine_state);
  }
};

class jal_op : public riscv_op {
public:
  jal_op(sim_op op) : riscv_op(op) {
    this->op_class = oper_type::jmp;
    op->could_cause_exception = true;
  };
  int get_dest() const override {
    return di.jj.rd;    
  }
  bool allocate(sim_state &machine_state) override {
    if(get_dest() > 0) {
      m->prev_prf_idx = machine_state.gpr_rat[get_dest()];
      int64_t prf_id = machine_state.gpr_freevec.find_first_unset();
      if(prf_id == -1) {
	return false;
      }
      machine_state.gpr_freevec.set_bit(prf_id);
      machine_state.gpr_rat[get_dest()] = prf_id;
      m->prf_idx = prf_id;
      machine_state.gpr_valid.clear_bit(prf_id);
    }
    return true;
  }
  bool ready(sim_state &machine_state) const override  {
    return true;
  }

  void execute(sim_state &machine_state) override {
    int32_t jaddr32 =
      (di.j.imm10_1 << 1)   |
      (di.j.imm11 << 11)    |
      (di.j.imm19_12 << 12) |
      (di.j.imm20 << 20);
    jaddr32 |= ((m->inst>>31)&1) ? 0xffe00000 : 0x0;
    int64_t jaddr = jaddr32;
    jaddr = (jaddr << 32) >> 32;
    m->correct_pc = m->pc + jaddr;
    
    if(m->fetch_npc != m->correct_pc) {
      m->exception = exception_type::branch;
    }
    if(get_dest() != -1) {
      machine_state.gpr_prf[m->prf_idx] = m->pc + 4;
    }
    m->complete_cycle = get_curr_cycle() + get_latency();
  }
  bool retire(sim_state &machine_state) override {
    if(m->prev_prf_idx != -1) {
      machine_state.gpr_freevec.clear_bit(m->prev_prf_idx);
      machine_state.gpr_valid.clear_bit(m->prev_prf_idx);
      machine_state.arch_grf[get_dest()] = machine_state.gpr_prf[m->prf_idx];
      machine_state.arch_grf_last_pc[get_dest()] = m->pc;
      machine_state.gpr_rat_retire[get_dest()] = m->prf_idx;
      machine_state.gpr_freevec_retire.set_bit(m->prf_idx);
      machine_state.gpr_freevec_retire.clear_bit(m->prev_prf_idx);
    }
    retired = true;
    machine_state.icnt++;
    machine_state.n_jumps++;
    
    machine_state.branch_pred->update(m->pc, m->pht_idx, true);


    uint32_t bht_idx = (m->pc>>2) & (machine_state.bht.size()-1);
    machine_state.bht.at(bht_idx).shift_left(1);
    machine_state.bht.at(bht_idx).set_bit(0);
    
    machine_state.bhr.shift_left(1);
    machine_state.bhr.set_bit(0);

    if(m->exception==exception_type::branch) {
      machine_state.mispredicted_jumps++;
      machine_state.alloc_blocked = true;
    }
    m->retire_cycle = get_curr_cycle();
    //std::cout << "jalr retire..\n";
    log_retire(machine_state);
    return true;
  }
  void rollback(sim_state &machine_state) override {
    if(get_dest() != -1) {
      if(m->prev_prf_idx != -1) {
	machine_state.gpr_rat[get_dest()] = m->prev_prf_idx;
      }
      if(m->prf_idx != -1) {
	machine_state.gpr_freevec.clear_bit(m->prf_idx);
	machine_state.gpr_valid.clear_bit(m->prf_idx);
      }
    }
    //if( ((jt == jump_type::jal) or (jt == jump_type::jr)) and
    //(m->return_stack_idx != -1)) {
    //machine_state.return_stack.set_tos_idx(m->return_stack_idx);
    //}
    log_rollback(machine_state);
  }
};


class branch_op : public riscv_op {
public:
  branch_op(sim_op op) : riscv_op(op) {
    this->op_class = oper_type::jmp;
    op->could_cause_exception = true;
  };
  int get_src0() const override {
    return di.b.rs1;
  }
  int get_src1() const override {
    return di.b.rs2;
  }
  bool allocate(sim_state &machine_state) override {
    if(get_src0() != -1) {
      m->src0_prf = machine_state.gpr_rat[get_src0()];
    }
    if(get_src1() != -1) {
      m->src1_prf = machine_state.gpr_rat[get_src1()];
    }
    return true;
  }
  bool ready(sim_state &machine_state) const override  {
    if(m->src0_prf != -1 and not(machine_state.gpr_valid.get_bit(m->src0_prf))) {
      return false;
    }
    if(m->src1_prf != -1 and not(machine_state.gpr_valid.get_bit(m->src1_prf))) {
      return false;
    }    
    return true;
  }
  void execute(sim_state &machine_state) override {
    int32_t disp32 =
	(di.b.imm4_1 << 1)  |
	(di.b.imm10_5 << 5) |	
        (di.b.imm11 << 11)  |
      (di.b.imm12 << 12);
    disp32 |= di.b.imm12 ? 0xffffe000 : 0x0;
    int64_t disp = disp32;
    disp = (disp << 32) >> 32;
    bool takeBranch = false;
    uint64_t u_rs1 = *reinterpret_cast<uint64_t*>(&machine_state.gpr_prf[m->src0_prf]);
    uint64_t u_rs2 = *reinterpret_cast<uint64_t*>(&machine_state.gpr_prf[m->src1_prf]);
    switch(di.b.sel)
      {
      case 0: /* beq */
	takeBranch = machine_state.gpr_prf[m->src0_prf] == machine_state.gpr_prf[m->src1_prf];
	break;
      case 1: /* bne */
	takeBranch = machine_state.gpr_prf[m->src0_prf] != machine_state.gpr_prf[m->src1_prf];
	break;
      case 4: /* blt */
	takeBranch = machine_state.gpr_prf[m->src0_prf] < machine_state.gpr_prf[m->src1_prf];
	break;
      case 5: /* bge */
	takeBranch = machine_state.gpr_prf[m->src0_prf] >= machine_state.gpr_prf[m->src1_prf];	  
	break;
      case 6: /* bltu */
	takeBranch = u_rs1 < u_rs2;
	break;
      case 7: /* bgeu */
	takeBranch = u_rs1 >= u_rs2;
	break;
      default:
	std::cout << "implement case " << di.b.sel << "\n";
	assert(0);
      }
    m->correct_pc = takeBranch ? disp + m->pc : m->pc + 4;
    if(m->fetch_npc != m->correct_pc) {
      m->exception = exception_type::branch;
    }
    m->complete_cycle = get_curr_cycle() + get_latency();
  }
  bool retire(sim_state &machine_state) override {
    retired = true;
    machine_state.icnt++;
    machine_state.n_jumps++;
    
    machine_state.branch_pred->update(m->pc, m->pht_idx, true);
    uint32_t bht_idx = (m->pc>>2) & (machine_state.bht.size()-1);
    machine_state.bht.at(bht_idx).shift_left(1);
    machine_state.bht.at(bht_idx).set_bit(0);
    
    machine_state.bhr.shift_left(1);
    machine_state.bhr.set_bit(0);

    if(m->exception==exception_type::branch) {
      machine_state.mispredicted_jumps++;
      machine_state.alloc_blocked = true;
    }
    m->retire_cycle = get_curr_cycle();
    log_retire(machine_state);
    return true;
  }
  void rollback(sim_state &machine_state) override {
    if(get_dest() != -1) {
      if(m->prev_prf_idx != -1) {
	machine_state.gpr_rat[get_dest()] = m->prev_prf_idx;
      }
      if(m->prf_idx != -1) {
	machine_state.gpr_freevec.clear_bit(m->prf_idx);
	machine_state.gpr_valid.clear_bit(m->prf_idx);
      }
    }
    //if( ((jt == jump_type::jal) or (jt == jump_type::jr)) and
    //(m->return_stack_idx != -1)) {
    //machine_state.return_stack.set_tos_idx(m->return_stack_idx);
    //}
    log_rollback(machine_state);
  }
};

class auipc_op : public gpr_dst_op {
public:
  auipc_op(sim_op op) : gpr_dst_op(op) {
    this->op_class = oper_type::alu;
  }
  int get_dest() const override {
    return (m->inst>>7) & 31;    
  }
  bool allocate(sim_state &machine_state) override {
    if(get_dest() > 0) {
      m->prev_prf_idx = machine_state.gpr_rat[get_dest()];
      int64_t prf_id = machine_state.gpr_freevec.find_first_unset();
      if(prf_id == -1) {
	return false;
      }
      machine_state.gpr_freevec.set_bit(prf_id);
      machine_state.gpr_rat[get_dest()] = prf_id;
      m->prf_idx = prf_id;
      machine_state.gpr_valid.clear_bit(prf_id);
    }
    return true;
  }
  bool ready(sim_state &machine_state) const override  {
    return true;
  }
  void execute(sim_state &machine_state) override {
    int64_t imm = m->inst & (~4095);
    imm = (imm << 32) >> 32;
    int64_t y = m->pc + imm;
    machine_state.gpr_prf[m->prf_idx] = y;
    m->complete_cycle = get_curr_cycle() + get_latency();
  }
};

riscv_op* decode_insn(sim_op m_op) {
  uint32_t opcode = (m_op->inst)&127;
  uint32_t rd = (m_op->inst>>7) & 31;
  riscv::riscv_t m(m_op->inst);

  switch(opcode)
    {
    case 0x0:
      return new nop_op(m_op, true);
    case 0x13: 
      return new itype_op(m_op);
    case 0x17: /* auipc */
      return new auipc_op(m_op);
    case 0x23: 
      return new riscv_store(m_op, riscv_store::stypes[m.s.sel]);
    case 0x50:
      return new nop_op(m_op, true);
      break;
    case 0x63: /* branches */
      return new branch_op(m_op);
    case 0x67: /* jalr */
      return new jalr_op(m_op);
    case 0x6f: /* jal */
      return new jal_op(m_op);
    case 0x73: { /* system */
      return new nop_op(m_op, false);      
      break;
    }
    default:
      break;
    }
  std::cout << "implement opcode " << std::hex << opcode << std::dec << "\n";
  return nullptr;
}


void meta_op::release() {
  if(op) {
    delete op;
    op = nullptr;    
  }
}


meta_op::~meta_op() {
  release();
}


bool riscv_store::allocate(sim_state &machine_state)  {
  m->src0_prf = machine_state.gpr_rat[get_src0()];
  m->src1_prf = machine_state.gpr_rat[get_src1()];
  m->store_tbl_idx = machine_state.store_tbl_freevec.find_first_unset();
  if(m->store_tbl_idx==-1) {
    return false;
  }
  machine_state.store_tbl[m->store_tbl_idx] = m;
  machine_state.store_tbl_freevec.set_bit(m->store_tbl_idx);
  return true;
}

bool riscv_store::ready(sim_state &machine_state) const  {
  if(not(machine_state.gpr_valid.get_bit(m->src0_prf)) or
     not(machine_state.gpr_valid.get_bit(m->src1_prf))) {
    return false;
  }
  return true;
}

void riscv_store::execute(sim_state &machine_state){
  effective_address = machine_state.gpr_prf[m->src1_prf] + imm;
  store_data = machine_state.gpr_prf[m->src0_prf];
  //if(machine_state.l1d) {
  //machine_state.l1d->write(m,effective_address & (~3U), 4);
  //}
  //else {
  m->complete_cycle = get_curr_cycle() + 1;/*sim_param::l1d_latency;*/
}

bool riscv_store::retire(sim_state &machine_state) {
  uint8_t *mem = machine_state.mem;
  switch(st)
    {
    case store_type::sb:
      *reinterpret_cast<int8_t*>(mem) = static_cast<int8_t>(store_data);
      break;
    case store_type::sh:
      *reinterpret_cast<int16_t*>(mem) = static_cast<int16_t>(store_data);      
      break;
    case store_type::sw:
      *reinterpret_cast<int32_t*>(mem) = static_cast<int32_t>(store_data);            
      break;
    case store_type::sd:
      *reinterpret_cast<int64_t*>(mem) = store_data;
      break;
    default:
      die();
    }
  retired = true;
 bool load_violation = false;

 for(size_t i = 0; i < machine_state.load_tbl_freevec.size(); i++ ){
   if(machine_state.load_tbl[i]==nullptr) {
     continue;
   }
   meta_op *mmo = machine_state.load_tbl[i];
   if(mmo->alloc_cycle < m->alloc_cycle) {
     continue;
   }
   
   auto ld = reinterpret_cast<riscv_load*>(mmo->op);
   if(ld == nullptr) {
     die();
   }
   if(mmo->is_complete and (effective_address>>3) == (ld->getEA()>>3)) {
     load_violation = true;
     std::cout << "load / store exception, store pc = " << std::hex << m->pc
	       << ", load pc = " << mmo->pc << std::dec << "\n";
   }
 }
 
 for(size_t i = 0; load_violation and (i < machine_state.load_tbl_freevec.size()); i++ ){
   if(machine_state.load_tbl[i]!=nullptr) {
     machine_state.load_tbl[i]->load_exception = true;
   }
 }
 
 machine_state.store_tbl_freevec.clear_bit(m->store_tbl_idx);
 machine_state.store_tbl[m->store_tbl_idx] = nullptr;
 m->retire_cycle = get_curr_cycle();
 
 log_retire(machine_state);
 return true;
}

int64_t riscv_store::get_latency() const {
  return sim_param::l1d_latency;
}
