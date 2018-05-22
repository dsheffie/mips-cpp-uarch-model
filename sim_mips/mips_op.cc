#include "globals.hh"
#include "mips_op.hh"
#include "helper.hh"
#include "parseMips.hh"

#include <map>

std::map<uint32_t, uint32_t> branch_target_map;
std::map<uint32_t, int32_t> branch_prediction_map;

std::map<uint32_t, uint32_t> load_alias_map;

template <typename T>
struct load_thunk {
  union thunk {
    uint32_t u32[sizeof(T)/sizeof(uint32_t)];
    T dt;
  };
  thunk t;
  load_thunk (T dt) {
    t.dt = dt;
  }
  load_thunk () {
    for(size_t i = 0; i < (sizeof(T)/sizeof(uint32_t)); i++) {
      t.u32[i] = 0;
    }
  }
  T &DT() {
    return t.dt;
  }
  uint32_t &operator[](size_t idx) {
    assert(idx <  (sizeof(T)/sizeof(uint32_t)));
    return t.u32[idx];
  }
};


class mtc0 : public mips_op {
public:
  mtc0(sim_op op) : mips_op(op) {
    this->op_class = mips_op_type::alu;
  }
  virtual int get_dest() const {
    return (m->inst >> 11) & 31;
  }
  virtual int get_src0() const {
    return (m->inst >> 16) & 31;
  }
  virtual bool allocate(sim_state &machine_state) {
    m->src0_prf = machine_state.gpr_rat[get_src0()];
    m->prev_prf_idx = machine_state.cpr0_rat[get_dest()];
    int64_t prf_id = machine_state.cpr0_freevec.find_first_unset();
    if(prf_id == -1)
      return false;
    assert(prf_id >= 0);
    machine_state.cpr0_freevec.set_bit(prf_id);
    machine_state.cpr0_rat[get_dest()] = prf_id;
    m->prf_idx = prf_id;
    machine_state.cpr0_valid.clear_bit(prf_id);
    return true;
  }
  virtual bool ready(sim_state &machine_state) const {
    if(m->src0_prf != -1 and not(machine_state.gpr_valid.get_bit(m->src0_prf))) {
      return false;
    }
    return true;
  }
  virtual void execute(sim_state &machine_state) {
    if(not(ready(machine_state))) {
      die();
    }
    machine_state.cpr0_prf[m->prf_idx] = machine_state.gpr_prf[m->src0_prf];
    m->complete_cycle = get_curr_cycle() + 1;
  }
  virtual void complete(sim_state &machine_state) {
    if(not(m->is_complete) and (get_curr_cycle() == m->complete_cycle)) {
      m->is_complete = true;
      machine_state.cpr0_valid.set_bit(m->prf_idx);
    }
  }
  virtual bool retire(sim_state &machine_state) {
    machine_state.cpr0_freevec.clear_bit(m->prev_prf_idx);
    machine_state.cpr0_valid.clear_bit(m->prev_prf_idx);
    retired = true;
    machine_state.icnt++;
    m->retire_cycle = get_curr_cycle();
    return true;
  }
  virtual void undo(sim_state &machine_state) {
    if(m->prev_prf_idx != -1) {
      machine_state.cpr0_rat[get_dest()] = m->prev_prf_idx;
    }
    if(m->prf_idx != -1) {
      machine_state.cpr0_freevec.clear_bit(m->prf_idx);
      machine_state.cpr0_valid.clear_bit(m->prf_idx);
    }
  }
};

class mtc1 : public mips_op {
public:
  mtc1(sim_op op) : mips_op(op) {
    this->op_class = mips_op_type::alu;
  }
  virtual int get_dest() const {
    return (m->inst >> 11) & 31;
  }
  virtual int get_src0() const {
    return (m->inst >> 16) & 31;
  }
  virtual bool allocate(sim_state &machine_state) {
    m->src0_prf = machine_state.gpr_rat[get_src0()];
    m->prev_prf_idx = machine_state.cpr1_rat[get_dest()];
    int64_t prf_id = machine_state.cpr1_freevec.find_first_unset();
    if(prf_id == -1)
      return false;
    assert(prf_id >= 0);
    machine_state.cpr1_freevec.set_bit(prf_id);
    machine_state.cpr1_rat[get_dest()] = prf_id;
    m->prf_idx = prf_id;
    machine_state.cpr1_valid.clear_bit(prf_id);
    return true;
  }
  virtual bool ready(sim_state &machine_state) const {
    if(m->src0_prf != -1 and not(machine_state.gpr_valid.get_bit(m->src0_prf))) {
      return false;
    }
    return true;
  }
  virtual void execute(sim_state &machine_state) {
    machine_state.cpr1_prf[m->prf_idx] = machine_state.gpr_prf[m->src0_prf];
    m->complete_cycle = get_curr_cycle() + 1;
  }
  virtual void complete(sim_state &machine_state) {
    if(not(m->is_complete) and (get_curr_cycle() == m->complete_cycle)) {
      m->is_complete = true;
      machine_state.cpr1_valid.set_bit(m->prf_idx);
    }
  }
  virtual bool retire(sim_state &machine_state) {
    machine_state.cpr1_freevec.clear_bit(m->prev_prf_idx);
    machine_state.cpr1_valid.clear_bit(m->prev_prf_idx);
    retired = true;
    machine_state.icnt++;
    machine_state.arch_cpr1[get_dest()] = machine_state.cpr1_prf[m->prf_idx];
    machine_state.arch_cpr1_last_pc[get_dest()] = m->pc;
    m->retire_cycle = get_curr_cycle();
    return true;
  }
  virtual void undo(sim_state &machine_state) {
    machine_state.cpr1_rat[get_dest()] = m->prev_prf_idx;
    machine_state.cpr1_freevec.clear_bit(m->prf_idx);
    machine_state.cpr1_valid.set_bit(m->prf_idx);
  }
};

class mfc1 : public mips_op {
public:
  mfc1(sim_op op) : mips_op(op) {
    this->op_class = mips_op_type::alu;
  }
  virtual int get_dest() const {
    return(m->inst>>16) & 31;
  }
  virtual int get_src0() const {
    return (m->inst >> 11) & 31;
  }
  virtual bool allocate(sim_state &machine_state) {
    m->src0_prf = machine_state.cpr1_rat[get_src0()];
    m->prev_prf_idx = machine_state.gpr_rat[get_dest()];
    int64_t prf_id = machine_state.gpr_freevec.find_first_unset();
    if(prf_id == -1)
      return false;
    machine_state.gpr_freevec.set_bit(prf_id);
    machine_state.gpr_rat[get_dest()] = prf_id;
    m->prf_idx = prf_id;
    machine_state.gpr_valid.clear_bit(prf_id);
    return true;
  }
  virtual bool ready(sim_state &machine_state) const {
    if(m->src0_prf != -1 and not(machine_state.cpr1_valid.get_bit(m->src0_prf))) {
      return false;
    }
    return true;
  }
  virtual void execute(sim_state &machine_state) {
    if(not(ready(machine_state))) {
      die();
    }
    machine_state.gpr_prf[m->prf_idx] = machine_state.cpr1_prf[m->src0_prf];
    m->complete_cycle = get_curr_cycle() + 1;
  }
  virtual void complete(sim_state &machine_state) {
    if(not(m->is_complete) and (get_curr_cycle() == m->complete_cycle)) {
      m->is_complete = true;
      machine_state.gpr_valid.set_bit(m->prf_idx);
    }
  }
  virtual bool retire(sim_state &machine_state) {
    machine_state.gpr_freevec.clear_bit(m->prev_prf_idx);
    machine_state.gpr_valid.clear_bit(m->prev_prf_idx);

    machine_state.icnt++;
    machine_state.arch_grf[get_dest()] = machine_state.gpr_prf[m->prf_idx];
    machine_state.arch_grf_last_pc[get_dest()] = m->pc;
    m->exec_parity = machine_state.gpr_parity();
    retired = true;
    m->retire_cycle = get_curr_cycle();
    return true;
  }
  virtual void undo(sim_state &machine_state) {
    if(m->prev_prf_idx != -1) {
      machine_state.gpr_rat[get_dest()] = m->prev_prf_idx;
    }
    if(m->prf_idx != -1) {
      machine_state.gpr_freevec.clear_bit(m->prf_idx);
      machine_state.gpr_valid.clear_bit(m->prf_idx);
    }
  }
};


class lo_hi_move : public mips_op {
public:
  enum class lo_hi_type {mfhi, mflo, mthi, mtlo};
protected:
  lo_hi_type lht;
public:
  lo_hi_move(sim_op op, lo_hi_type lht) : mips_op(op), lht(lht) {
    this->op_class = mips_op_type::alu;
  }
  virtual int get_dest() const {
    switch(lht)
      {
      case lo_hi_type::mtlo:
	return 32;
      case lo_hi_type::mthi:
	return 33;
      case lo_hi_type::mflo:
      case lo_hi_type::mfhi:
	break;
      }
    return (m->inst >> 11) & 31;
  }
  virtual int get_src0() const {
    switch(lht)
      {
      case lo_hi_type::mtlo:
      case lo_hi_type::mthi:
	break;
      case lo_hi_type::mflo:
	return 32;
      case lo_hi_type::mfhi:
	return 33;
	break;
      }
    return (m->inst >> 21) & 31;
  } 
  virtual bool allocate(sim_state &machine_state) {
    m->src0_prf = machine_state.gpr_rat[get_src0()];
    m->prev_prf_idx = machine_state.gpr_rat[get_dest()];
    int64_t prf_id = machine_state.gpr_freevec.find_first_unset();
    if(prf_id == -1) {
      return false;
    }
    machine_state.gpr_freevec.set_bit(prf_id);
    machine_state.gpr_rat[get_dest()] = prf_id;
    m->prf_idx = prf_id;
    machine_state.gpr_valid.clear_bit(prf_id);
    return true;
  }
  virtual bool ready(sim_state &machine_state) const {
    if(not(machine_state.gpr_valid.get_bit(m->src0_prf))) {
      return false;
    }
    return true;
  }
  virtual void execute(sim_state &machine_state) {
    machine_state.gpr_prf[m->prf_idx] = machine_state.gpr_prf[m->src0_prf];
    m->complete_cycle = get_curr_cycle() + 1;
  }

  virtual void complete(sim_state &machine_state) {
    if(not(m->is_complete) and (get_curr_cycle() == m->complete_cycle)) {
      m->is_complete = true;
      machine_state.gpr_valid.set_bit(m->prf_idx);
    }
  }
  virtual bool retire(sim_state &machine_state) {
    machine_state.gpr_freevec.clear_bit(m->prev_prf_idx);
    machine_state.gpr_valid.clear_bit(m->prev_prf_idx);
    retired = true;
    machine_state.icnt++;
    machine_state.arch_grf[get_dest()] = machine_state.gpr_prf[m->prf_idx];
    machine_state.arch_grf_last_pc[get_dest()] = m->pc;
    m->exec_parity = machine_state.gpr_parity();
    m->retire_cycle = get_curr_cycle();
    return true;
  }
  virtual void undo(sim_state &machine_state) {
    machine_state.gpr_rat[get_dest()] = m->prev_prf_idx;
    machine_state.gpr_freevec.clear_bit(m->prf_idx);
    machine_state.gpr_valid.clear_bit(m->prf_idx);
  }

};

class rtype_alu_op : public mips_op {
public:
  enum class r_type {sll, srl, sra, srlv, srav,
		     addu, add, subu, sub, and_, 
		     or_, xor_, nor_, slt, sltu,
		     teq, sllv, movn, movz,
		     wrecked};
protected:
  rtype r;
  r_type rt;
  bool take_trap;
public:
  rtype_alu_op(sim_op op, r_type rt) :
    mips_op(op), r(op->inst), rt(rt), take_trap(false) {
    this->op_class = mips_op_type::alu;
  }
  virtual int get_dest() const {
    return (rt != r_type::teq) ? r.rr.rd : -1;
  }
  virtual int get_src0() const {
    return r.rr.rt;
  }
  virtual int get_src1() const {
    return r.rr.rs;
  }
  virtual int get_src2() const {
    switch(rt) 
      {
      case r_type::movn:
      case r_type::movz:
	return get_dest();
	break;
      default:
	break;
      }
    return -1;
  }
  virtual bool allocate(sim_state &machine_state) {
    if(get_src0() != -1) {
      m->src0_prf = machine_state.gpr_rat[get_src0()];
    }
    if(get_src1() != -1) {
      m->src1_prf = machine_state.gpr_rat[get_src1()];
    }
    if(get_src2() != -1) {
      m->src2_prf = machine_state.gpr_rat[get_src2()];
    }
    if(get_dest() > 0) {
      m->prev_prf_idx = machine_state.gpr_rat[get_dest()];
      int64_t prf_id = machine_state.gpr_freevec.find_first_unset();
      if(prf_id == -1)
	return false;

      machine_state.gpr_freevec.set_bit(prf_id);
      machine_state.gpr_rat[get_dest()] = prf_id;
      m->prf_idx = prf_id;
      machine_state.gpr_valid.clear_bit(prf_id);
    }
    return true;
  }
  virtual bool ready(sim_state &machine_state) const {
    if(m->src0_prf != -1 and not(machine_state.gpr_valid.get_bit(m->src0_prf))) {
      return false;
    }
    if(m->src1_prf != -1 and not(machine_state.gpr_valid.get_bit(m->src1_prf))) {
      return false;
    }
    if(m->src2_prf != -1 and not(machine_state.gpr_valid.get_bit(m->src2_prf))) {
      return false;
    }
    return true;
  }
  virtual void execute(sim_state &machine_state) {
    if(m->prf_idx != -1) {
      uint32_t sa = (m->inst >> 6) & 31;
      switch(rt)
	{
	case r_type::sll:
	  machine_state.gpr_prf[m->prf_idx] = machine_state.gpr_prf[m->src0_prf] << sa;
	  break;
	case r_type::srl:
	  machine_state.gpr_prf[m->prf_idx] = static_cast<uint32_t>(machine_state.gpr_prf[m->src0_prf]) >> sa;
	  break;
	case r_type::sra:
	  machine_state.gpr_prf[m->prf_idx] = machine_state.gpr_prf[m->src0_prf] >> sa;
	  break;
	case r_type::sllv:
	  machine_state.gpr_prf[m->prf_idx] = machine_state.gpr_prf[m->src0_prf] <<
	    (machine_state.gpr_prf[m->src1_prf] & 0x1f);
	  break;
	case r_type::srlv:
	  machine_state.gpr_prf[m->prf_idx] = static_cast<uint32_t>(machine_state.gpr_prf[m->src0_prf]) >>
	    (machine_state.gpr_prf[m->src1_prf] & 0x1f);
	  break;
	  
	case r_type::addu: {
	  uint32_t urs = static_cast<uint32_t>(machine_state.gpr_prf[m->src1_prf]);
	  uint32_t urt = static_cast<uint32_t>(machine_state.gpr_prf[m->src0_prf]);
	  machine_state.gpr_prf[m->prf_idx] = (urs + urt);
	  break;
	}
	case r_type::subu: {
	  uint32_t urs = static_cast<uint32_t>(machine_state.gpr_prf[m->src1_prf]);
	  uint32_t urt = static_cast<uint32_t>(machine_state.gpr_prf[m->src0_prf]);
	  uint32_t y = urs - urt;
	  machine_state.gpr_prf[m->prf_idx] = y;
	  break;
	}
	case r_type::and_:
	  machine_state.gpr_prf[m->prf_idx] = machine_state.gpr_prf[m->src0_prf] &
	    machine_state.gpr_prf[m->src1_prf];
	  break;
	case r_type::or_:
	  machine_state.gpr_prf[m->prf_idx] = machine_state.gpr_prf[m->src0_prf] |
	    machine_state.gpr_prf[m->src1_prf];
	  break;
	case r_type::xor_:
	  machine_state.gpr_prf[m->prf_idx] = machine_state.gpr_prf[m->src0_prf] ^
	    machine_state.gpr_prf[m->src1_prf];
	  break;
	case r_type::slt:
	  machine_state.gpr_prf[m->prf_idx] = machine_state.gpr_prf[m->src1_prf] <
	    machine_state.gpr_prf[m->src0_prf];
	  break;
	case r_type::sltu: {
	  uint32_t urs = static_cast<uint32_t>(machine_state.gpr_prf[m->src1_prf]);
	  uint32_t urt = static_cast<uint32_t>(machine_state.gpr_prf[m->src0_prf]);
	  machine_state.gpr_prf[m->prf_idx] = (urs < urt);
	  break;
	}
	case r_type::movn:
	  machine_state.gpr_prf[m->prf_idx] = (machine_state.gpr_prf[m->src0_prf] != 0) ? 
	    machine_state.gpr_prf[m->src1_prf] : machine_state.gpr_prf[m->src2_prf];
	  break;
	case r_type::movz:
	  machine_state.gpr_prf[m->prf_idx] = (machine_state.gpr_prf[m->src0_prf] == 0) ? 
	    machine_state.gpr_prf[m->src1_prf] : machine_state.gpr_prf[m->src2_prf];
	  break;
	case r_type::teq:
	  take_trap = machine_state.gpr_prf[m->src0_prf] == machine_state.gpr_prf[m->src1_prf];
	  break;
	default:
	  dprintf(2, "rtype wtf ((pc = %x)\n", m->pc);
	  die();
	}
    }
    m->complete_cycle = get_curr_cycle() + 1;
  }
  virtual void complete(sim_state &machine_state) {
    if(not(m->is_complete) and (get_curr_cycle() == m->complete_cycle)) {
      m->is_complete = true;
      //dprintf(log_fd, "inst @ %x retiring, prf_idx = %d\n", m->pc, m->prf_idx);
      if(m->prf_idx != -1) {
	machine_state.gpr_valid.set_bit(m->prf_idx);
      }
    }
  }
  virtual bool retire(sim_state &machine_state) {
    if(m->prev_prf_idx != -1) {
      machine_state.gpr_freevec.clear_bit(m->prev_prf_idx);
      machine_state.gpr_valid.clear_bit(m->prev_prf_idx);

      machine_state.arch_grf[get_dest()] = machine_state.gpr_prf[m->prf_idx];
      machine_state.arch_grf_last_pc[get_dest()] = m->pc;
      m->exec_parity = machine_state.gpr_parity();
    }
    retired = true;
    machine_state.icnt++;
    if(take_trap) {
      dprintf(log_fd, "need to trap...\n");
      die();
    }
    m->retire_cycle = get_curr_cycle();
    return true;
  }
  virtual void undo(sim_state &machine_state) {
    if(get_dest() > 0) {
      if(m->prev_prf_idx != -1) {
	machine_state.gpr_rat[get_dest()] = m->prev_prf_idx;
      }
      if(m->prf_idx != -1) {
	machine_state.gpr_freevec.clear_bit(m->prf_idx);
	machine_state.gpr_valid.clear_bit(m->prf_idx);
      }
    }
  }
};


class itype_alu_op : public mips_op {
protected:
  itype i_;
public:
  itype_alu_op(sim_op op) :
    mips_op(op), i_(op->inst) {
    this->op_class = mips_op_type::alu;
  }
  virtual int get_dest() const {
    return i_.ii.rt;
  }
  virtual int get_src0() const {
    return i_.ii.rs;
  }
  virtual bool allocate(sim_state &machine_state) {
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
  virtual bool ready(sim_state &machine_state) const {
    if(m->src0_prf != -1 and not(machine_state.gpr_valid.get_bit(m->src0_prf))) {
      return false;
    }
    return true;
  }
  virtual void execute(sim_state &machine_state) {
    uint32_t opcode = (m->inst)>>26;
    uint32_t uimm32 = m->inst & ((1<<16) - 1);
    int16_t simm16 = (int16_t)uimm32;
    int32_t simm32 = (int32_t)simm16;
    switch(opcode)
      {
      case 0x09: /* addiu */
	machine_state.gpr_prf[m->prf_idx] = machine_state.gpr_prf[m->src0_prf] + simm32;
	break;
      case 0x0a: /* slti */
	machine_state.gpr_prf[m->prf_idx] = machine_state.gpr_prf[m->src0_prf] < simm32;
	break;
      case 0x0b: /* sltiu */
      machine_state.gpr_prf[m->prf_idx] = 
      static_cast<uint32_t>(machine_state.gpr_prf[m->src0_prf]) < static_cast<uint32_t>(simm32);
	break;
      case 0x0c: /* andi */
	machine_state.gpr_prf[m->prf_idx] = machine_state.gpr_prf[m->src0_prf] & uimm32;
	break;
      case 0x0d: /* ori */
	machine_state.gpr_prf[m->prf_idx] = machine_state.gpr_prf[m->src0_prf] | uimm32;
	break;
      case 0x0e: /* xori */
	machine_state.gpr_prf[m->prf_idx] = machine_state.gpr_prf[m->src0_prf] ^ uimm32;
	break;
      default:
	dprintf(log_fd, "implement me %x\n", m->pc);
	die();
      }
    m->complete_cycle = get_curr_cycle() + 1;
  }
  
  virtual void complete(sim_state &machine_state) {
    if(not(m->is_complete) and (get_curr_cycle() == m->complete_cycle)) {
      m->is_complete = true;
      machine_state.gpr_valid.set_bit(m->prf_idx);
    }
  }
  virtual bool retire(sim_state &machine_state) {
    if(m->is_complete == false) {
      die();
    }
    machine_state.gpr_freevec.clear_bit(m->prev_prf_idx);
    machine_state.gpr_valid.clear_bit(m->prev_prf_idx);
    retired = true;
    machine_state.icnt++;
    machine_state.arch_grf[get_dest()] = machine_state.gpr_prf[m->prf_idx];
    machine_state.arch_grf_last_pc[get_dest()] = m->pc;
    m->exec_parity = machine_state.gpr_parity();
    m->retire_cycle = get_curr_cycle();
    return true;
  }
  virtual void undo(sim_state &machine_state) {
    if(get_dest() > 0) {
      if(m->prev_prf_idx != -1) {
	machine_state.gpr_rat[get_dest()] = m->prev_prf_idx;
      }
      if(m->prf_idx != -1) {
	machine_state.gpr_freevec.clear_bit(m->prf_idx);
	machine_state.gpr_valid.clear_bit(m->prf_idx);
      }
    }
  }
};


class itype_lui_op : public itype_alu_op {
public:
  itype_lui_op(sim_op op) : itype_alu_op(op) {}
  virtual int get_src0() const {
    return -1;
  }
  virtual void execute(sim_state &machine_state) {
    uint32_t uimm32 = m->inst & ((1<<16) - 1);
    uimm32 <<= 16;
    machine_state.gpr_prf[m->prf_idx] = uimm32;
    m->complete_cycle = get_curr_cycle() + 1;
  }
};


class jump_op : public mips_op {
public:
  enum class jump_type {jalr, jr, j, jal}; 
protected:
  itype i_;
  jump_type jt;
public:
  jump_op(sim_op op, jump_type jt) :
    mips_op(op), i_(op->inst), jt(jt) {
    this->op_class = mips_op_type::jmp;
    op->has_delay_slot = true;
    op->is_branch_or_jump = true;
  }
  virtual int get_dest() const {
    switch(jt)
      {
      case jump_type::jalr:
      case jump_type::jal:
	return 31;
      default:
	break;
      }
    return -1;
  }
  virtual int get_src0() const {
    switch(jt)
      {
      case jump_type::jr:
      case jump_type::jalr:
	return i_.ii.rs;
      default:
	break;
      }
    return -1;
  }
  virtual bool allocate(sim_state &machine_state) {
    if(get_src0() != -1) {
      m->src0_prf = machine_state.gpr_rat[get_src0()];
    }
    if(get_dest() != -1) {
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
  virtual bool ready(sim_state &machine_state) const {
    if(m->src0_prf != -1 and not(machine_state.gpr_valid.get_bit(m->src0_prf))) {
      return false;
    }
    return true;
  }
  virtual void complete(sim_state &machine_state) {
    if(not(m->is_complete) and (get_curr_cycle() == m->complete_cycle)) {
      m->is_complete = true;
      if(m->prf_idx != -1) {
	machine_state.gpr_valid.set_bit(m->prf_idx);
      }
    }
  }
  virtual void execute(sim_state &machine_state) {
    uint32_t pc_mask = (~((1U<<28)-1));
    uint32_t jaddr = (m->inst & ((1<<26)-1)) << 2;
    switch(jt)
      {
      case jump_type::jr:
      case jump_type::jalr:
	m->correct_pc = machine_state.gpr_prf[m->src0_prf];
	break;
      case jump_type::j:
      case jump_type::jal:
	m->correct_pc = ((m->pc + 4)&pc_mask) | jaddr;
	break;
      }
    m->branch_exception = (m->fetch_npc != m->correct_pc);
    
    if(get_dest() != -1) {
      machine_state.gpr_prf[m->prf_idx] = m->pc + 8;
    }
    m->complete_cycle = get_curr_cycle() + 1;
  }
  virtual bool retire(sim_state &machine_state) {
    if(m->prev_prf_idx != -1) {
      machine_state.gpr_freevec.clear_bit(m->prev_prf_idx);
      machine_state.gpr_valid.clear_bit(m->prev_prf_idx);
      machine_state.arch_grf[get_dest()] = machine_state.gpr_prf[m->prf_idx];
      machine_state.arch_grf_last_pc[get_dest()] = m->pc;
    }
    retired = true;
    machine_state.icnt++;
    machine_state.n_jumps++;
    m->exec_parity = machine_state.gpr_parity();

    /* strongly taken */
    branch_target_map[m->pc] = m->correct_pc;
    branch_prediction_map[m->pc] = 3;
    
    machine_state.mispredicted_jumps += m->branch_exception;
    m->retire_cycle = get_curr_cycle();
    return true;
  }
  virtual void undo(sim_state &machine_state) {
    if(get_dest() != -1) {
      if(m->prev_prf_idx != -1) {
	machine_state.gpr_rat[get_dest()] = m->prev_prf_idx;
      }
      if(m->prf_idx != -1) {
	machine_state.gpr_freevec.clear_bit(m->prf_idx);
	machine_state.gpr_valid.clear_bit(m->prf_idx);
      }
    }
  }
};


class branch_op : public mips_op {
public:
  enum class branch_type {beq, bne, blez, bgtz, beql, bnel, blezl, bgtzl, bgez, bgezl, bltz, bltzl,
			  bc1f, bc1t, bc1fl, bc1tl};
protected:
  itype i_;
  branch_type bt;
  bool take_br = false;
  uint32_t branch_target = 0;
  bool is_likely_branch() const {
    switch(bt)
      {
      case branch_type::beql:
      case branch_type::bnel:
      case branch_type::blezl:
      case branch_type::bgtzl:
      case branch_type::bgezl:
      case branch_type::bltzl:
      case branch_type::bc1fl:
      case branch_type::bc1tl:
	return true;
      default:
	break;
      }
    return false;
  }
  bool is_fp_branch() const {
    switch(bt)
      {
      case branch_type::bc1f:
      case branch_type::bc1t:
      case branch_type::bc1fl:
      case branch_type::bc1tl:
	return true;
      default:
	break;
      }
    return false;
  }
  bool gpr_ready(sim_state &machine_state) const {
    if(m->src0_prf != -1 and not(machine_state.gpr_valid[m->src0_prf])) {
      return false;
    }
    if(m->src1_prf != -1 and not(machine_state.gpr_valid[m->src1_prf])) {
      return false;
    }
    return true;
  }
  bool fp_ready(sim_state &machine_state) const {
    return machine_state.fcr1_valid[m->src0_prf];
  }
  bool getConditionCode(uint32_t cr, uint32_t cc) {
    return ((cr & (1U<<cc)) >> cc) & 0x1;
  }
public:
  branch_op(sim_op op, branch_type bt) :
    mips_op(op), bt(bt), i_(op->inst), take_br(false) {
    this->op_class = mips_op_type::jmp;
    int16_t himm = (int16_t)(m->inst & ((1<<16) - 1));
    int32_t imm = ((int32_t)himm) << 2;
    uint32_t npc = m->pc+4;
    branch_target = (imm+npc);
    op->is_branch_or_jump = true;
  }
  virtual int get_src0() const {
    if(is_fp_branch()) {
      return CP1_CR25;
    }
    return i_.ii.rs;
  }
  virtual int get_src1() const {
    switch(bt)
      {
      case branch_type::blez:
      case branch_type::blezl:
      case branch_type::bgtz:
      case branch_type::bgtzl:
      case branch_type::bgez:
      case branch_type::bgezl:
      case branch_type::bltz:
      case branch_type::bltzl:
      case branch_type::bc1f:
      case branch_type::bc1t:
      case branch_type::bc1fl:
      case branch_type::bc1tl:
	return -1;
      default:
	break;
      }
    return i_.ii.rt;
  }
  virtual bool allocate(sim_state &machine_state) {
    if(is_fp_branch()) {
      m->src0_prf = machine_state.fcr1_rat[CP1_CR25];
    }
    else {
      if(get_src0() != -1) {
	m->src0_prf = machine_state.gpr_rat[get_src0()];
      }
      if(get_src1() != -1) {
	m->src1_prf = machine_state.gpr_rat[get_src1()];
      }
    }
    return true;
  }
  virtual bool ready(sim_state &machine_state) const {
    if(is_fp_branch()) {
      return fp_ready(machine_state);
    }
    return gpr_ready(machine_state);
  }
  virtual void execute(sim_state &machine_state) {
    switch(bt)
      {
      case branch_type::beq:
	take_br = machine_state.gpr_prf[m->src0_prf] == machine_state.gpr_prf[m->src1_prf];
	m->has_delay_slot = true;
	break;
      case branch_type::bne:
	take_br = machine_state.gpr_prf[m->src0_prf] != machine_state.gpr_prf[m->src1_prf];
	m->has_delay_slot = true;
	break;
      case branch_type::beql:
	take_br = machine_state.gpr_prf[m->src0_prf] == machine_state.gpr_prf[m->src1_prf];
	m->likely_squash = not(take_br);
	m->has_delay_slot = take_br;
	break;
      case branch_type::bnel:
	take_br = machine_state.gpr_prf[m->src0_prf] != machine_state.gpr_prf[m->src1_prf];
	m->likely_squash = not(take_br);
	m->has_delay_slot = take_br;
	break;
      case branch_type::blez:
	take_br = machine_state.gpr_prf[m->src0_prf] <= 0;
	m->has_delay_slot = true;
	break;
      case branch_type::bgtz:
	take_br = machine_state.gpr_prf[m->src0_prf] > 0;
	m->has_delay_slot = true;
	break;
      case branch_type::bltz:
	take_br = machine_state.gpr_prf[m->src0_prf] < 0;
	m->has_delay_slot = true;
	break;
      case branch_type::bgez:
	take_br = machine_state.gpr_prf[m->src0_prf] >= 0;
	m->has_delay_slot = true;
	break;
      case branch_type::bltzl:
	take_br = machine_state.gpr_prf[m->src0_prf] < 0;
	m->likely_squash = not(take_br);
	m->has_delay_slot = take_br;
	break;
      case branch_type::bgtzl:
	take_br = machine_state.gpr_prf[m->src0_prf] > 0;
	m->likely_squash = not(take_br);
	m->has_delay_slot = take_br;
	break;
      case branch_type::bgezl:
	take_br = machine_state.gpr_prf[m->src0_prf] >= 0;
	m->likely_squash = not(take_br);
	m->has_delay_slot = take_br;
	break;
      case branch_type::blezl:
	take_br = machine_state.gpr_prf[m->src0_prf] <= 0;
	m->has_delay_slot = true;
	m->has_delay_slot = take_br;
	break;

      case branch_type::bc1t:
	take_br = getConditionCode(machine_state.fcr1_prf[m->src0_prf], (m->inst >> 18)&7);
	m->has_delay_slot = true;
	break;
      case branch_type::bc1f:
	take_br = not(getConditionCode(machine_state.fcr1_prf[m->src0_prf], (m->inst >> 18)&7));
	m->has_delay_slot = true;
	break;
      default:
	dprintf(2, "wtf @ %x\n", m->pc);
	die();
      }

    if(take_br) {
      m->correct_pc = branch_target;
    }
    else {
      m->correct_pc = m->pc + 8;
      if(is_likely_branch()) {
	m->branch_exception = true;
      }
    }

    m->branch_exception |= (m->predict_taken != take_br);
    //if(m->predict_taken xor take_br) {
    //dprintf(2, "%x : predicted %d, but branch was %d\n",
    //m->pc, m->predict_taken, take_br);
    //}

    m->complete_cycle = get_curr_cycle() + 1;
  }
  virtual void complete(sim_state &machine_state) {
    if(not(m->is_complete) and (get_curr_cycle() == m->complete_cycle)) {
      m->is_complete = true;
    }
  }
  virtual bool retire(sim_state &machine_state) {
    retired = true;
    machine_state.icnt++;
    machine_state.n_branches++;
    machine_state.mispredicted_branches += m->branch_exception;
    m->exec_parity = machine_state.gpr_parity();

    
    branch_target_map[m->pc] = branch_target;
    if(branch_prediction_map.find(m->pc)==branch_prediction_map.end()) {
      /* initialize as either weakly taken or weakly not-taken */
      branch_prediction_map[m->pc] = take_br ? 2 : 1;
    }
    else {
      int32_t counter = branch_prediction_map.at(m->pc);
      counter = take_br ? counter + 1 : counter - 1;
      counter = std::min(3, counter);
      counter = std::max(0, counter);
      branch_prediction_map.at(m->pc) = counter;
    }
    m->retire_cycle = get_curr_cycle();
    return true;
  }
  virtual void undo(sim_state &machine_state) {}
};


class mips_load : public mips_op {
public:
  enum class load_type {lb,lbu,lh,lhu,lw,ldc1,lwc1,bogus};
protected:
  itype i_;
  load_type lt;
  int32_t imm = -1;
  uint32_t effective_address = ~0;
public:
  mips_load(sim_op op) : mips_op(op), i_(op->inst), lt(load_type::bogus) {
    this->op_class = mips_op_type::load;
    int16_t himm = static_cast<int16_t>(m->inst & ((1<<16) - 1));
    imm = static_cast<int32_t>(himm);
  }
  uint32_t getEA() const {
    return effective_address;
  }
};

class load_op : public mips_load {
public:
  load_op(sim_op op, load_type lt) : mips_load(op) {
    this->lt = lt;
  }
  virtual int get_src0() const {
    return i_.ii.rs;
  }
  virtual int get_dest() const {
    return  i_.ii.rt;
  }
  virtual bool allocate(sim_state &machine_state) {
    m->src0_prf = machine_state.gpr_rat[get_src0()];
    m->prev_prf_idx = machine_state.gpr_rat[get_dest()];
    m->prf_idx = machine_state.gpr_freevec.find_first_unset();
    m->load_tbl_idx = machine_state.load_tbl_freevec.find_first_unset();
    
    if(m->prf_idx == -1 or m->load_tbl_idx == -1) {
      return false;
    }
    machine_state.load_tbl[m->load_tbl_idx] = m;
    machine_state.gpr_freevec.set_bit(m->prf_idx);
    machine_state.load_tbl_freevec.set_bit(m->load_tbl_idx);
    machine_state.gpr_rat[get_dest()] = m->prf_idx;
    machine_state.gpr_valid.clear_bit(m->prf_idx);
    return true;
  }
  virtual bool ready(sim_state &machine_state) const {
    if(not(machine_state.gpr_valid.get_bit(m->src0_prf))) {
      return false;
    }
    
    //serialize
#if 0
    for(size_t i = 0, s = machine_state.store_tbl_freevec.size(); i < s; i++) {
      if(machine_state.store_tbl[i] != nullptr) {
	if((machine_state.store_tbl[i]->alloc_cycle < m->alloc_cycle)) {
	  return false;
	}
      }
    }
#endif
    
#if 1
    auto it = load_alias_map.find(m->pc);
    if(it != load_alias_map.end()) {
      //std::cout << "potential conflict!\n";
      for(size_t i = 0, s = machine_state.store_tbl_freevec.size(); i < s; i++) {
	if(machine_state.store_tbl[i] != nullptr) {
	  auto st = machine_state.store_tbl[i];
	  if(st->pc == it->second and (st->alloc_cycle < m->alloc_cycle)) {
	    //std::cout << "stalled due to inflight store\n";
	    return false;
	  }
	}
      }
    }
#endif
    return true;
  }
  virtual void execute(sim_state &machine_state) {
    effective_address = machine_state.gpr_prf[m->src0_prf] + imm;
    m->complete_cycle = get_curr_cycle() + 1;
  }
  virtual void complete(sim_state &machine_state) {
    if(not(m->is_complete) and (get_curr_cycle() == m->complete_cycle)) {
      m->is_complete = true;
      sparse_mem & mem = *(machine_state.mem);
      switch(lt)
	{
	case load_type::lb:
	  machine_state.gpr_prf[m->prf_idx] = 
	    accessBigEndian(*((int8_t*)(mem + effective_address)));
	  break;
	case load_type::lbu:
	  *reinterpret_cast<uint32_t*>(&machine_state.gpr_prf[m->prf_idx]) = 
	    static_cast<uint32_t>(mem.at(effective_address));
	  break;
	case load_type::lh:
	  machine_state.gpr_prf[m->prf_idx] = 
	    accessBigEndian(*((int16_t*)(mem + effective_address)));
	  break;
	case load_type::lhu:
	  *reinterpret_cast<uint32_t*>(&machine_state.gpr_prf[m->prf_idx]) = 
	    static_cast<uint32_t>(accessBigEndian(*(uint16_t*)(mem + effective_address))); 
	  break;
	case load_type::lw:
	  machine_state.gpr_prf[m->prf_idx] =
	    accessBigEndian(*((int32_t*)(mem + effective_address))); 
	  break;
	default:
	  std::cout << std::hex << m->pc << std::dec << ":" <<
	    getAsmString(m->pc, m->inst) << "\n";
	  die();
	}
      //dprintf(2, "%x : early load ea %x, cycle %llu\n",
      //m->pc, effective_address, get_curr_cycle());
      machine_state.gpr_valid.set_bit(m->prf_idx);
    }
  }
  virtual bool retire(sim_state &machine_state) {
    if(m->load_exception) {
      dprintf(2, "ATTEMPTING TO RETIRE LOAD EXCEPTION @ %x!!\n", m->pc);
      asm("int3");
    }

    machine_state.load_tbl[m->load_tbl_idx] = nullptr;
    machine_state.load_tbl_freevec.clear_bit(m->load_tbl_idx);
    machine_state.gpr_freevec.clear_bit(m->prev_prf_idx);
    machine_state.gpr_valid.clear_bit(m->prev_prf_idx);
    retired = true;
    machine_state.icnt++;

    
    machine_state.arch_grf[get_dest()] = machine_state.gpr_prf[m->prf_idx];
    machine_state.arch_grf_last_pc[get_dest()] = m->pc;
    m->exec_parity = machine_state.gpr_parity();
    m->retire_cycle = get_curr_cycle();
    return true;
  }
  virtual void undo(sim_state &machine_state) {
    machine_state.gpr_rat[get_dest()] = m->prev_prf_idx;
    machine_state.gpr_freevec.clear_bit(m->prf_idx);
    machine_state.gpr_valid.clear_bit(m->prf_idx);
    machine_state.load_tbl_freevec.clear_bit(m->load_tbl_idx);
    machine_state.load_tbl[m->load_tbl_idx] = nullptr;
  }
};

class store_op : public mips_op {
public:
  enum class store_type {sb, sh, sw}; 
protected:
  itype i_;
  store_type st;
  int32_t imm = -1;
  uint32_t effective_address = ~0;
  int32_t store_data = ~0;
public:
  store_op(sim_op op, store_type st) :
    mips_op(op), i_(op->inst), st(st) {
    this->op_class = mips_op_type::store;
    int16_t himm = static_cast<int16_t>(m->inst & ((1<<16) - 1));
    imm = static_cast<int32_t>(himm);
    op->is_store = true;
  }
  virtual int get_src0() const {
    return i_.ii.rt;
  }
  virtual int get_src1() const {
    return i_.ii.rs;
  }
  virtual bool allocate(sim_state &machine_state) {
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
  virtual bool ready(sim_state &machine_state) const {
    if(not(machine_state.gpr_valid.get_bit(m->src0_prf)) or
       not(machine_state.gpr_valid.get_bit(m->src1_prf))) {
      return false;
    }
    return true;
  }
  virtual void execute(sim_state &machine_state) {
    effective_address = machine_state.gpr_prf[m->src1_prf] + imm;
    store_data = machine_state.gpr_prf[m->src0_prf];
    m->complete_cycle = get_curr_cycle() + 1;
  }
  virtual void complete(sim_state &machine_state) {
    if(not(m->is_complete) and (get_curr_cycle() == m->complete_cycle)) {
      m->is_complete = true;
    }
  }
  virtual bool retire(sim_state &machine_state) {
    sparse_mem & mem = *(machine_state.mem);
    switch(st)
      {
      case store_type::sb:
	mem.at(effective_address) = static_cast<int8_t>(store_data);
	break;
      case store_type::sh:
	*((int16_t*)(mem + effective_address)) =
	  accessBigEndian(static_cast<int16_t>(store_data));
	break;
      case store_type::sw:
	*((int32_t*)(mem + effective_address)) = accessBigEndian(store_data);
	break;
      default:
	die();
      }
    retired = true;
    machine_state.icnt++;

    bool load_violation = false;
    for(size_t i = 0; i < machine_state.load_tbl_freevec.size(); i++ ){
      if(machine_state.load_tbl[i]==nullptr) {
	continue;
      }
      mips_meta_op *mmo = machine_state.load_tbl[i];
      auto ld = reinterpret_cast<mips_load*>(mmo->op);
      if(ld == nullptr) {
	die();
      }
      if(mmo->is_complete and (effective_address>>6) == (ld->getEA()>>6)) {
	load_violation = true;
	load_alias_map[mmo->pc] = m->pc;
      }
    }
    //std::cout << "load / store exception, store pc = " << std::hex << m->pc << std::dec << "\n";
    for(size_t i = 0; load_violation and (i < machine_state.load_tbl_freevec.size()); i++ ){
      if(machine_state.load_tbl[i]!=nullptr) {
	machine_state.load_tbl[i]->load_exception = true;
      }
    }
    
    machine_state.store_tbl_freevec.clear_bit(m->store_tbl_idx);
    machine_state.store_tbl[m->store_tbl_idx] = nullptr;
    m->retire_cycle = get_curr_cycle();
    return true;
  }
  virtual void undo(sim_state &machine_state) {
    machine_state.store_tbl_freevec.clear_bit(m->store_tbl_idx);
    machine_state.store_tbl[m->store_tbl_idx] = nullptr;
  }
};


class fp_load_op : public mips_load {
public:
public:
  fp_load_op(sim_op op, load_type lt) : mips_load(op) {
    this->lt = lt;
  }
  virtual int get_src0() const {
    return (m->inst >> 21) & 31;
  }
  virtual int get_dest() const {
    return  (m->inst >> 16) & 31;
  }
  virtual bool allocate(sim_state &machine_state) {
    int num_needed_regs = lt==load_type::ldc1 ? 2 : 1;
    if(machine_state.cpr1_freevec.num_free() < num_needed_regs)
      return false;

    m->load_tbl_idx = machine_state.load_tbl_freevec.find_first_unset();
    if(m->load_tbl_idx == -1) {
      return false;
    }
    
    machine_state.load_tbl_freevec.set_bit(m->load_tbl_idx);
    m->src0_prf = machine_state.gpr_rat[get_src0()];
    m->prev_prf_idx = machine_state.cpr1_rat[get_dest()];
    m->aux_prev_prf_idx = machine_state.cpr1_rat[get_dest()+1];
    
    m->prf_idx = machine_state.cpr1_freevec.find_first_unset();
    machine_state.cpr1_freevec.set_bit(m->prf_idx);
    machine_state.cpr1_rat[get_dest()] = m->prf_idx;
    machine_state.cpr1_valid.clear_bit(m->prf_idx);

    if(lt == load_type::ldc1) {
      m->aux_prf_idx = machine_state.cpr1_freevec.find_first_unset();
      machine_state.cpr1_freevec.set_bit(m->aux_prf_idx);
      machine_state.cpr1_rat[get_dest()+1] = m->aux_prf_idx;
      machine_state.cpr1_valid.clear_bit(m->aux_prf_idx);
    }

    
    return true;
  }
  virtual bool ready(sim_state &machine_state) const {
    return machine_state.gpr_valid.get_bit(m->src0_prf);
  }
  virtual void execute(sim_state &machine_state) {
    effective_address = machine_state.gpr_prf[m->src0_prf] + imm;
    m->complete_cycle = get_curr_cycle() + 1;
  }
  virtual void complete(sim_state &machine_state) {
    if(not(m->is_complete) and (get_curr_cycle() == m->complete_cycle)) {
      m->is_complete = true;
      sparse_mem & mem = *(machine_state.mem);
      switch(lt)
	{
	case load_type::ldc1: {
	  load_thunk<uint64_t> ld(accessBigEndian(*((uint64_t*)(mem + effective_address))));
	  machine_state.cpr1_prf[m->prf_idx] = ld[0];
	  machine_state.cpr1_prf[m->aux_prf_idx] = ld[1];
	  break;
	}
	case load_type::lwc1:
	  machine_state.cpr1_prf[m->prf_idx] = accessBigEndian(*((uint32_t*)(mem + effective_address)));
	  break;
	default:
	  std::cerr << "unimplemented.." << __PRETTY_FUNCTION__ << "\n";
	  die();
	}
    }
  }
  virtual bool retire(sim_state &machine_state) {
    machine_state.load_tbl[m->load_tbl_idx] = nullptr;
    machine_state.load_tbl_freevec.clear_bit(m->load_tbl_idx);
    machine_state.cpr1_valid.set_bit(m->prf_idx);
    machine_state.cpr1_freevec.clear_bit(m->prev_prf_idx);
    machine_state.cpr1_valid.clear_bit(m->prev_prf_idx);

    retired = true;
    machine_state.icnt++;
    machine_state.arch_cpr1[get_dest()] = machine_state.cpr1_prf[m->prf_idx];
    machine_state.arch_cpr1_last_pc[get_dest()] = m->pc;
    if(m->aux_prf_idx != -1) {
      machine_state.cpr1_valid.set_bit(m->aux_prf_idx);
      machine_state.cpr1_freevec.clear_bit(m->aux_prev_prf_idx);
      machine_state.cpr1_valid.clear_bit(m->aux_prev_prf_idx);
      machine_state.arch_cpr1[get_dest()+1] = machine_state.cpr1_prf[m->aux_prf_idx];
      machine_state.arch_cpr1_last_pc[get_dest()+1] = m->pc;
    }
    m->retire_cycle = get_curr_cycle();
    return true;
  }
  virtual void undo(sim_state &machine_state) {
    machine_state.load_tbl[m->load_tbl_idx] = nullptr;
    machine_state.load_tbl_freevec.clear_bit(m->load_tbl_idx);
    machine_state.cpr1_rat[get_dest()] = m->prev_prf_idx;
    machine_state.cpr1_freevec.clear_bit(m->prf_idx);
    machine_state.cpr1_valid.clear_bit(m->prf_idx);
    if(m->aux_prf_idx != -1) {
      machine_state.cpr1_rat[get_dest()+1] = m->aux_prev_prf_idx;
      machine_state.cpr1_freevec.clear_bit(m->aux_prf_idx);
      machine_state.cpr1_valid.clear_bit(m->aux_prf_idx);
    }
  }
};

class fp_store_op : public mips_op {
public:
  enum class store_type {sdc1, swc1}; 
protected:
  itype i_;
  store_type st;
  int32_t imm = -1;
  uint32_t effective_address = ~0;
  uint32_t store_data[2] = {0};
public:
  fp_store_op(sim_op op, store_type st) :
    mips_op(op), i_(op->inst), st(st) {
    this->op_class = mips_op_type::store;
    int16_t himm = static_cast<int16_t>(m->inst & ((1<<16) - 1));
    imm = static_cast<int32_t>(himm);
    op->is_store = true;
    op->is_fp_store = true;
  }
  virtual int get_src0() const {
    return (m->inst >> 16) & 31; /* cpr1 reg */
  }
  virtual int get_src1() const {
    return (m->inst >> 21) & 31; /* gpr reg */
  }
  virtual bool allocate(sim_state &machine_state) {
    m->src0_prf = machine_state.cpr1_rat[get_src0()];
    m->src1_prf = machine_state.gpr_rat[get_src1()];
    if(st == store_type::sdc1) {
      m->src2_prf = machine_state.cpr1_rat[get_src0()+1];
    }
    return true;
  }
  virtual bool ready(sim_state &machine_state) const {
    if(not(machine_state.cpr1_valid.get_bit(m->src0_prf)) or
       not(machine_state.gpr_valid.get_bit(m->src1_prf))) {
      return false;
    }
    if(m->src2_prf != -1 and not(machine_state.cpr1_valid.get_bit(m->src2_prf))) {
      return false;
    }
    return true;
  }
  virtual void execute(sim_state &machine_state) {
    effective_address = machine_state.gpr_prf[m->src1_prf] + imm;
    switch(st)
      {
      case store_type::sdc1:
	store_data[0] = *reinterpret_cast<uint64_t*>(&machine_state.cpr1_prf[m->src0_prf]);
	store_data[1] = *reinterpret_cast<uint64_t*>(&machine_state.cpr1_prf[m->src2_prf]);
	break;
      case store_type::swc1:
	store_data[0] = *reinterpret_cast<uint32_t*>(&machine_state.cpr1_prf[m->src0_prf]);
	break;
      }
    
    m->complete_cycle = get_curr_cycle() + 1;
  }
  virtual void complete(sim_state &machine_state) {
    if(not(m->is_complete) and (get_curr_cycle() == m->complete_cycle)) {
      m->is_complete = true;
    }
  }
  virtual bool retire(sim_state &machine_state) {

      
    sparse_mem & mem = *(machine_state.mem);
    switch(st)
      {
      case store_type::swc1:
	*((uint32_t*)(mem + effective_address)) = accessBigEndian(store_data[0]);
	break;
      case store_type::sdc1: {
	load_thunk<uint64_t> ld;
	ld[0] = store_data[0];
	ld[1] = store_data[1];
	*reinterpret_cast<uint64_t*>(mem + effective_address) = accessBigEndian(ld.DT());
	break;
      }
      default:
	die();
      }

    bool load_violation = false;
    for(size_t i = 0; i < machine_state.load_tbl_freevec.size(); i++ ){
      if(machine_state.load_tbl[i]==nullptr) {
	continue;
      }
      mips_meta_op *mmo = machine_state.load_tbl[i];
      auto ld = reinterpret_cast<mips_load*>(mmo->op);
      if(ld == nullptr) {
	die();
      }
      if(mmo->is_complete and ((effective_address>>3) == (ld->getEA()>>3))) {
	load_violation = true;
      }
    }
    for(size_t i = 0; load_violation and (i < machine_state.load_tbl_freevec.size()); i++ ){
      if(machine_state.load_tbl[i]!=nullptr) {
	machine_state.load_tbl[i]->load_exception = true;
      }
    }
    
    retired = true;
    m->retire_cycle = get_curr_cycle();
    machine_state.icnt++;
    return true;
  }
  virtual void undo(sim_state &machine_state) {}
};


class mul_op : public mips_op {
public:
  mul_op(sim_op op) : mips_op(op) {
    this->op_class = mips_op_type::alu;
  }
  virtual int get_dest() const {
    return (m->inst >> 11) & 31;
  }
  virtual int get_src0() const {
    return (m->inst >> 21) & 31;
  }
  virtual int get_src1() const {
    return (m->inst >> 16) & 31;
  }
  virtual bool allocate(sim_state &machine_state) {
    m->src0_prf = machine_state.gpr_rat[get_src0()];
    m->src1_prf = machine_state.gpr_rat[get_src1()];
    m->prev_prf_idx = machine_state.gpr_rat[get_dest()];
    int64_t prf_id = machine_state.gpr_freevec.find_first_unset();
    if(prf_id == -1)
      return false;
    machine_state.gpr_freevec.set_bit(prf_id);
    machine_state.gpr_rat[get_dest()] = prf_id;
    m->prf_idx = prf_id;
    machine_state.gpr_valid.clear_bit(prf_id);
    return true;
  }
  virtual bool ready(sim_state &machine_state) const {
    if(not(machine_state.gpr_valid.get_bit(m->src0_prf))) {
      return false;
    }
    if(not(machine_state.gpr_valid.get_bit(m->src1_prf))) {
      return false;
    }
    return true;
  }
  virtual void execute(sim_state &machine_state) {
    int64_t a = static_cast<int64_t>(machine_state.gpr_prf[m->src1_prf]);
    int64_t b = static_cast<int64_t>(machine_state.gpr_prf[m->src0_prf]);
    int64_t y = a*b;
    machine_state.gpr_prf[m->prf_idx] = static_cast<int32_t>(y);
    m->complete_cycle = get_curr_cycle() + 4;
  }
  virtual void complete(sim_state &machine_state) {
    if(not(m->is_complete) and (get_curr_cycle() == m->complete_cycle)) {
      m->is_complete = true;
      machine_state.gpr_valid.set_bit(m->prf_idx);
    }
  }
  virtual bool retire(sim_state &machine_state) {
    machine_state.gpr_freevec.clear_bit(m->prev_prf_idx);
    machine_state.gpr_valid.clear_bit(m->prev_prf_idx);
    machine_state.arch_grf[get_dest()] = machine_state.gpr_prf[m->prf_idx];
    machine_state.arch_grf_last_pc[get_dest()] = m->pc;
    m->exec_parity = machine_state.gpr_parity();
    retired = true;
    machine_state.icnt++;
    m->retire_cycle = get_curr_cycle();
    return true;
  }
  virtual void undo(sim_state &machine_state) {
    machine_state.gpr_rat[get_dest()] = m->prev_prf_idx;
    machine_state.gpr_freevec.clear_bit(m->prf_idx);
    machine_state.gpr_valid.clear_bit(m->prf_idx);
  }
};

class clz_op : public mips_op {
public:
  clz_op(sim_op op) : mips_op(op) {
    this->op_class = mips_op_type::alu;
  }
  virtual int get_dest() const {
    return (m->inst >> 11) & 31;
  }
  virtual int get_src0() const {
    return (m->inst >> 21) & 31;
  }
  virtual bool allocate(sim_state &machine_state) {
    m->src0_prf = machine_state.gpr_rat[get_src0()];
    m->prev_prf_idx = machine_state.gpr_rat[get_dest()];
    int64_t prf_id = machine_state.gpr_freevec.find_first_unset();
    if(prf_id == -1)
      return false;
    machine_state.gpr_freevec.set_bit(prf_id);
    machine_state.gpr_rat[get_dest()] = prf_id;
    m->prf_idx = prf_id;
    machine_state.gpr_valid.clear_bit(prf_id);
    return true;
  }
  virtual bool ready(sim_state &machine_state) const {
    if(not(machine_state.gpr_valid.get_bit(m->src0_prf))) {
      return false;
    }
    return true;
  }
  virtual void execute(sim_state &machine_state) {
    if(machine_state.gpr_prf[m->src0_prf]==0) {
      machine_state.gpr_prf[m->prf_idx] = 32;
    }
    else {
      machine_state.gpr_prf[m->prf_idx] = __builtin_clz(machine_state.gpr_prf[m->src0_prf]);
    }
    m->complete_cycle = get_curr_cycle() + 1;
  }
  virtual void complete(sim_state &machine_state) {
    if(not(m->is_complete) and (get_curr_cycle() == m->complete_cycle)) {
      m->is_complete = true;
      machine_state.gpr_valid.set_bit(m->prf_idx);
    }
  }
  virtual bool retire(sim_state &machine_state) {
    machine_state.gpr_freevec.clear_bit(m->prev_prf_idx);
    machine_state.gpr_valid.clear_bit(m->prev_prf_idx);
    machine_state.arch_grf[get_dest()] = machine_state.gpr_prf[m->prf_idx];
    machine_state.arch_grf_last_pc[get_dest()] = m->pc;
    m->exec_parity = machine_state.gpr_parity();
    retired = true;
    machine_state.icnt++;
    m->retire_cycle = get_curr_cycle();
    return true;
  }
  virtual void undo(sim_state &machine_state) {
    machine_state.gpr_rat[get_dest()] = m->prev_prf_idx;
    machine_state.gpr_freevec.clear_bit(m->prf_idx);
    machine_state.gpr_valid.clear_bit(m->prf_idx);
  }
};


class mult_div_op : public mips_op {
public:
  enum class mult_div_types {mult, multu, div, divu, madd, maddu};
protected:
  mult_div_types mdt;
public:
  mult_div_op(sim_op op, mult_div_types mdt) : mips_op(op), mdt(mdt) {
    this->op_class = mips_op_type::alu;
  }
  virtual int get_src0() const {
    /* rs */
    return (m->inst >> 21) & 31;
  }
  virtual int get_src1() const {
    /* rt */
    return (m->inst >> 16) & 31;
  }
  virtual int get_src2() const {
    switch(mdt)
      {
      case mult_div_types::madd:
      case mult_div_types::maddu:
	return 32;
      default:
	break;
      }
    return -1;
  }
  virtual int get_src3() const {
    switch(mdt)
      {
      case mult_div_types::madd:
      case mult_div_types::maddu:
	return 33;
      default:
	break;
      }
    return -1;
  }
  virtual bool allocate(sim_state &machine_state) {
    if(machine_state.gpr_freevec.num_free() < 2)
      return false;

    m->src0_prf = machine_state.gpr_rat[get_src0()];
    m->src1_prf = machine_state.gpr_rat[get_src1()];
    if(get_src2() != -1) {
      m->src2_prf = machine_state.gpr_rat[get_src2()];
    }
    if(get_src3() != -1) {
      m->src3_prf = machine_state.gpr_rat[get_src3()];
    }
    m->prev_lo_prf_idx = machine_state.gpr_rat[32];
    m->prev_hi_prf_idx = machine_state.gpr_rat[33];

    m->lo_prf_idx = machine_state.gpr_freevec.find_first_unset();
    if(m->lo_prf_idx==-1) {
      dprintf(log_fd,"terminal allocation error @ %s:%d\n", __PRETTY_FUNCTION__, __LINE__);
      die();
    }
    machine_state.gpr_freevec.set_bit(m->lo_prf_idx);
    m->hi_prf_idx = machine_state.gpr_freevec.find_first_unset();
    if(m->hi_prf_idx==-1) {
      dprintf(log_fd,"terminal allocation error @ %s:%d\n", __PRETTY_FUNCTION__, __LINE__);
      die();
    }
    machine_state.gpr_freevec.set_bit(m->hi_prf_idx);


    machine_state.gpr_rat[32] = m->lo_prf_idx;
    machine_state.gpr_rat[33] = m->hi_prf_idx;

    machine_state.gpr_valid.clear_bit(m->lo_prf_idx);
    machine_state.gpr_valid.clear_bit(m->hi_prf_idx);
    return true;
  }
  virtual bool ready(sim_state &machine_state) const {
    if(not(machine_state.gpr_valid.get_bit(m->src0_prf)) or
       not(machine_state.gpr_valid.get_bit(m->src1_prf))) {
      return false;
    }
    if(m->src2_prf != -1 and not(machine_state.gpr_valid.get_bit(m->src2_prf))) {
      return false;
    }
    if(m->src3_prf != -1 and not(machine_state.gpr_valid.get_bit(m->src3_prf))) {
      return false;
    }
    return true;
  }
  virtual void execute(sim_state &machine_state) {
    int latency = 4;
    uint32_t *lo = reinterpret_cast<uint32_t*>(&machine_state.gpr_prf[m->lo_prf_idx]);
    uint32_t *hi = reinterpret_cast<uint32_t*>(&machine_state.gpr_prf[m->hi_prf_idx]);
    
    switch(mdt)
      {
      case mult_div_types::mult: {
	int64_t y = static_cast<int64_t>(machine_state.gpr_prf[m->src1_prf]) *
	  static_cast<int64_t>(machine_state.gpr_prf[m->src0_prf]);
	*reinterpret_cast<int32_t*>(lo) = static_cast<int32_t>(y & 0xffffffff);
	*reinterpret_cast<int32_t*>(hi) = static_cast<int32_t>(y >> 32);
	break;
      }
      case mult_div_types::madd: {
	int64_t acc = static_cast<int64_t>(machine_state.gpr_prf[m->src3_prf]) << 32;
	acc |= static_cast<int64_t>(machine_state.gpr_prf[m->src2_prf]);
	int64_t y = static_cast<int64_t>(machine_state.gpr_prf[m->src1_prf]) *
	  static_cast<int64_t>(machine_state.gpr_prf[m->src0_prf]);
	y += acc;
	*reinterpret_cast<int32_t*>(lo) = static_cast<int32_t>(y & 0xffffffff);
	*reinterpret_cast<int32_t*>(hi) = static_cast<int32_t>(y >> 32);
	break;
      }

      case mult_div_types::multu: {
	uint64_t u_a64 = static_cast<uint64_t>(*reinterpret_cast<uint32_t*>(&machine_state.gpr_prf[m->src1_prf]));
	uint64_t u_b64 = static_cast<uint64_t>(*reinterpret_cast<uint32_t*>(&machine_state.gpr_prf[m->src0_prf]));
	uint64_t y = u_a64*u_b64;
	*lo = static_cast<uint32_t>(y);
	*hi = static_cast<uint32_t>(y>>32);
	break;
      }
      case mult_div_types::divu: {
	if(machine_state.gpr_prf[m->src1_prf] != 0) {
	  *lo = (uint32_t)machine_state.gpr_prf[m->src0_prf] /
	    (uint32_t)machine_state.gpr_prf[m->src1_prf];
	  *hi = (uint32_t)machine_state.gpr_prf[m->src0_prf] %
	    (uint32_t)machine_state.gpr_prf[m->src1_prf];
	}
	latency = 32;
	break;
      }
	
      default:
	dprintf(2, "implement me @ %s\n", __PRETTY_FUNCTION__);
	die();
      }
    
    m->complete_cycle = get_curr_cycle() + latency;
  }
  virtual void complete(sim_state &machine_state) {
    if(not(m->is_complete) and (get_curr_cycle() == m->complete_cycle)) {
      m->is_complete = true;
      machine_state.gpr_valid.set_bit(m->lo_prf_idx);
      machine_state.gpr_valid.set_bit(m->hi_prf_idx);
    }
  }
  virtual bool retire(sim_state &machine_state) {
    machine_state.gpr_freevec.clear_bit(m->prev_lo_prf_idx);
    machine_state.gpr_freevec.clear_bit(m->prev_hi_prf_idx);
    machine_state.gpr_valid.clear_bit(m->prev_lo_prf_idx);
    machine_state.gpr_valid.clear_bit(m->prev_hi_prf_idx);
    retired = true;
    machine_state.icnt++;
    machine_state.arch_grf[32] = machine_state.gpr_prf[m->lo_prf_idx];
    machine_state.arch_grf_last_pc[32] = m->pc;
    machine_state.arch_grf[33] = machine_state.gpr_prf[m->hi_prf_idx];
    machine_state.arch_grf_last_pc[33] = m->pc;
    m->retire_cycle = get_curr_cycle();
    return true;
  }
  virtual void undo(sim_state &machine_state) {
    machine_state.gpr_rat[32] = m->prev_lo_prf_idx;
    machine_state.gpr_rat[33] = m->prev_hi_prf_idx;
    machine_state.gpr_freevec.clear_bit(m->lo_prf_idx);
    machine_state.gpr_freevec.clear_bit(m->hi_prf_idx);
    machine_state.gpr_valid.clear_bit(m->lo_prf_idx);
    machine_state.gpr_valid.clear_bit(m->hi_prf_idx);
  }
};


class se_op : public mips_op {
public:
  enum class se_type {seb, seh};
protected:
  se_type st;
public:
  se_op(sim_op op, se_type st) : mips_op(op), st(st) {
    this->op_class = mips_op_type::system;
  }
  virtual int get_src0() const {
    return (m->inst >> 16) & 31;
  }
  virtual int get_dest() const {
    return (m->inst >> 11) & 31; 
  }
  virtual bool allocate(sim_state &machine_state) {
    m->src0_prf = machine_state.gpr_rat[get_src0()];
    m->prf_idx = machine_state.gpr_freevec.find_first_unset();
    if(m->prf_idx == -1)
      return false;
    m->prev_prf_idx = machine_state.gpr_rat[get_dest()];
    machine_state.gpr_freevec.set_bit(m->prf_idx);
    machine_state.gpr_rat[get_dest()] = m->prf_idx;
    machine_state.gpr_valid.clear_bit(m->prf_idx);
    return true;
  }
  virtual bool ready(sim_state &machine_state) const {
    return machine_state.gpr_valid.get_bit(m->src0_prf);
  }
  virtual void execute(sim_state &machine_state) {
    switch(st)
      {
      case se_type::seb:
	machine_state.gpr_prf[m->prf_idx] = static_cast<int32_t>(static_cast<int8_t>(machine_state.gpr_prf[m->src0_prf])); 
	break;
      case se_type::seh:
	machine_state.gpr_prf[m->prf_idx] = static_cast<int32_t>(static_cast<int16_t>(machine_state.gpr_prf[m->src0_prf])); 
	break;
      default:
	die();
      }
    m->complete_cycle = get_curr_cycle() + 1;
  }
  virtual void complete(sim_state &machine_state) {
    if(not(m->is_complete) and (get_curr_cycle() == m->complete_cycle)) {
      m->is_complete = true;
      machine_state.gpr_valid.set_bit(m->prf_idx);
    }
  }
  virtual bool retire(sim_state &machine_state) {
    machine_state.gpr_freevec.clear_bit(m->prev_prf_idx);
    machine_state.gpr_valid.clear_bit(m->prev_prf_idx);
    machine_state.icnt++;
    machine_state.arch_grf[get_dest()] = machine_state.gpr_prf[m->prf_idx];
    machine_state.arch_grf_last_pc[get_dest()] = m->pc;
    m->exec_parity = machine_state.gpr_parity();
    m->retire_cycle = get_curr_cycle();
    return true;
  }
  virtual void undo(sim_state &machine_state) {
    machine_state.gpr_rat[get_dest()] = m->prev_prf_idx;
    machine_state.gpr_freevec.clear_bit(m->prf_idx);
    machine_state.gpr_valid.clear_bit(m->prf_idx);
  }
};

class ext_op : public mips_op {
public:
  ext_op(sim_op op) : mips_op(op) {
    this->op_class = mips_op_type::alu;
  }
  virtual int get_src0() const {
    return (m->inst >> 21) & 31;
  }
  virtual int get_dest() const {
    return (m->inst >> 16) & 31; 
  }
  virtual bool allocate(sim_state &machine_state) {
    m->src0_prf = machine_state.gpr_rat[get_src0()];
    m->prf_idx = machine_state.gpr_freevec.find_first_unset();
    if(m->prf_idx == -1)
      return false;
    m->prev_prf_idx = machine_state.gpr_rat[get_dest()];
    machine_state.gpr_freevec.set_bit(m->prf_idx);
    machine_state.gpr_rat[get_dest()] = m->prf_idx;
    machine_state.gpr_valid.clear_bit(m->prf_idx);
    return true;
  }
  virtual bool ready(sim_state &machine_state) const {
    return machine_state.gpr_valid.get_bit(m->src0_prf);
  }
  virtual void execute(sim_state &machine_state) {
    uint32_t pos = (m->inst >> 6) & 31;
    uint32_t size = ((m->inst >> 11) & 31) + 1;
    machine_state.gpr_prf[m->prf_idx] = (machine_state.gpr_prf[m->src0_prf] >> pos) &
      ((1<<size)-1);
    m->complete_cycle = get_curr_cycle() + 1;
  }
  virtual void complete(sim_state &machine_state) {
    if(not(m->is_complete) and (get_curr_cycle() == m->complete_cycle)) {
      m->is_complete = true;
      machine_state.gpr_valid.set_bit(m->prf_idx);
    }
  }
  virtual bool retire(sim_state &machine_state) {
    machine_state.gpr_freevec.clear_bit(m->prev_prf_idx);
    machine_state.gpr_valid.clear_bit(m->prev_prf_idx);
    machine_state.icnt++;
    machine_state.arch_grf[get_dest()] = machine_state.gpr_prf[m->prf_idx];
    machine_state.arch_grf_last_pc[get_dest()] = m->pc;
    m->exec_parity = machine_state.gpr_parity();
    m->retire_cycle = get_curr_cycle();
    return true;
  }
  virtual void undo(sim_state &machine_state) {
    machine_state.gpr_rat[get_dest()] = m->prev_prf_idx;
    machine_state.gpr_freevec.clear_bit(m->prf_idx);
    machine_state.gpr_valid.clear_bit(m->prf_idx);
  }
};


class movci_op : public mips_op {
protected:
  bool getConditionCode(uint32_t cr, uint32_t cc) {
    return ((cr & (1U<<cc)) >> cc) & 0x1;
  }
public:
  movci_op(sim_op op) : mips_op(op) {
    this->op_class = mips_op_type::alu;
  }
  virtual int get_dest() const {
    return (m->inst >> 11) & 31; 
  }
  virtual int get_src0() const {
    return (m->inst >> 11) & 31; /* previous value of dest */
  }
  virtual int get_src1() const {
    return (m->inst >> 21) & 31; /* other register */
  }
  virtual bool allocate(sim_state &machine_state) {
    m->src0_prf = machine_state.gpr_rat[get_src0()];
    m->src1_prf = machine_state.gpr_rat[get_src1()];
    m->src2_prf = machine_state.fcr1_rat[CP1_CR25];
    m->prf_idx = machine_state.gpr_freevec.find_first_unset();
    if(m->prf_idx == -1)
      return false;
    m->prev_prf_idx = machine_state.gpr_rat[get_dest()];
    machine_state.gpr_freevec.set_bit(m->prf_idx);
    machine_state.gpr_rat[get_dest()] = m->prf_idx;
    machine_state.gpr_valid.clear_bit(m->prf_idx);
    return true;
  }
  virtual bool ready(sim_state &machine_state) const {
    if(not(machine_state.gpr_valid.get_bit(m->src0_prf)))
      return false;
    if(not(machine_state.gpr_valid.get_bit(m->src1_prf)))
      return false;
    if(not(machine_state.fcr1_valid.get_bit(m->src2_prf)))
      return false;
    return true;
  }
  virtual void execute(sim_state &machine_state) {
    uint32_t cc = (m->inst >> 18) & 7;
    uint32_t tf = (m->inst>>16) & 1;
    int32_t r = 0;
    bool z = getConditionCode(machine_state.fcr1_prf[m->src2_prf], cc);
    if(tf==0) {
      r = z ? machine_state.gpr_prf[m->src0_prf] : machine_state.gpr_prf[m->src1_prf];
    }
    else {
      r = z ? machine_state.gpr_prf[m->src1_prf] : machine_state.gpr_prf[m->src0_prf];
    }
    machine_state.gpr_prf[m->prf_idx] = r;
    m->complete_cycle = get_curr_cycle() + 1;
  }
  virtual void complete(sim_state &machine_state) {
    if(not(m->is_complete) and (get_curr_cycle() == m->complete_cycle)) {
      m->is_complete = true;
      machine_state.gpr_valid.set_bit(m->prf_idx);
    }
  }
  virtual bool retire(sim_state &machine_state) {
    machine_state.gpr_freevec.clear_bit(m->prev_prf_idx);
    machine_state.gpr_valid.clear_bit(m->prev_prf_idx);
    machine_state.icnt++;
    machine_state.arch_grf[get_dest()] = machine_state.gpr_prf[m->prf_idx];
    machine_state.arch_grf_last_pc[get_dest()] = m->pc;
    m->exec_parity = machine_state.gpr_parity();
    m->retire_cycle = get_curr_cycle();
    return true;
  }
  virtual void undo(sim_state &machine_state) {
    machine_state.gpr_rat[get_dest()] = m->prev_prf_idx;
    machine_state.gpr_freevec.clear_bit(m->prf_idx);
    machine_state.gpr_valid.clear_bit(m->prf_idx);
  }
};

class fp_cmp : public mips_op {
protected:
  uint32_t fmt;
  template <typename T>
  struct fp_cmp_functor {
    uint32_t compare(int cond, const T &x, const T &y) const {
      switch(cond)
	{
	case COND_UN:
	  return (x == y);
	case COND_EQ:
	  return (x == y);
	case COND_LT:
	  return (x < y);	  
	case COND_LE:
	  return (x <= y);
	default:
	  die();
	  break;
	}
      return false;
    }
  };
  uint32_t setCC(uint32_t old_cr, uint32_t v, uint32_t cc) const {
    uint32_t m0 = 1U<<cc;
    uint32_t m1 = ~m0;
    uint32_t m2 = ~(v-1);
    return (old_cr & m1) | ((1U<<cc) & m2);
  }
  void execute_double(sim_state &machine_state) {
    load_thunk<double> src0, src1;
    fp_cmp_functor<double> cmp;
    src0[0] = machine_state.cpr1_prf[m->src0_prf];
    src0[1] = machine_state.cpr1_prf[m->src2_prf];
    src1[0] = machine_state.cpr1_prf[m->src1_prf];
    src1[1] = machine_state.cpr1_prf[m->src3_prf];
    uint32_t v = cmp.compare(m->inst & 15, src0.DT(), src1.DT());
    machine_state.fcr1_prf[m->prf_idx] =
      setCC(machine_state.fcr1_prf[m->src4_prf], v, (m->inst >> 8) & 7);
  }
  void execute_float(sim_state &machine_state) {
    load_thunk<float> src0, src1;
    fp_cmp_functor<float> cmp;
    src0[0] = machine_state.cpr1_prf[m->src0_prf];
    src1[0] = machine_state.cpr1_prf[m->src1_prf];
    cmp.compare(m->inst & 15, src0.DT(), src1.DT());
    uint32_t v = machine_state.fcr1_prf[m->src4_prf];
    machine_state.fcr1_prf[m->prf_idx] =
      setCC(machine_state.fcr1_prf[m->src4_prf], v, (m->inst >> 8) & 7);
  }
public:
  fp_cmp(sim_op op) : mips_op(op), fmt((op->inst >> 21) & 31) {
    this->op_class = mips_op_type::fp; 
  }
  virtual int get_src0() const {
    return (m->inst >> 11) & 31; /* fs */
  }
  virtual int get_src1() const {
    return (m->inst >> 16) & 31; /* ft */
  }
  virtual bool allocate(sim_state &machine_state) {
    m->src0_prf = machine_state.cpr1_rat[get_src0()];
    m->src1_prf = machine_state.cpr1_rat[get_src1()];
    m->src4_prf = machine_state.fcr1_rat[CP1_CR25];
    
    if(fmt == FMT_D) {
      m->src2_prf = machine_state.cpr1_rat[get_src0()+1];
      m->src3_prf = machine_state.cpr1_rat[get_src1()+1];
    }
    
    m->prev_prf_idx = m->src4_prf;
    m->prf_idx = machine_state.fcr1_freevec.find_first_unset();
    if(m->prf_idx==-1)
      return false;
        
    machine_state.fcr1_freevec.set_bit(m->prf_idx);
    machine_state.fcr1_valid.clear_bit(m->prf_idx);
    machine_state.fcr1_rat[CP1_CR25] = m->prf_idx;

    return true;
  }
  virtual bool ready(sim_state &machine_state) const {
    if(not(machine_state.cpr1_valid[m->src0_prf])) {
      return false;
    }
    if(not(machine_state.cpr1_valid[m->src1_prf])) {
      return false;
    }
    if(not(machine_state.fcr1_valid[m->src4_prf])) {
      return false;
    }
    if(m->src2_prf != -1 and not(machine_state.cpr1_valid[m->src2_prf])) {
      return false;
    }
    if(m->src3_prf != -1 and not(machine_state.cpr1_valid[m->src3_prf])) {
      std::cout << __LINE__ << "\n";
      return false;
    }
    return true;
  }
  virtual void complete(sim_state &machine_state) {
    if(not(m->is_complete) and (get_curr_cycle() == m->complete_cycle)) {
      m->is_complete = true;
      machine_state.fcr1_valid.set_bit(m->prf_idx);
    }
  }
  virtual void execute(sim_state &machine_state) {
    switch(fmt)
      {
      case FMT_S:
	execute_float(machine_state);
	break;
      case FMT_D:
	execute_double(machine_state);
	break;
      default:
	die();
      }
    m->complete_cycle = get_curr_cycle() + 1;
  }
  virtual bool retire(sim_state &machine_state) {
    machine_state.fcr1_freevec.clear_bit(m->prev_prf_idx);
    machine_state.fcr1_valid.clear_bit(m->prev_prf_idx);
    retired = true;
    machine_state.icnt++;
    m->retire_cycle = get_curr_cycle();
    return true;
  }
  virtual void undo(sim_state &machine_state) {
    machine_state.fcr1_rat[CP1_CR25] = m->prev_prf_idx;
    machine_state.fcr1_freevec.clear_bit(m->prf_idx);
    machine_state.fcr1_valid.clear_bit(m->prf_idx);
  }

};

class fp_arith_op : public mips_op {
public:
  enum class fp_op_type {add, sub, mul, div, sqrt, abs, mov, neg, recip, rsqrt};
protected:
  uint32_t fmt;
  fp_op_type fot;
  bool allocate_double(sim_state &machine_state) {
    m->prev_prf_idx = machine_state.cpr1_rat[get_dest()];
    m->aux_prev_prf_idx = machine_state.cpr1_rat[get_dest()+1];
    m->src0_prf = machine_state.cpr1_rat[get_src0()];
    m->src1_prf = machine_state.cpr1_rat[get_src0()+1];
    if(get_src1()!=-1) {
      m->src2_prf = machine_state.cpr1_rat[get_src1()];
      m->src3_prf = machine_state.cpr1_rat[get_src1()+1];
    }
    if(machine_state.cpr1_freevec.num_free() < 2)
      return false;
    m->prf_idx = machine_state.cpr1_freevec.find_first_unset();
    machine_state.cpr1_freevec.set_bit(m->prf_idx);
    m->aux_prf_idx = machine_state.cpr1_freevec.find_first_unset();
    machine_state.cpr1_freevec.set_bit(m->aux_prf_idx);
    
    machine_state.cpr1_valid.clear_bit(m->prf_idx);
    machine_state.cpr1_valid.clear_bit(m->aux_prf_idx);

    machine_state.cpr1_rat[get_dest()] = m->prf_idx;
    machine_state.cpr1_rat[get_dest()+1] = m->aux_prf_idx;
    return true;
  }
  bool allocate_float(sim_state &machine_state) {
    m->prev_prf_idx = machine_state.cpr1_rat[get_dest()];
    m->src0_prf = machine_state.cpr1_rat[get_src0()];
    if(get_src1()!=-1) {
      m->src1_prf = machine_state.cpr1_rat[get_src1()];
    }
    assert(get_src1() != -1);
    assert(machine_state.cpr1_rat[get_src1()] != -1);
    assert(m->src1_prf != -1);
    if(machine_state.cpr1_freevec.num_free() < 1)
      return false;
    m->prf_idx = machine_state.cpr1_freevec.find_first_unset();
    machine_state.cpr1_freevec.set_bit(m->prf_idx);
    machine_state.cpr1_valid.clear_bit(m->prf_idx);
    machine_state.cpr1_rat[get_dest()] = m->prf_idx;
    return true;
  }
  template <typename T>
  struct fp_executor {
    T operator()(fp_op_type fot,
		 int &latency,
		 const T src0,
		 const T src1) const {
      T dest = static_cast<T>(0);
      switch(fot)
	{
	case fp_op_type::add:
	  dest = src0+src1;
	  latency = 2;
	  break;
	case fp_op_type::sub:
	  dest = src0-src1;
	  latency = 2;
	  break;
	case fp_op_type::mul:
	  dest = src0*src1;
	  latency = 3;
	  break;
	case fp_op_type::div:
	  dest = src0/src1;
	  latency = 32;
	  break;
	case fp_op_type::mov:
	  dest = src0;
	  latency = 1;
	  break;
	default:
	  die();
	  break;
	}
      return dest;
    }
  };
  void execute_double(sim_state &machine_state) {
    int latency = 1;
    load_thunk<double> src0, src1, dest;
    src0[0] = machine_state.cpr1_prf[m->src0_prf];
    src0[1] = machine_state.cpr1_prf[m->src1_prf];
    if(get_src1() != -1) {
      src1[0] = machine_state.cpr1_prf[m->src2_prf];
      src1[1] = machine_state.cpr1_prf[m->src3_prf];
    }
    fp_executor<double> exec;
    dest.DT() = exec(fot, latency, src0.DT(), src1.DT());
    machine_state.cpr1_prf[m->prf_idx] = dest[0];
    machine_state.cpr1_prf[m->aux_prf_idx] = dest[1];
    m->complete_cycle = get_curr_cycle() + latency;
  }
  void execute_float(sim_state &machine_state) {
    int latency = 1;
    load_thunk<float> src0, src1, dest;
    src0[0] = machine_state.cpr1_prf[m->src0_prf];
    if(get_src1() != -1) {
      src1[0] = machine_state.cpr1_prf[m->src1_prf];
    }
    fp_executor<float> exec;
    dest.DT() = exec(fot, latency, src0.DT(), src1.DT());
    machine_state.cpr1_prf[m->prf_idx] = dest[0];
    m->complete_cycle = get_curr_cycle() + latency;
  }
  
public:
  fp_arith_op(sim_op op, fp_op_type fot) :
    mips_op(op), fmt((op->inst >> 21) & 31), fot(fot) {
    this->op_class = mips_op_type::fp;
  }
  virtual int get_src0() const {
    return (m->inst >> 11)&31; /* fs */
  }
  virtual int get_src1() const {
    switch(fot)
      {
      case fp_op_type::add:
      case fp_op_type::sub:
      case fp_op_type::mul:
      case fp_op_type::div:
	return (m->inst>>16)&31; /* ft */
      default:
	break;
      }
    return -1;
  }
  virtual int get_dest() const {
    return (m->inst >> 6)&31; 
  }
  virtual bool allocate(sim_state &machine_state) {
    bool allocated = false;
    if(fmt == FMT_D)
      allocated = allocate_double(machine_state);
    else
      allocated = allocate_float(machine_state);

    return allocated;

  }
  virtual void execute(sim_state &machine_state) {
    if(fmt == FMT_D)
      execute_double(machine_state);
    else
      execute_float(machine_state);
  }
  virtual bool ready(sim_state &machine_state) const {
    if(m->src0_prf != -1 and not(machine_state.cpr1_valid[m->src0_prf])) {
      return false;
    }
    if(m->src1_prf != -1 and not(machine_state.cpr1_valid[m->src1_prf])) {
      return false;
    }
    if(m->src2_prf != -1 and not(machine_state.cpr1_valid[m->src2_prf])) {
      return false;
    }
    if(m->src3_prf != -1 and not(machine_state.cpr1_valid[m->src3_prf])) {
      return false;
    }
    //std::cout << getAsmString(m->inst, m->pc) <<  " became ready @ " << get_curr_cycle() << "\n";
    //std::cout << "\tsrc0 prf" << m->src0_prf <<",value=" << std::hex
    //<< machine_state.cpr1_prf[m->src0_prf] << std::dec << "\n";
    //std::cout << "\tsrc1 prf" << m->src1_prf <<",value=" << std::hex
    // 	      << machine_state.cpr1_prf[m->src1_prf] << std::dec << "\n";
    return true;
  }

  virtual void complete(sim_state &machine_state) {
    if(not(m->is_complete) and (get_curr_cycle() == m->complete_cycle)) {
      m->is_complete = true;
      machine_state.cpr1_valid.set_bit(m->prf_idx);
      if(m->aux_prf_idx != -1) {
	machine_state.cpr1_valid.set_bit(m->aux_prf_idx);
      }
    }
  }

  virtual bool retire(sim_state &machine_state) {
    machine_state.cpr1_freevec.clear_bit(m->prev_prf_idx);
    machine_state.cpr1_valid.clear_bit(m->prev_prf_idx);
    machine_state.icnt++;
    machine_state.arch_cpr1[get_dest()] = machine_state.cpr1_prf[m->prf_idx];
    machine_state.arch_cpr1_last_pc[get_dest()] = m->pc;
    m->retire_cycle = get_curr_cycle();
    if(m->aux_prf_idx != -1) {
      machine_state.cpr1_freevec.clear_bit(m->aux_prev_prf_idx);
      machine_state.cpr1_valid.clear_bit(m->aux_prev_prf_idx);
      machine_state.arch_cpr1[get_dest()+1] = machine_state.cpr1_prf[m->aux_prf_idx];
    }
    return true;
  }

  virtual void undo(sim_state &machine_state) {
    machine_state.cpr1_rat[get_dest()] = m->prev_prf_idx;
    machine_state.cpr1_freevec.clear_bit(m->prf_idx);
    machine_state.cpr1_valid.clear_bit(m->prf_idx);
    if(m->aux_prf_idx != -1) {
      machine_state.cpr1_rat[get_dest()+1] = m->aux_prev_prf_idx;
      machine_state.cpr1_freevec.clear_bit(m->aux_prf_idx);
      machine_state.cpr1_valid.clear_bit(m->aux_prf_idx);
    }
  }

  
};

class cvts_truncw_op : public mips_op {
public:
  enum class op_type {cvts, truncw};
protected:
  uint32_t fmt;
  op_type ot;
  void execute_cvts(sim_state &machine_state) {
    switch(fmt)
      {
      case FMT_D:
	die();
	break;
      case FMT_W:
	*reinterpret_cast<float*>(&machine_state.cpr1_prf[m->prf_idx]) =
	  static_cast<float>(*reinterpret_cast<int32_t*>(&machine_state.cpr1_prf[m->src0_prf]));
	break;
      }
  }
  void execute_truncw(sim_state &machine_state) {
    switch(fmt)
      {
      case FMT_D:
	die();
	break;
      case FMT_S:
	*reinterpret_cast<int32_t*>(&machine_state.cpr1_prf[m->prf_idx]) =
	  static_cast<int32_t>(*reinterpret_cast<float*>(&machine_state.cpr1_prf[m->src0_prf]));
	break;
      }
  }
public:
  cvts_truncw_op(sim_op op, op_type ot) : mips_op(op), fmt((op->inst >> 21) & 31), ot(ot) {
    this->op_class = mips_op_type::fp;
  }
  virtual int get_src0() const {
    return (m->inst >> 11) & 31;
  }
  virtual int get_dest() const {
    return (m->inst >> 6) & 31; 
  }
  virtual bool allocate(sim_state &machine_state) {
    m->src0_prf = machine_state.cpr1_rat[get_src0()];
    if(fmt == FMT_D) {
      m->src1_prf = machine_state.cpr1_rat[get_src0()+1];
    }
    m->prf_idx = machine_state.cpr1_freevec.find_first_unset();
    if(m->prf_idx == -1)
      return false;
    m->prev_prf_idx = machine_state.cpr1_rat[get_dest()];
    machine_state.cpr1_freevec.set_bit(m->prf_idx);
    machine_state.cpr1_rat[get_dest()] = m->prf_idx;
    machine_state.cpr1_valid.clear_bit(m->prf_idx);
    return true;
  }
  virtual bool ready(sim_state &machine_state) const {
    if(not(machine_state.cpr1_valid.get_bit(m->src0_prf)))
      return false;
    if(m->src1_prf != -1 and not(machine_state.cpr1_valid.get_bit(m->src1_prf)))
      return false;
    return true;
  }
  virtual void execute(sim_state &machine_state) {
    switch(ot)
      {
      case op_type::cvts:
	execute_cvts(machine_state);
	break;
      case op_type::truncw:
	execute_truncw(machine_state);
	break;
      default:
	die();
      }
    m->complete_cycle = get_curr_cycle() + 1;
  }
  virtual void complete(sim_state &machine_state) {
    if(not(m->is_complete) and (get_curr_cycle() == m->complete_cycle)) {
      m->is_complete = true;
      machine_state.cpr1_valid.set_bit(m->prf_idx);
    }
  }
  virtual bool retire(sim_state &machine_state) {
    machine_state.cpr1_freevec.clear_bit(m->prev_prf_idx);
    machine_state.cpr1_valid.clear_bit(m->prev_prf_idx);

    machine_state.arch_cpr1[get_dest()] = machine_state.cpr1_prf[m->prf_idx];
    machine_state.arch_cpr1_last_pc[get_dest()] = m->pc;
    
    retired = true;
    machine_state.icnt++;
    m->retire_cycle = get_curr_cycle();
    
    return true;
  }
  virtual void undo(sim_state &machine_state) {
    machine_state.cpr1_rat[get_dest()] = m->prev_prf_idx;
    machine_state.cpr1_freevec.clear_bit(m->prf_idx);
    machine_state.cpr1_valid.clear_bit(m->prf_idx);
  }
};


class cvtd_op : public mips_op {
protected:
  uint32_t fmt;
public:
  cvtd_op(sim_op op) : mips_op(op), fmt((op->inst >> 21) & 31) {
    this->op_class = mips_op_type::fp;
  }
  virtual int get_src0() const {
    return (m->inst >> 11) & 31;
  }
  virtual int get_dest() const {
    return (m->inst >> 6) & 31; 
  }
  virtual bool allocate(sim_state &machine_state) {
    if(machine_state.cpr1_freevec.num_free() < 2)
      return false;
    m->prev_prf_idx = machine_state.cpr1_rat[get_dest()];
    m->aux_prev_prf_idx = machine_state.cpr1_rat[get_dest()+1];
    m->src0_prf = machine_state.cpr1_rat[get_src0()];
    m->prf_idx = machine_state.cpr1_freevec.find_first_unset();
    machine_state.cpr1_freevec.set_bit(m->prf_idx);
    m->aux_prf_idx = machine_state.cpr1_freevec.find_first_unset();
    machine_state.cpr1_freevec.set_bit(m->aux_prf_idx);
    
    machine_state.cpr1_valid.clear_bit(m->prf_idx);
    machine_state.cpr1_valid.clear_bit(m->aux_prf_idx);

    machine_state.cpr1_rat[get_dest()] = m->prf_idx;
    machine_state.cpr1_rat[get_dest()+1] = m->aux_prf_idx;
    return true;
  }
  virtual bool ready(sim_state &machine_state) const {
    return machine_state.cpr1_valid.get_bit(m->src0_prf);
  }
  virtual void execute(sim_state &machine_state) {
    load_thunk<double> dest;
    switch(fmt)
      {
      case FMT_S: {
	load_thunk<float> src0;
	src0[0] = machine_state.cpr1_prf[m->src0_prf];
	dest.DT() = static_cast<double>(src0.DT());
	break;
      }
      case FMT_W: {
	load_thunk<int32_t> src0;
	src0[0] = machine_state.cpr1_prf[m->src0_prf];
	dest.DT() = static_cast<double>(src0.DT());
	break;
      }
      default:
	die();
      }
    machine_state.cpr1_prf[m->prf_idx] = dest[0];
    machine_state.cpr1_prf[m->aux_prf_idx] = dest[1];
    m->complete_cycle = get_curr_cycle() + 1;
  }
  virtual void complete(sim_state &machine_state) {
    if(not(m->is_complete) and (get_curr_cycle() == m->complete_cycle)) {
      m->is_complete = true;
      machine_state.cpr1_valid.set_bit(m->prf_idx);
      machine_state.cpr1_valid.set_bit(m->aux_prf_idx);
    }
  }
  virtual bool retire(sim_state &machine_state) {
    machine_state.cpr1_freevec.clear_bit(m->prev_prf_idx);
    machine_state.cpr1_valid.clear_bit(m->prev_prf_idx);
    machine_state.cpr1_freevec.clear_bit(m->aux_prev_prf_idx);
    machine_state.cpr1_valid.clear_bit(m->aux_prev_prf_idx);

    machine_state.arch_cpr1[get_dest()] = machine_state.cpr1_prf[m->prf_idx];
    machine_state.arch_cpr1[get_dest()+1] = machine_state.cpr1_prf[m->aux_prf_idx];
    machine_state.arch_cpr1_last_pc[get_dest()] = m->pc;
    
    retired = true;
    machine_state.icnt++;
    m->retire_cycle = get_curr_cycle();
    
    return true;
  }
  virtual void undo(sim_state &machine_state) {
    machine_state.cpr1_rat[get_dest()] = m->prev_prf_idx;
    machine_state.cpr1_rat[get_dest()+1] = m->aux_prev_prf_idx;
    machine_state.cpr1_freevec.clear_bit(m->prf_idx);
    machine_state.cpr1_valid.clear_bit(m->prf_idx);
    machine_state.cpr1_freevec.clear_bit(m->aux_prf_idx);
    machine_state.cpr1_valid.clear_bit(m->aux_prf_idx);
  }
};


class break_op : public mips_op {
public:
  break_op(sim_op op) : mips_op(op) {
    this->op_class = mips_op_type::system;
  }
  virtual bool allocate(sim_state &machine_state) {
    return true;
  }
  virtual bool ready(sim_state &machine_state) const {
    return true;
  }
  virtual void execute(sim_state &machine_state) {
    m->complete_cycle = get_curr_cycle() + 1;
  }
  virtual void complete(sim_state &machine_state) {
    if(not(m->is_complete) and (get_curr_cycle() == m->complete_cycle)) {
      m->is_complete = true;
    }
  }
  virtual bool retire(sim_state &machine_state) {
    retired = true;
    machine_state.icnt++;
    m->retire_cycle = get_curr_cycle();
    return true;
  }
  virtual bool stop_sim() const {
    return true;
  }
  virtual void undo(sim_state &machine_state) {}
};


class monitor_op : public mips_op {
protected:
  int32_t src_regs[4] = {0};
  
public:
  monitor_op(sim_op op) : mips_op(op) {
    this->op_class = mips_op_type::system;
  }
  virtual int get_dest() const {
    return 2;
  }
  virtual int get_src0() const {
    return 4;
  }
  virtual int get_src1() const {
    return 5;
  }
  virtual int get_src2() const {
    return 6;
  }
  virtual int get_src3() const {
    return 31;
  }
  virtual bool allocate(sim_state &machine_state) {
    m->src0_prf = machine_state.gpr_rat[4];
    m->src1_prf = machine_state.gpr_rat[5];
    m->src2_prf = machine_state.gpr_rat[6];
    m->src3_prf = machine_state.gpr_rat[31];
    m->prev_prf_idx = machine_state.gpr_rat[2];
    int64_t prf_id = machine_state.gpr_freevec.find_first_unset();
    if(prf_id == -1)
      return false;
    assert(prf_id >= 0);
    machine_state.gpr_freevec.set_bit(prf_id);
    machine_state.gpr_rat[2] = prf_id;
    m->prf_idx = prf_id;
    machine_state.gpr_valid.clear_bit(prf_id);
    return true;
  }
  virtual bool ready(sim_state &machine_state) const {
    if(not(machine_state.gpr_valid.get_bit(m->src0_prf))) {
      return false;
    }
    if(not(machine_state.gpr_valid.get_bit(m->src1_prf))) {
      return false;
    }
    if(not(machine_state.gpr_valid.get_bit(m->src2_prf))) {
      return false;
    }
    if(not(machine_state.gpr_valid.get_bit(m->src3_prf))) {
      return false;
    }
    return true;
  }
  virtual void execute(sim_state &machine_state) {
    src_regs[0] = machine_state.gpr_prf[m->src0_prf];
    src_regs[1] = machine_state.gpr_prf[m->src1_prf];
    src_regs[2] = machine_state.gpr_prf[m->src2_prf];
    src_regs[3] = machine_state.gpr_prf[m->src3_prf];
    m->correct_pc = src_regs[3];
    m->complete_cycle = get_curr_cycle() + 1;
    m->branch_exception = true;
  }
  virtual void complete(sim_state &machine_state) {
    if(not(m->is_complete) and (get_curr_cycle() == m->complete_cycle)) {
      m->is_complete = true;
    }
  }
  virtual bool retire(sim_state &machine_state) {
    uint32_t reason = ((m->inst >> RSVD_INSTRUCTION_ARG_SHIFT) & RSVD_INSTRUCTION_ARG_MASK) >> 1;
    sparse_mem & mem = *(machine_state.mem);
    machine_state.gpr_prf[m->prf_idx] = 0;
    switch(reason)
      {
      case 8: {
      /* int write(int file, char *ptr, int len) */
	int fd = src_regs[0];
	int nr = src_regs[2];
	machine_state.gpr_prf[m->prf_idx] = per_page_rdwr<true>(mem, fd, src_regs[1], nr);
	if(fd==1)
	  fflush(stdout);
	else if(fd==2)
	  fflush(stderr);
      }
      case 10: /* close */
	if(src_regs[0] > 2) {
	  machine_state.gpr_prf[m->prf_idx] = close(src_regs[0]);
	}
	break;	
      case 33: {
	*((uint32_t*)(mem + (uint32_t)src_regs[0] + 0)) = 0;
	*((uint32_t*)(mem + (uint32_t)src_regs[0] + 4)) = 0;
	break;
      }
      case 35: {
	for(int i = 0; i < std::min(20, sysArgc); i++) {
	  uint32_t arrayAddr = static_cast<uint32_t>(src_regs[0])+4*i;
	  uint32_t ptr = accessBigEndian(*((uint32_t*)(mem + arrayAddr)));
	  strcpy((char*)(mem + ptr), sysArgv[i]);
	}
	machine_state.gpr_prf[m->prf_idx] = sysArgc;

      }
      case 55:
	*((uint32_t*)(mem + (uint32_t)src_regs[0] + 0)) = accessBigEndian(K1SIZE);
	/* No Icache */
	*((uint32_t*)(mem + (uint32_t)src_regs[0] + 4)) = 0;
	/* No Dcache */
	*((uint32_t*)(mem + (uint32_t)src_regs[0] + 8)) = 0;
	break;

      default:
	std::cerr << "execute monitor op with reason "<< reason << "\n";
	die();

      }
    /* not valid until after this instruction retires */
    machine_state.gpr_valid.set_bit(m->prf_idx);
    machine_state.gpr_freevec.clear_bit(m->prev_prf_idx);

    retired = true;
    machine_state.icnt++;
    machine_state.arch_grf[get_dest()] = machine_state.gpr_prf[m->prf_idx];
    machine_state.arch_grf_last_pc[get_dest()] = m->pc;
    m->exec_parity = machine_state.gpr_parity();
    m->retire_cycle = get_curr_cycle();
    return true;
  }
  virtual void undo(sim_state &machine_state) {
    machine_state.gpr_rat[get_dest()] = m->prev_prf_idx;
    machine_state.gpr_freevec.clear_bit(m->prf_idx);
    machine_state.gpr_valid.clear_bit(m->prf_idx);
  }
};

class rtype_const_shift_alu_op : public rtype_alu_op {
public:
  rtype_const_shift_alu_op(sim_op op, rtype_alu_op::r_type rt) : rtype_alu_op(op, rt) {}
  virtual int get_src1() const {
    return -1;
  }
};

static mips_op* decode_jtype_insn(sim_op m_op) {
  uint32_t opcode = (m_op->inst)>>26;
  switch(opcode)
    {
    case 0x2:
      return new jump_op(m_op, jump_op::jump_type::j);
    case 0x3:
      return new jump_op(m_op, jump_op::jump_type::jal);
    default:
      break;
    }
  return nullptr;
}


static mips_op* decode_itype_insn(sim_op m_op) {
  uint32_t opcode = (m_op->inst)>>26;
  switch(opcode)
    {
    case 0x1:
      switch((m_op->inst >> 16) & 31)
	{
	case 0x0:
	  return new branch_op(m_op,branch_op::branch_type::bltz);
	case 0x1:
	  return new branch_op(m_op,branch_op::branch_type::bgez);
	case 0x2:
	  return new branch_op(m_op,branch_op::branch_type::bltzl);
	case 0x3:
	  return new branch_op(m_op,branch_op::branch_type::bgezl);
	default:
	  dprintf(log_fd, "unknown branch type @ %x\n", m_op->pc);
	  die();
	}
    case 0x04: /* beq */
      return new branch_op(m_op,branch_op::branch_type::beq);
    case 0x05: /* bne */
      return new branch_op(m_op,branch_op::branch_type::bne);
    case 0x6: /* blez */
      return new branch_op(m_op,branch_op::branch_type::blez);
    case 0x7: /* bgtz */
      return new branch_op(m_op,branch_op::branch_type::bgtz);
    case 0x08: /* addi */
      return new itype_alu_op(m_op);
    case 0x09: /* addiu */
      return new itype_alu_op(m_op);
    case 0x0A: /* slti */
      return new itype_alu_op(m_op);
    case 0x0B:/* sltiu */
      return new itype_alu_op(m_op);
    case 0x0c: /* andi */
      return new itype_alu_op(m_op);
    case 0x0d: /* ori */
      return new itype_alu_op(m_op);
    case 0x0e: /* xori */
      return new itype_alu_op(m_op);
    case 0x0F: /* lui */
      return new itype_lui_op(m_op);
    case 0x14:
      return new branch_op(m_op,branch_op::branch_type::beql);
    case 0x16:
      return new branch_op(m_op,branch_op::branch_type::blezl);
    case 0x15:
      return new branch_op(m_op,branch_op::branch_type::bnel);
    case 0x17:
      return new branch_op(m_op,branch_op::branch_type::bgtzl);
    case 0x20:
      return new load_op(m_op, load_op::load_type::lb);
    case 0x21: /* lh */
      return new load_op(m_op, load_op::load_type::lh);
    case 0x23: /* lw */
      return new load_op(m_op, load_op::load_type::lw);
    case 0x24: /* lbu */
      return new load_op(m_op, load_op::load_type::lbu);
    case 0x25: /* lbu */
      return new load_op(m_op, load_op::load_type::lhu);
    case 0x28: /* sb */
      return new store_op(m_op, store_op::store_type::sb);
    case 0x29: /* sh */
      return new store_op(m_op, store_op::store_type::sh);
    case 0x2B: /* sw */
      return new store_op(m_op, store_op::store_type::sw);
    case 0x31:
      return new fp_load_op(m_op, fp_load_op::load_type::lwc1);
    case 0x35:
      return new fp_load_op(m_op, fp_load_op::load_type::ldc1);
    case 0x39:
      return new fp_store_op(m_op, fp_store_op::store_type::swc1);
    case 0x3d:
      return new fp_store_op(m_op, fp_store_op::store_type::sdc1);
    default:
      break;
    }
  return nullptr;
}


static mips_op* decode_rtype_insn(sim_op m_op) {
  uint32_t opcode = (m_op->inst)>>26;
  uint32_t funct = m_op->inst & 63;
  
  switch(funct) 
    {
    case 0x00: /*sll*/
      return new rtype_const_shift_alu_op(m_op, rtype_alu_op::r_type::sll);
    case 0x01: /* movci */
      return new movci_op(m_op);
    case 0x02: /* srl */
      return new rtype_const_shift_alu_op(m_op, rtype_alu_op::r_type::srl);
    case 0x03: /* sra */
      return new rtype_const_shift_alu_op(m_op, rtype_alu_op::r_type::sra);
    case 0x04: /* sllv */
      return new rtype_alu_op(m_op, rtype_alu_op::r_type::sllv);
    case 0x05:
      return new monitor_op(m_op);
    case 0x06: /* srlv */
      return new rtype_alu_op(m_op, rtype_alu_op::r_type::srlv);
    case 0x07: /* srav */
      return new rtype_alu_op(m_op, rtype_alu_op::r_type::srav);
    case 0x08: /* jr */
      return new jump_op(m_op, jump_op::jump_type::jr);
    case 0x09: /* jalr */
      return new jump_op(m_op, jump_op::jump_type::jalr);
    case 0x0a: /* movz */
      return new rtype_alu_op(m_op, rtype_alu_op::r_type::movz);
    case 0x0b: /* movn */
      return new rtype_alu_op(m_op, rtype_alu_op::r_type::movn);
#if 0
    case 0x0C: /* syscall */
      printf("syscall()\n");
      die();
      break;
#endif
    case 0x0D: /* break */
      return new break_op(m_op);
#if 0 
    case 0x0f: /* sync */
      s->pc += 4;
      break;
#endif
    case 0x10: /* mfhi */
      return new lo_hi_move(m_op, lo_hi_move::lo_hi_type::mfhi);
    case 0x11: /* mthi */ 
      return new lo_hi_move(m_op, lo_hi_move::lo_hi_type::mthi);
    case 0x12: /* mflo */
      return new lo_hi_move(m_op, lo_hi_move::lo_hi_type::mflo);
    case 0x13: /* mtlo */
      return new lo_hi_move(m_op, lo_hi_move::lo_hi_type::mtlo);
    case 0x18: /* mult */
      return new mult_div_op(m_op, mult_div_op::mult_div_types::mult);
    case 0x19:  /* multu */
      return new mult_div_op(m_op, mult_div_op::mult_div_types::multu);
#if 0
    case 0x1A: /* div */
      if(s->gpr[rt] != 0) {
	s->lo = s->gpr[rs] / s->gpr[rt];
	s->hi = s->gpr[rs] % s->gpr[rt];
      }
      s->pc += 4;
      break;
#endif
    case 0x1B: /* divu */
      return new mult_div_op(m_op, mult_div_op::mult_div_types::divu);
    case 0x20: /* add */
      return new rtype_alu_op(m_op, rtype_alu_op::r_type::add);
    case 0x21: 
      return new rtype_alu_op(m_op, rtype_alu_op::r_type::addu);
    case 0x22: /* sub */
      return new rtype_alu_op(m_op, rtype_alu_op::r_type::sub);
    case 0x23: /*subu*/  
      return new rtype_alu_op(m_op, rtype_alu_op::r_type::subu);
    case 0x24: /* and */
      return new rtype_alu_op(m_op, rtype_alu_op::r_type::and_);
    case 0x25: /* or */
      return new rtype_alu_op(m_op, rtype_alu_op::r_type::or_);
    case 0x26: /* xor */
      return new rtype_alu_op(m_op, rtype_alu_op::r_type::xor_);
    case 0x27: /* nor */
      return new rtype_alu_op(m_op, rtype_alu_op::r_type::nor_);
    case 0x2A: /* slt */
      return new rtype_alu_op(m_op, rtype_alu_op::r_type::slt);
    case 0x2B:  /* sltu */
      return new rtype_alu_op(m_op, rtype_alu_op::r_type::sltu);
    case 0x34: /* teq */
      return new rtype_alu_op(m_op, rtype_alu_op::r_type::teq);
    default:
      dprintf(log_fd,"unknown RType instruction @ %x\n", m_op->pc); 
      return nullptr;
    }
}

mips_op* decode_coproc1_insn(sim_op m_op) {
  uint32_t opcode = m_op->inst>>26;
  uint32_t functField = (m_op->inst>>21) & 31;
  uint32_t lowop = m_op->inst & 63;  
  uint32_t fmt = (m_op->inst >> 21) & 31;
  uint32_t nd_tf = (m_op->inst>>16) & 3;
  uint32_t lowbits = m_op->inst & ((1<<11)-1);
  opcode &= 0x3;

  if(fmt == 0x8) {
    switch(nd_tf)
      {
      case 0x0:
	return new branch_op(m_op, branch_op::branch_type::bc1f);
      case 0x1:
	return new branch_op(m_op, branch_op::branch_type::bc1t);
      case 0x2:
	return new branch_op(m_op, branch_op::branch_type::bc1fl);
      case 0x3:
	return new branch_op(m_op, branch_op::branch_type::bc1tl);
      }
    return nullptr;
  }
  else if((lowbits == 0) && ((functField==0x0) || (functField==0x4))) {
    if(functField == 0x0)
      return new mfc1(m_op);
    else if(functField == 0x4)
      return new mtc1(m_op);
  }
  else {
    if((lowop >> 4) == 3) {
      return new fp_cmp(m_op);
    }
    else {
      switch(lowop)
	{
	  case 0x0:
	    return new fp_arith_op(m_op, fp_arith_op::fp_op_type::add);
	  case 0x1:
	    return new fp_arith_op(m_op, fp_arith_op::fp_op_type::sub);
	  case 0x2:
	    return new fp_arith_op(m_op, fp_arith_op::fp_op_type::mul);
	  case 0x3:
	    return new fp_arith_op(m_op, fp_arith_op::fp_op_type::div);
	  case 0x4:
	    return new fp_arith_op(m_op, fp_arith_op::fp_op_type::sqrt);
	  case 0x5:
	    return new fp_arith_op(m_op, fp_arith_op::fp_op_type::abs);
	  case 0x6:
	    return new fp_arith_op(m_op, fp_arith_op::fp_op_type::mov);
	  case 0x7:
	    return new fp_arith_op(m_op, fp_arith_op::fp_op_type::neg);
#if 0
	  case 0x9:
	    _truncl(inst, s);
	    break;
#endif
	  case 0xd:
	    return new cvts_truncw_op(m_op, cvts_truncw_op::op_type::truncw);
#if 0
	  case 0x11:
	    _fmovc(inst, s);
	    break;
	  case 0x12:
	    _fmovz(inst, s);
	    break;
	  case 0x13:
	    _fmovn(inst, s);
	    break;
#endif
	case 0x15:
	  return new fp_arith_op(m_op, fp_arith_op::fp_op_type::recip);
	case 0x16:
	  return new fp_arith_op(m_op, fp_arith_op::fp_op_type::rsqrt);
	case 0x20:
	  return new cvts_truncw_op(m_op, cvts_truncw_op::op_type::cvts);
	case 0x21:
	  return new cvtd_op(m_op);
	default:
	  printf("unhandled coproc1 instruction (%x) @ %08x\n", m_op->inst, m_op->pc);
	  die();
	  break;
	}
    }
  }
  
  
  return nullptr;
}

static mips_op* decode_special2_insn(sim_op m_op) {
  uint32_t funct = m_op->inst & 63; 
  switch(funct)
    {
    case 0x0:
      return new mult_div_op(m_op, mult_div_op::mult_div_types::madd);
    case 0x1:
      return new mult_div_op(m_op, mult_div_op::mult_div_types::maddu);
    case 0x2:
      return new mul_op(m_op);
    case 0x20:
      return new clz_op(m_op);
    default:
      break;
    }
  return nullptr;
}


static mips_op* decode_special3_insn(sim_op m_op) {
  uint32_t funct = m_op->inst & 63;
  uint32_t op = (m_op->inst>>6) & 31;
  switch(funct)
    {
    case 0x0:
      return new ext_op(m_op);
    case 0x20:
      switch(op)
	{
	case 0x10:
	  return new se_op(m_op, se_op::se_type::seb);
	case 0x18:
	  return new se_op(m_op, se_op::se_type::seh);
	default:
	  die();
	}
    default:
      break;
    }
  
  return nullptr;
}





mips_op* decode_insn(sim_op m_op) {
  uint32_t opcode = (m_op->inst)>>26;
  bool isRType = (opcode==0);
  bool isJType = ((opcode>>1)==1);
  bool isCoproc0 = (opcode == 0x10);
  bool isCoproc1 = (opcode == 0x11);
  bool isCoproc1x = (opcode == 0x13);
  bool isCoproc2 = (opcode == 0x12);
  bool isSpecial2 = (opcode == 0x1c); 
  bool isSpecial3 = (opcode == 0x1f);
  bool isLoadLinked = (opcode == 0x30);
  bool isStoreCond = (opcode == 0x38);


  if(isRType)
    return decode_rtype_insn(m_op);
  else if(isSpecial2)
    return decode_special2_insn(m_op);
  else if(isSpecial3)
    return decode_special3_insn(m_op);
  else if(isJType)
    return decode_jtype_insn(m_op);
  else if(isCoproc0) {
    switch((m_op->inst >> 21) &31) 
      {
#if 0
      case 0x0: /*mfc0*/
#endif
      case 0x4: /*mtc0*/
	return new mtc0(m_op);
      default:
	break;
      }
    return nullptr;
  }
  else if(isCoproc1) {
    return decode_coproc1_insn(m_op);
  }
  else if(isCoproc1x)
    return nullptr;
  else if(isCoproc2)
    return nullptr;
  else if(isLoadLinked)
    return nullptr;
  else if(isStoreCond)
    return nullptr;
  else {
    return decode_itype_insn(m_op);
  }
}

mips_meta_op::~mips_meta_op() {
  if(op) {
    delete op;
  }
}

bool mips_op::allocate(sim_state &machine_state) {
  dprintf(log_fd, "allocate must be implemented\n");
  die();
  return false;
}

void mips_op::execute(sim_state &machine_state) {
  dprintf(log_fd, "execute must be implemented, pc %x\n", m->pc);
  die();
}

void mips_op::complete(sim_state &machine_state) {
  dprintf(log_fd, "complete must be implemented, pc %x\n", m->pc);
  die();
}

bool mips_op::retire(sim_state &machine_state) {
  dprintf(log_fd, "retire must be implemented, pc %x\n", m->pc);
  return false;
}

bool mips_op::ready(sim_state &machine_state) const {
  dprintf(log_fd, "ready must be implemented\n");
  return false;
}

void mips_op::undo(sim_state &machine_state) {
  dprintf(log_fd, "implement %x for undo\n", m->pc);
  die();
}
