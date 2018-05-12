#include "mips_op.hh"

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
  virtual void allocate(sim_state &machine_state) {
    m->src0_prf = machine_state.gpr_rat[get_src0()];
    m->prev_prf_idx = machine_state.cpr0_rat[get_dest()];
    int64_t prf_id = machine_state.cpr0_freevec.find_first_unset();
    assert(prf_id >= 0);
    machine_state.cpr0_freevec.set_bit(prf_id);
    machine_state.cpr0_rat[get_dest()] = prf_id;
    m->prf_idx = prf_id;
    machine_state.cpr0_valid.clear_bit(prf_id);
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
    return true;
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
  virtual void allocate(sim_state &machine_state) {
    m->src0_prf = machine_state.gpr_rat[get_src0()];
    m->prev_prf_idx = machine_state.cpr1_rat[get_dest()];
    int64_t prf_id = machine_state.cpr1_freevec.find_first_unset();
    assert(prf_id >= 0);
    machine_state.cpr1_freevec.set_bit(prf_id);
    machine_state.cpr1_rat[get_dest()] = prf_id;
    m->prf_idx = prf_id;
    dprintf(2,"%x : mtc1 alloc'd new dest -> %d\n",
	    m->pc, prf_id);
    machine_state.cpr1_valid.clear_bit(prf_id);

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
      dprintf(2, "%x : m->prf_idx = %d\n", m->pc, m->prf_idx);
      machine_state.cpr1_valid.set_bit(m->prf_idx);
    }
  }
  virtual bool retire(sim_state &machine_state) {
    machine_state.cpr1_freevec.clear_bit(m->prev_prf_idx);
    return true;
  }
  virtual void undo(sim_state &machine_state) {
    if(m->prev_prf_idx != -1) {
      machine_state.cpr1_rat[get_dest()] = m->prev_prf_idx;
    }
    if(m->prf_idx != -1) {
      machine_state.cpr1_freevec.clear_bit(m->prf_idx);
    }
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
  virtual void allocate(sim_state &machine_state) {
    m->src0_prf = machine_state.cpr1_rat[get_src0()];
    m->prev_prf_idx = machine_state.gpr_rat[get_dest()];
    int64_t prf_id = machine_state.gpr_freevec.find_first_unset();
    assert(prf_id >= 0);
    machine_state.gpr_freevec.set_bit(prf_id);
    machine_state.gpr_rat[get_dest()] = prf_id;
    m->prf_idx = prf_id;
    machine_state.gpr_valid.clear_bit(prf_id);

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
    return true;
  }
  virtual void undo(sim_state &machine_state) {
    if(m->prev_prf_idx != -1) {
      machine_state.gpr_rat[get_dest()] = m->prev_prf_idx;
    }
    if(m->prf_idx != -1) {
      machine_state.gpr_freevec.clear_bit(m->prf_idx);
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
  virtual void allocate(sim_state &machine_state) {
    if(get_src0() != -1) {
      m->src0_prf = machine_state.gpr_rat[get_src0()];
    }
    if(get_src1() != -1) {
      m->src1_prf = machine_state.gpr_rat[get_src1()];
    }
    if(get_dest() > 0) {
      m->prev_prf_idx = machine_state.gpr_rat[get_dest()];
      int64_t prf_id = machine_state.gpr_freevec.find_first_unset();
      assert(prf_id >= 0);
      machine_state.gpr_freevec.set_bit(prf_id);
      machine_state.gpr_rat[get_dest()] = prf_id;
      m->prf_idx = prf_id;
      machine_state.gpr_valid.clear_bit(prf_id);
    }
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
    uint32_t funct = m->inst & 63;
    uint32_t sa = (m->inst >> 6) & 31;
    switch(funct)
      {
      case 0x00: /*sll*/
	machine_state.gpr_prf[m->prf_idx] = machine_state.gpr_prf[m->src0_prf] << sa;
	break;
      default:
	dprintf(2, "wtf is funct %x\n", funct);
	exit(-1);
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
    }
    return true;
  }
  virtual void undo(sim_state &machine_state) {
    if(get_dest() > 0) {
      if(m->prev_prf_idx != -1) {
	machine_state.gpr_rat[get_dest()] = m->prev_prf_idx;
      }
      if(m->prf_idx != -1) {
	machine_state.gpr_freevec.clear_bit(m->prf_idx);
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
  virtual void allocate(sim_state &machine_state) {
    if(get_src0() != -1) {
      m->src0_prf = machine_state.gpr_rat[get_src0()];
    }
    if(get_dest() > 0) {
      m->prev_prf_idx = machine_state.gpr_rat[get_dest()];
      int64_t prf_id = machine_state.gpr_freevec.find_first_unset();
      assert(prf_id >= 0);
      machine_state.gpr_freevec.set_bit(prf_id);
      machine_state.gpr_rat[get_dest()] = prf_id;
      m->prf_idx = prf_id;
      machine_state.gpr_valid.clear_bit(prf_id);
    }
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
      case 0x0d: /* ori */
	machine_state.gpr_prf[m->prf_idx] = machine_state.gpr_prf[m->src0_prf] | uimm32;
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
      machine_state.gpr_valid.set_bit(m->prf_idx);
    }
  }
  virtual bool retire(sim_state &machine_state) {
    machine_state.gpr_freevec.clear_bit(m->prev_prf_idx);
    return true;
  }
  virtual void undo(sim_state &machine_state) {
    if(get_dest() > 0) {
      if(m->prev_prf_idx != -1) {
	machine_state.gpr_rat[get_dest()] = m->prev_prf_idx;
      }
      if(m->prf_idx != -1) {
	machine_state.gpr_freevec.clear_bit(m->prf_idx);
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
protected:
  itype i_;
  uint32_t branch_target = 0;
public:
  jump_op(sim_op op) : mips_op(op), i_(op->inst) {
    this->op_class = mips_op_type::jmp;
    op->has_delay_slot = true;
  }
  virtual int get_dest() const {
    uint32_t funct = m->inst & 63;
    return funct==0x09 ? 31 : -1;
  }
  virtual int get_src0() const {
    return i_.ii.rs;
  }
  virtual void allocate(sim_state &machine_state) {
    dprintf(2, "jump allocated\n");
    if(get_src0() != -1) {
      m->src0_prf = machine_state.gpr_rat[get_src0()];
    }
    if(get_dest() != -1) {
      m->prev_prf_idx = machine_state.gpr_rat[get_dest()];
      int64_t prf_id = machine_state.gpr_freevec.find_first_unset();
      assert(prf_id >= 0);
      machine_state.gpr_freevec.set_bit(prf_id);
      machine_state.gpr_rat[get_dest()] = prf_id;
      m->prf_idx = prf_id;
      machine_state.gpr_valid.clear_bit(prf_id);
    }
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
    m->correct_pc = machine_state.gpr_prf[m->src0_prf];
    if(get_dest() != -1) {
      machine_state.gpr_prf[m->prf_idx] = m->pc + 8;
    }
    m->complete_cycle = get_curr_cycle() + 1;
  }
  virtual bool retire(sim_state &machine_state) {
    if(m->prev_prf_idx != -1) {
      machine_state.gpr_freevec.clear_bit(m->prev_prf_idx);
    }
    return true;
  }
  virtual void undo(sim_state &machine_state) {
    if(get_dest() != -1) {
      if(m->prev_prf_idx != -1) {
	machine_state.gpr_rat[get_dest()] = m->prev_prf_idx;
      }
      if(m->prf_idx != -1) {
	machine_state.gpr_freevec.clear_bit(m->prf_idx);
      }
    }
  }
};

class branch_op : public mips_op {
protected:
  itype i_;
  bool take_br = false;
  uint32_t branch_target = 0;
public:
  branch_op(sim_op op) : mips_op(op), i_(op->inst), take_br(false) {
    this->op_class = mips_op_type::jmp;
    int16_t himm = (int16_t)(m->inst & ((1<<16) - 1));
    int32_t imm = ((int32_t)himm) << 2;
    uint32_t npc = m->pc+4;
    branch_target = (imm+npc);
    op->has_delay_slot = true;
    op->correct_pc = branch_target;
  }
  virtual int get_src0() const {
    return i_.ii.rt;
  }
  virtual int get_src1() const {
    return i_.ii.rs;
  }
  virtual void allocate(sim_state &machine_state) {
    dprintf(2, "branch allocated\n");
    if(get_src0() != -1) {
      m->src0_prf = machine_state.gpr_rat[get_src0()];
    }
    if(get_src1() != -1) {
      m->src1_prf = machine_state.gpr_rat[get_src1()];
    }
  }
  virtual bool ready(sim_state &machine_state) const {
    if(m->src0_prf != -1 and not(machine_state.gpr_valid.get_bit(m->src0_prf))) {
      dprintf(2, "branch src0 not ready\n");
      return false;
    }
    if(m->src1_prf != -1 and not(machine_state.gpr_valid.get_bit(m->src1_prf))) {
      dprintf(2, "branch src1 not ready\n");
      return false;
    }
    return true;
  }
  virtual void execute(sim_state &machine_state) {
    uint32_t opcode = (m->inst)>>26;
    dprintf(2, "src0 %d, src1 %d\n", m->src0_prf, m->src1_prf);
    dprintf(2, "src0 value %d\n",machine_state.gpr_prf[m->src0_prf]);
    dprintf(2, "src1 value %d\n",machine_state.gpr_prf[m->src1_prf]);
    
    switch(opcode)
      {
      case 0x04:
	//_beq(inst, s);
	take_br = machine_state.gpr_prf[m->src0_prf] == machine_state.gpr_prf[m->src1_prf];
	break;
      case 0x05:
	//_bne(inst, s);
	take_br = machine_state.gpr_prf[m->src0_prf] != machine_state.gpr_prf[m->src1_prf];
	break;
#if 0
      case 0x06:
	//_blez(inst, s); 
	break;
      case 0x07:
	//_bgtz(inst, s); 
	break;
#endif
      default:
	dprintf(2, "wtf @ %x\n", m->pc);
	exit(-1);
      }

    dprintf(2, "take_br = %d\n", take_br);
    if(take_br != m->predict_taken) {

      m->branch_exception = true;
    }
    
    m->complete_cycle = get_curr_cycle() + 1;
  }
  virtual void complete(sim_state &machine_state) {
    if(not(m->is_complete) and (get_curr_cycle() == m->complete_cycle)) {
      m->is_complete = true;
    }
  }
  
};

class rtype_const_shift_alu_op : public rtype_alu_op {
public:
  rtype_const_shift_alu_op(sim_op op) : rtype_alu_op(op) {}
  virtual int get_src1() const {
    return -1;
  }
};


static mips_op* decode_itype_insn(sim_op m_op) {
  uint32_t opcode = (m_op->inst)>>26;
  switch(opcode)
    {
    case 0x04: /* beq */
      return new branch_op(m_op);
    case 0x05: /* bne */
      return new branch_op(m_op);
    case 0x6: /* blez */
      return new branch_op(m_op);
    case 0x7: /* bgtz */
      return new branch_op(m_op);
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
#if 0
    case 0x05:
      _monitor(inst,s);
      break;
#endif
    case 0x06:
      return new rtype_alu_op(m_op);
    case 0x07:
      return new rtype_alu_op(m_op);
    case 0x08: /* jr */
      return new jump_op(m_op);
    case 0x09: /* jalr */
      return new jump_op(m_op);
#if 0
    case 0x0C: /* syscall */
      printf("syscall()\n");
      exit(-1);
      break;
    case 0x0D: /* break */
      s->brk = 1;
      break;
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
      dprintf(2,"unknown RType instruction\n"); 
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


  if(isRType) {
    return decode_rtype_insn(m_op);
  }
  else if(isSpecial2)
    return nullptr;
  else if(isSpecial3)
    return nullptr;
  else if(isJType)
    return nullptr;
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

void mips_op::allocate(sim_state &machine_state) {
  dprintf(2, "allocate must be implemented\n");
  exit(-1);
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
