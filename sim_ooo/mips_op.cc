#include <cmath>
#include <map>
#include <set>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "globals.hh"
#include "mips_op.hh"
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



mips_meta_op::~mips_meta_op() {
  if(op) {
    delete op;
  }
}

void mips_meta_op::release() {
  if(op) {
    delete op;
    op = nullptr;
  }
}

bool riscv_op::allocate(sim_state &machine_state) {
  die();
  return false;
}

void riscv_op::execute(sim_state &machine_state) {
  die();
}

void riscv_op::complete(sim_state &machine_state) {
  die();
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
  //*global::sim_log  << *this << "\n";
  //std::cout << machine_state.rob.size() << "\n";

  //std::cout << *this << " : retirement freevec = "
  //<< machine_state.gpr_freevec_retire.popcount()
  //<< "\n";
  
  //assert(machine_state.gpr_freevec_retire.popcount()==sim_state::num_gpr_regs);
  //assert(machine_state.cpr1_freevec_retire.popcount()==sim_state::num_cpr1_regs);
  
}

void riscv_op::log_rollback(sim_state &machine_state) const {}

int64_t riscv_op::get_latency() const {
  return 1;
}

class nop_op : public riscv_op {
  nop_op(sim_op op) : riscv_op(op) {
    this->op_class = oper_type::alu;
  }
  bool allocate(sim_state &machine_state) override {
    return true;
  }
  bool ready(sim_state &machine_state) const override  {
    return true;
  }
  void execute(sim_state &machine_state) override {}
};

class gpr_dst_op : public riscv_op {
public:
  gpr_dst_op(sim_op op) : riscv_op(op) {}
  int get_dest() const override {
    return (m->inst>>7) & 31;    
  }
   void complete(sim_state &machine_state) override {
    if(not(m->is_complete) and (get_curr_cycle() == m->complete_cycle)) {
      m->is_complete = true;
      machine_state.gpr_valid.set_bit(m->prf_idx);
    }
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
    assert(false);
  }
  bool ready(sim_state &machine_state) const override  {
    if(m->src0_prf != -1 and not(machine_state.gpr_valid.get_bit(m->src0_prf))) {
      return false;
    }
    return true;
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
  }
};

riscv_op* decode_insn(sim_op m_op) {
  uint32_t opcode = (m_op->inst)&127;
  uint32_t rd = (m_op-=>inst>>7) & 31;
  switch(opcode)
    {
    case 0x13: /* nop */
      return new itype_op(m_op);
    case 0x17: /* auipc */
      return new rd==0 ? nop_op(m_op) : auipc_op(m_op);
    default:
      break;
    }
  std::cout << "implement opcode " << std::hex << opcode << std::dec << "\n";
  return nullptr;
}
