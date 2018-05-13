#include "mips_op.hh"
#include "helper.hh"

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
      dprintf(2, "mistakes were made @ %d\n", __LINE__);
      exit(-1);
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
    return true;
  }
  virtual void undo(sim_state &machine_state) {
    dprintf(2, "%s", __PRETTY_FUNCTION__);
    if(m->prev_prf_idx != -1) {
      machine_state.cpr0_rat[get_dest()] = m->prev_prf_idx;
    }
    if(m->prf_idx != -1) {
      machine_state.cpr0_freevec.clear_bit(m->prf_idx);
      machine_state.cpr0_valid.set_bit(m->prf_idx);
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
    dprintf(2, "::::: prev dest of mtc is %d, new dest %d\n", m->prev_prf_idx, prf_id);
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
      dprintf(2, "mistakes were made @ %d\n", __LINE__);
      exit(-1);
    }
    machine_state.cpr1_prf[m->prf_idx] = machine_state.gpr_prf[m->src0_prf];
    m->complete_cycle = get_curr_cycle() + 1;
  }
  virtual void complete(sim_state &machine_state) {
    if(not(m->is_complete) and (get_curr_cycle() == m->complete_cycle)) {
      m->is_complete = true;
      dprintf(2, "%x : m->prf_idx = %ld\n", m->pc, m->prf_idx);
      machine_state.cpr1_valid.set_bit(m->prf_idx);
    }
  }
  virtual bool retire(sim_state &machine_state) {
    machine_state.cpr1_freevec.clear_bit(m->prev_prf_idx);
    machine_state.cpr1_valid.clear_bit(m->prev_prf_idx);
    retired = true;
    machine_state.icnt++;
    return true;
  }
  virtual void undo(sim_state &machine_state) {
    dprintf(2, "%s", __PRETTY_FUNCTION__);
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
    machine_state.gpr_rat_sanity_check(prf_id);
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
      dprintf(2, "mistakes were made @ %d\n", __LINE__);
      exit(-1);
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
    if(machine_state.gpr_rat_sanity_check(m->prev_prf_idx)) {
      dprintf(2, "mapping still exists!..%x\n", m->pc);      
    }
    machine_state.icnt++;
    retired = true;
    return true;
  }
  virtual void undo(sim_state &machine_state) {
    dprintf(2, "%s", __PRETTY_FUNCTION__);
    if(m->prev_prf_idx != -1) {
      machine_state.gpr_rat[get_dest()] = m->prev_prf_idx;
    }
    if(m->prf_idx != -1) {
      machine_state.gpr_freevec.clear_bit(m->prf_idx);
      machine_state.gpr_valid.clear_bit(m->prf_idx);
    }
  }
};


class rtype_alu_op : public mips_op {
protected:
  rtype r;
public:
  rtype_alu_op(sim_op op) :
    mips_op(op), r(op->inst) {
    this->op_class = mips_op_type::alu;
  }
  virtual int get_dest() const {
    return r.rr.rd;
  }
  virtual int get_src0() const {
    return r.rr.rs;
  }
  virtual int get_src1() const {
    return r.rr.rt;
  }
  virtual bool allocate(sim_state &machine_state) {
    if(get_src0() != -1) {
      m->src0_prf = machine_state.gpr_rat[get_src0()];
    }
    if(get_src1() != -1) {
      m->src1_prf = machine_state.gpr_rat[get_src1()];
    }
    if(get_dest() > 0) {
      m->prev_prf_idx = machine_state.gpr_rat[get_dest()];
      int64_t prf_id = machine_state.gpr_freevec.find_first_unset();
      if(prf_id == -1)
	return false;
      machine_state.gpr_rat_sanity_check(prf_id);
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
    return true;
  }
  virtual void execute(sim_state &machine_state) {
    if(not(ready(machine_state))) {
      dprintf(2, "mistakes were made @ %d\n", __LINE__);
      exit(-1);
    }
    if(m->prf_idx != -1) {
      uint32_t funct = m->inst & 63;
      uint32_t sa = (m->inst >> 6) & 31;
      switch(funct)
	{
	case 0x00: /*sll*/
	  machine_state.gpr_prf[m->prf_idx] = machine_state.gpr_prf[m->src0_prf] << sa;
	  break;
	case 0x02: /*srl */
	  machine_state.gpr_prf[m->prf_idx] = static_cast<uint32_t>(machine_state.gpr_prf[m->src0_prf]) >> sa;
	  break;
	case 0x21: { /* addu */
	  uint32_t urs = static_cast<uint32_t>(machine_state.gpr_prf[m->src0_prf]);
	  uint32_t urt = static_cast<uint32_t>(machine_state.gpr_prf[m->src1_prf]);
	  machine_state.gpr_prf[m->prf_idx] = (urs + urt);
	  break;
	}
	case 0x23: {/*subu*/  
	  uint32_t urs = static_cast<uint32_t>(machine_state.gpr_prf[m->src0_prf]);
	  uint32_t urt = static_cast<uint32_t>(machine_state.gpr_prf[m->src1_prf]);
	  uint32_t y = urs - urt;
	  machine_state.gpr_prf[m->prf_idx] = y;
	  break;
	}
	case 0x24:
	  machine_state.gpr_prf[m->prf_idx] = machine_state.gpr_prf[m->src0_prf] &
	    machine_state.gpr_prf[m->src1_prf];
	  break;
	case 0x2B: { /* sltu */
	  uint32_t urs = static_cast<uint32_t>(machine_state.gpr_prf[m->src0_prf]);
	  uint32_t urt = static_cast<uint32_t>(machine_state.gpr_prf[m->src1_prf]);
	  machine_state.gpr_prf[m->prf_idx] = (urs < urt);
	  break;
	}
	default:
	  dprintf(2, "wtf is funct %x (pc = %x)\n", funct, m->pc);
	  exit(-1);
	}
    }
    m->complete_cycle = get_curr_cycle() + 1;
  }
  virtual void complete(sim_state &machine_state) {
    if(not(m->is_complete) and (get_curr_cycle() == m->complete_cycle)) {
      m->is_complete = true;
      //dprintf(2, "inst @ %x retiring, prf_idx = %d\n", m->pc, m->prf_idx);
      if(m->prf_idx != -1) {
	machine_state.gpr_valid.set_bit(m->prf_idx);
      }
    }
  }
  virtual bool retire(sim_state &machine_state) {
    if(m->prev_prf_idx != -1) {
      machine_state.gpr_freevec.clear_bit(m->prev_prf_idx);
      machine_state.gpr_valid.clear_bit(m->prev_prf_idx);
      if(machine_state.gpr_rat_sanity_check(m->prev_prf_idx)) {
	dprintf(2, "mapping still exists!..%x\n", m->pc);      
      }
    }
    retired = true;
    machine_state.icnt++;
    return true;
  }
  virtual void undo(sim_state &machine_state) {
    dprintf(2, "%s", __PRETTY_FUNCTION__);
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
      if(machine_state.gpr_rat_sanity_check(prf_id)) {
	dprintf(2, "%x : dest register %d has old prf %d, new prf %d\n",
		m->pc, get_dest(), m->prev_prf_idx, prf_id);
	exit(-1);
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
	dprintf(2, "implement me %x\n", m->pc);
	exit(-1);
      }
    m->complete_cycle = get_curr_cycle() + 1;
  }
  
  virtual void complete(sim_state &machine_state) {
    if(not(m->is_complete) and (get_curr_cycle() == m->complete_cycle)) {
      m->is_complete = true;
      dprintf(2, "::%x arch reg %d -> prf %lld\n", m->pc, get_dest(), m->prf_idx);
      machine_state.gpr_valid.set_bit(m->prf_idx);
    }
  }
  virtual bool retire(sim_state &machine_state) {
    dprintf(2, "::%x %s %d\n", m->pc, __PRETTY_FUNCTION__, m->is_complete);
    if(m->is_complete == false) {
      exit(-1);
    }
    if(machine_state.gpr_rat_sanity_check(m->prev_prf_idx)) {
      dprintf(2, "mapping still exists!..%x\n", m->pc);      
    }
    machine_state.gpr_freevec.clear_bit(m->prev_prf_idx);
    machine_state.gpr_valid.clear_bit(m->prev_prf_idx);
    retired = true;
    machine_state.icnt++;
    return true;
  }
  virtual void undo(sim_state &machine_state) {
    dprintf(2, "%s", __PRETTY_FUNCTION__);
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
	dprintf(2, "%lu gprs in use..\n", machine_state.gpr_freevec.popcount());
	return false;
      }
      machine_state.gpr_rat_sanity_check(prf_id);
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
    m->branch_exception = not(m->predict_taken);

    dprintf(2, "=> %lu : executing jump %x, this = %p\n",
	    get_curr_cycle(), m->pc, this);

    if(m->complete_cycle != -1) {
      dprintf(2,"mistakes have been made\n");
      asm("int3");
    }
    
    if(get_dest() != -1) {
      machine_state.gpr_prf[m->prf_idx] = m->pc + 8;
    }
    m->complete_cycle = get_curr_cycle() + 1;
  }
  virtual bool retire(sim_state &machine_state) {
    if(m->prev_prf_idx != -1) {
      if(machine_state.gpr_rat_sanity_check(m->prev_prf_idx)) {
	dprintf(2, "mapping still exists!..%x\n", m->pc);      
      }
      machine_state.gpr_freevec.clear_bit(m->prev_prf_idx);
      machine_state.gpr_valid.clear_bit(m->prev_prf_idx);
    }
    retired = true;
    machine_state.icnt++;
    return true;
  }
  virtual void undo(sim_state &machine_state) {
    dprintf(2, "%s", __PRETTY_FUNCTION__);
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

uint32_t not_ready_count = 0;

class branch_op : public mips_op {
public:
  enum class branch_type {beq, bne, blez, bgtz, beql, bnel, blezl, bgtzl, bgez, bgezl, bltz, bltzl};
protected:
  itype i_;
  branch_type bt;
  bool take_br = false;
  uint32_t branch_target = 0;
public:
  branch_op(sim_op op, branch_type bt) :
    mips_op(op), bt(bt), i_(op->inst), take_br(false) {
    this->op_class = mips_op_type::jmp;
    int16_t himm = (int16_t)(m->inst & ((1<<16) - 1));
    int32_t imm = ((int32_t)himm) << 2;
    uint32_t npc = m->pc+4;
    branch_target = (imm+npc);
    op->correct_pc = branch_target;
  }
  virtual int get_src0() const {
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
	return -1;
      default:
	break;
      }
    return i_.ii.rt;
  }
  virtual bool allocate(sim_state &machine_state) {
    dprintf(2, "branch allocated\n");
    if(get_src0() != -1) {
      m->src0_prf = machine_state.gpr_rat[get_src0()];
      dprintf(2, "branch src0 arch reg %d, prf %d\n",
	      get_src0(), m->src0_prf);
    }
    if(get_src1() != -1) {
      m->src1_prf = machine_state.gpr_rat[get_src1()];
      dprintf(2, "branch src1 arch reg %d, prf %d\n",
	      get_src1(), m->src1_prf);
    }
    return true;
  }
  virtual bool ready(sim_state &machine_state) const {
    if(not_ready_count == 10)
      exit(-1);
    
    if(m->src0_prf != -1 and not(machine_state.gpr_valid.get_bit(m->src0_prf))) {
      dprintf(2, "branch %x : src0 (prf %d) not ready, alloc'd @ %llu\n",
	      m->pc, m->src0_prf,m->alloc_cycle );
      not_ready_count++;
      return false;
    }
    if(m->src1_prf != -1 and not(machine_state.gpr_valid.get_bit(m->src1_prf))) {
      dprintf(2, "branch %x : src1 not ready\n", m->pc);
      not_ready_count++;
      return false;
    }
    not_ready_count = 0;
    return true;
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
	take_br = machine_state.gpr_prf[m->src0_prf] > 0;
	m->has_delay_slot = true;
	break;
      case branch_type::bgtz:
	take_br = machine_state.gpr_prf[m->src0_prf] <= 0;
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
      default:
	dprintf(2, "wtf @ %x\n", m->pc);
	exit(-1);
      }

    if(take_br != m->predict_taken) {
      if(take_br and not(m->predict_taken)) {
	m->branch_exception = true;
      }
      else {
	dprintf(2, "need to handle other mispredict condition\n");
	exit(-1);
      }
    }

    if(m->likely_squash) {
      dprintf(2,"LIKELY SQUASHHHHHH\n");
      m->branch_exception = true;
      m->correct_pc = m->pc + 8;
    }

    if(m->pc == 0xa00200b4) {
      dprintf(2, "=====> %llu : bne @ %x in execute, taken = %d, target = %x, exception = %d\n", 
	      get_curr_cycle(), m->pc, take_br, branch_target, m->branch_exception);
    }
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
    return true;
  }
  virtual void undo(sim_state &machine_state) {
      dprintf(2, "%s", __PRETTY_FUNCTION__);
  }
};


class load_op : public mips_op {
public:
  enum class load_type {lb,lbu,lh,lhu,lw}; 
protected:
  itype i_;
  load_type lt;
  int32_t imm = -1;
  uint32_t effective_address = ~0;
public:
  load_op(sim_op op, load_type lt) :
    mips_op(op), i_(op->inst), lt(lt) {
    this->op_class = mips_op_type::mem;
    int16_t himm = static_cast<int16_t>(m->inst & ((1<<16) - 1));
    imm = static_cast<int32_t>(himm);
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
    int64_t prf_id = machine_state.gpr_freevec.find_first_unset();
    if(prf_id == -1)
      return false;
    machine_state.gpr_rat_sanity_check(prf_id);
    machine_state.gpr_freevec.set_bit(prf_id);
    machine_state.gpr_rat[get_dest()] = prf_id;
    dprintf(2, "allocated load -> %d to prf %lld, rat = %lld\n", 
	    get_dest(), prf_id, machine_state.gpr_rat[get_dest()]);
    m->prf_idx = prf_id;
    machine_state.gpr_valid.clear_bit(prf_id);
    return true;
  }
  virtual bool ready(sim_state &machine_state) const {
    if(not(machine_state.gpr_valid.get_bit(m->src0_prf))) {
      dprintf(2, "load @ %x not ready\n", m->pc);
      return false;
    }
    return true;
  }
  virtual void execute(sim_state &machine_state) {
    effective_address = machine_state.gpr_prf[m->src0_prf] + imm;
    m->complete_cycle = get_curr_cycle() + 1;
  }
  virtual void complete(sim_state &machine_state) {
    if(not(m->is_complete) and (get_curr_cycle() == m->complete_cycle)) {
      m->is_complete = true;
    }
  }
  virtual bool retire(sim_state &machine_state) {
    sparse_mem & mem = *(machine_state.mem);
    switch(lt)
      {
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
	exit(-1);
      }

    if(machine_state.gpr_rat_sanity_check(m->prev_prf_idx)) {
      dprintf(2, "mapping still exists!..%x\n", m->pc);      
    }
    machine_state.gpr_valid.set_bit(m->prf_idx);
    machine_state.gpr_freevec.clear_bit(m->prev_prf_idx);
    machine_state.gpr_valid.clear_bit(m->prev_prf_idx);
    dprintf(2, "LOAD @ %x complete to prf %d!, alloc'd @ %llu\n",
	    m->pc, m->prf_idx, m->alloc_cycle);
    retired = true;
    machine_state.icnt++;
    return true;
  }
  virtual void undo(sim_state &machine_state) {
    dprintf(2, "%s", __PRETTY_FUNCTION__);
    machine_state.gpr_rat[get_dest()] = m->prev_prf_idx;
    machine_state.gpr_freevec.clear_bit(m->prf_idx);
    machine_state.gpr_valid.clear_bit(m->prf_idx);
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
    this->op_class = mips_op_type::mem;
    int16_t himm = static_cast<int16_t>(m->inst & ((1<<16) - 1));
    imm = static_cast<int32_t>(himm);
  }
  virtual int get_src0() const {
    return i_.ii.rt;
  }
  virtual int get_src1() const {
    return i_.ii.rs;
  }
  virtual bool allocate(sim_state &machine_state) {
    if(get_src0() != -1) {
      m->src0_prf = machine_state.gpr_rat[get_src0()];
    }
    if(get_src1() != -1) {
      m->src1_prf = machine_state.gpr_rat[get_src1()];
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
    dprintf(2, "store at head of rob, ea %x, data %d\n", effective_address, store_data);
    sparse_mem & mem = *(machine_state.mem);
    switch(st)
      {
      case store_type::sb:
	mem.at(effective_address) = static_cast<int8_t>(store_data);
	break;
      case store_type::sh:
	*((int16_t*)(mem + effective_address)) = accessBigEndian(static_cast<int16_t>(store_data));
	break;
      case store_type::sw:
	*((int32_t*)(mem + effective_address)) = accessBigEndian(store_data);
	break;
      default:
	exit(-1);
      }
    retired = true;
    machine_state.icnt++;
    return true;
  }
  virtual void undo(sim_state &machine_state) {
    dprintf(2, "%s", __PRETTY_FUNCTION__);
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
    machine_state.terminate_sim = true;
    retired = true;
    machine_state.icnt++;
    return true;
  }
  virtual void undo(sim_state &machine_state) {
  }
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
    machine_state.gpr_rat_sanity_check(prf_id);
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
    dprintf(2, "monitor exception @ cycle %lu, complete = %d\n",
	    get_curr_cycle(), m->is_complete);
    m->branch_exception = true;
  }
  virtual void complete(sim_state &machine_state) {
    if(not(m->is_complete) and (get_curr_cycle() == m->complete_cycle)) {
      dprintf(2, "**** monitor marked complete @ cycle %lu\n", get_curr_cycle());
      m->is_complete = true;
    }
  }
  virtual bool retire(sim_state &machine_state) {
    uint32_t reason = ((m->inst >> RSVD_INSTRUCTION_ARG_SHIFT) & RSVD_INSTRUCTION_ARG_MASK) >> 1;
    sparse_mem & mem = *(machine_state.mem);
    switch(reason)
      {
      case 55:
	*((uint32_t*)(mem + (uint32_t)src_regs[0] + 0)) = accessBigEndian(K1SIZE);
	/* No Icache */
	*((uint32_t*)(mem + (uint32_t)src_regs[0] + 4)) = 0;
	/* No Dcache */
	*((uint32_t*)(mem + (uint32_t)src_regs[0] + 8)) = 0;
	break;
      default:
	dprintf(2, "execute monitor op with reason %u\n", reason);
	exit(-1);
      }
    /* not valid until after this instruction retires */
    machine_state.gpr_valid.set_bit(m->prf_idx);
    machine_state.gpr_freevec.clear_bit(m->prev_prf_idx);
    if(machine_state.gpr_rat_sanity_check(m->prev_prf_idx)) {
      dprintf(2, "mapping still exists!..%x\n", m->pc);      
    }
    dprintf(2, "==> execute monitor op with reason %u, return address %x\n",
	    reason, src_regs[3]);
    retired = true;
    machine_state.icnt++;
    return true;
  }
  virtual void undo(sim_state &machine_state) {
    dprintf(2, "%s", __PRETTY_FUNCTION__);
    machine_state.gpr_rat[get_dest()] = m->prev_prf_idx;
    machine_state.gpr_freevec.clear_bit(m->prf_idx);
    machine_state.gpr_valid.clear_bit(m->prf_idx);
  }
};

class rtype_const_shift_alu_op : public rtype_alu_op {
public:
  rtype_const_shift_alu_op(sim_op op) : rtype_alu_op(op) {}
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
	  dprintf(2, "unknown branch type @ %x\n", m_op->pc);
	  exit(-1);
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
      return new rtype_const_shift_alu_op(m_op);
#if 0
    case 0x01: /* movci */
      _movci(inst,s);
      break;
#endif
    case 0x02: /* srl */
      return new rtype_const_shift_alu_op(m_op);
    case 0x03: /* sra */
      return new rtype_const_shift_alu_op(m_op);
    case 0x04: /* sllv */
      return new rtype_alu_op(m_op);
    case 0x05:
      return new monitor_op(m_op);
    case 0x06:
      return new rtype_alu_op(m_op);
    case 0x07:
      return new rtype_alu_op(m_op);
    case 0x08: /* jr */
      return new jump_op(m_op, jump_op::jump_type::jr);
    case 0x09: /* jalr */
      return new jump_op(m_op, jump_op::jump_type::jalr);
#if 0
    case 0x0C: /* syscall */
      printf("syscall()\n");
      exit(-1);
      break;
#endif
    case 0x0D: /* break */
      return new break_op(m_op);
#if 0 
    case 0x0f: /* sync */
      s->pc += 4;
      break;
    case 0x10: /* mfhi */
      s->gpr[rd] = s->hi;
      s->pc += 4;
      break;
    case 0x11: /* mthi */ 
      s->hi = s->gpr[rs];
      s->pc += 4;
      break;
    case 0x12: /* mflo */
      s->gpr[rd] = s->lo;
      s->pc += 4;
      break;
    case 0x13: /* mtlo */
      s->lo = s->gpr[rs];
      s->pc += 4;
      break;
    case 0x18: { /* mult */
      int64_t y;
      y = (int64_t)s->gpr[rs] * (int64_t)s->gpr[rt];
      s->lo = (int32_t)(y & 0xffffffff);
      s->hi = (int32_t)(y >> 32);
      s->pc += 4;
      break;
    }
    case 0x19: { /* multu */
      uint64_t y;
      uint64_t u0 = (uint64_t)*((uint32_t*)&s->gpr[rs]);
      uint64_t u1 = (uint64_t)*((uint32_t*)&s->gpr[rt]);
      y = u0*u1;
      *((uint32_t*)&(s->lo)) = (uint32_t)y;
      *((uint32_t*)&(s->hi)) = (uint32_t)(y>>32);
      s->pc += 4;
      break;
    }
    case 0x1A: /* div */
      if(s->gpr[rt] != 0) {
	s->lo = s->gpr[rs] / s->gpr[rt];
	s->hi = s->gpr[rs] % s->gpr[rt];
      }
      s->pc += 4;
      break;
    case 0x1B: /* divu */
      if(s->gpr[rt] != 0) {
	s->lo = (uint32_t)s->gpr[rs] / (uint32_t)s->gpr[rt];
	s->hi = (uint32_t)s->gpr[rs] % (uint32_t)s->gpr[rt];
      }
      s->pc += 4;
      break;
#endif
    case 0x20: /* add */
      return new rtype_alu_op(m_op);
    case 0x21: 
      return new rtype_alu_op(m_op);
    case 0x22: /* sub */
      return new rtype_alu_op(m_op);
    case 0x23: /*subu*/  
      return new rtype_alu_op(m_op);
    case 0x24: /* and */
      return new rtype_alu_op(m_op);
    case 0x25: /* or */
      return new rtype_alu_op(m_op);
    case 0x26: /* xor */
      return new rtype_alu_op(m_op);
    case 0x27: /* nor */
      return new rtype_alu_op(m_op);
    case 0x2A: /* slt */
      return new rtype_alu_op(m_op);
    case 0x2B:  /* sltu */
      return new rtype_alu_op(m_op);
#if 0
    case 0x0B: /* movn */
      s->gpr[rd] = (s->gpr[rt] != 0) ? s->gpr[rs] : s->gpr[rd];
      s->pc +=4;
      break;
    case 0x0A: /* movz */
      s->gpr[rd] = (s->gpr[rt] == 0) ? s->gpr[rs] : s->gpr[rd];
      s->pc += 4;
      break;
    case 0x34: /* teq */
      if(s->gpr[rs] == s->gpr[rt]) {
	printf("teq trap!!!!!\n");
	exit(-1);
      }
      s->pc += 4;
      break;
#endif
    default:
      dprintf(2,"unknown RType instruction @ %x\n", m_op->pc); 
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
#if 0
    switch(nd_tf)
      {
	case 0x0:
	  _bc1f(inst, s);
	  break;
	case 0x1:
	  _bc1t(inst, s);
	  break;
	case 0x2:
	  _bc1fl(inst, s);
	  break;
	case 0x3:
	  _bc1tl(inst, s);
	  break;
	}
#endif
    return nullptr;
  }
  else if((lowbits == 0) && ((functField==0x0) || (functField==0x4))) {
    if(functField == 0x0)
      return new mfc1(m_op);
    else if(functField == 0x4)
      return new mtc1(m_op);
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
    return nullptr;
  else if(isSpecial3)
    return nullptr;
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
  dprintf(2, "allocate must be implemented\n");
  exit(-1);
  return false;
}

void mips_op::execute(sim_state &machine_state) {
  dprintf(2, "execute must be implemented, pc %x\n", m->pc);
  exit(-1);
}

void mips_op::complete(sim_state &machine_state) {
  dprintf(2, "complete must be implemented, pc %x\n", m->pc);
  exit(-1);
}

bool mips_op::retire(sim_state &machine_state) {
  dprintf(2, "retire must be implemented, pc %x\n", m->pc);
  return false;
}

bool mips_op::ready(sim_state &machine_state) const {
  dprintf(2, "ready must be implemented\n");
  return false;
}

void mips_op::undo(sim_state &machine_state) {
  dprintf(2, "implement %x for undo\n", m->pc);
  exit(-1);
}
