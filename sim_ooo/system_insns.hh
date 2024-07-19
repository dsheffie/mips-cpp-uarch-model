#include "temu_code.hh"

static void set_mstatus(sim_state *s, int64_t v) {
  int64_t mask = MSTATUS_MASK;
  s->mstatus = (s->mstatus & ~mask) | (v & mask);
}

static int64_t read_csr(int csr_id, const sim_state *s, bool &undef) {
  undef = false;
  switch(csr_id)
    {
    case 0x100:
      return s->mstatus & 0x3000de133UL;
    case 0x104:
      return s->mie & s->mideleg;
    case 0x105:
      return s->stvec;
    case 0x140:
      return s->sscratch;
    case 0x141:
      return s->sepc;
    case 0x142:
      return s->scause;
    case 0x143:
      return s->stval;
    case 0x144:
      return s->mip & s->mideleg;
    case 0x180:
      return s->satp;
    case 0x300:
      return s->mstatus;
    case 0x301: /* misa */
      return s->misa;
    case 0x302:
      return s->medeleg;
    case 0x303:
      return s->mideleg;
    case 0x304:
      return s->mie;
    case 0x305:
      return s->mtvec;
    case 0x340:
      return s->mscratch;
    case 0x341:
      return s->mepc;
    case 0x342:
      return s->mcause;
    case 0x343:
      return s->mtvec;
    case 0x344:
      return s->mip;      
    case 0x3b0:
      return s->pmpaddr0;
    case 0x3b1:
      return s->pmpaddr1;      
    case 0x3b2:
      return s->pmpaddr2;
    case 0x3b3:
      return s->pmpaddr3;      
    case 0xc00:
      return s->icnt;
    case 0xc01:
      die();
      return -1;//s->get_time();
    case 0xc03:
      return 0;      
    case 0xf14:
      return s->mhartid;      
    default:
      undef = true;
      die();
      break;
    }
  return 0;
}


static void write_csr(int csr_id, sim_state *s, int64_t v, bool &undef) {
  undef = false;
  riscv::csr_t c(v);
  switch(csr_id)
    {
    case 0x100:
      //printf("%lx writes %lx, old %lx\n", s->pc, v, s->mstatus);
      s->mstatus = (v & 0x000de133UL) | ((s->mstatus & (~0x000de133UL)));
      break;
    case 0x104:
      s->mie = (s->mie & ~(s->mideleg)) | (v & s->mideleg);
      break;
    case 0x105:
      s->stvec = v;
      break;
    case 0x106:
      s->scounteren = v;
      break;
    case 0x140:
      s->sscratch = v;
      break;
    case 0x141:
      s->sepc = v;
      break;
    case 0x142:
      s->scause = v;
      break;
    case 0x143:
      s->stvec = v;
      break;
    case 0x144:
      s->mip = (s->mip & ~(s->mideleg)) | (v & s->mideleg);      
      break;
    case 0x180:
      if(c.satp.mode == 8 &&
	 c.satp.asid == 0) {
	s->satp = v;
	//printf("tlb had %lu entries\n", tlb.size());
	//tlb.clear();
	die();
      }
      break;
    case 0x300:
      set_mstatus(s,v);
      break;
    case 0x301:
      s->misa = v;
      break;
    case 0x302:
      s->medeleg = v;
      break;
    case 0x303:
      s->mideleg = v;
      break;
    case 0x304:
      s->mie = v;
      break;
    case 0x305:
      s->mtvec = v;
      break;
    case 0x306:
      s->mcounteren = v;
      break;
    case 0x340:
      s->mscratch = v;
      break;
    case 0x341:
      s->mepc = v;
      break;
    case 0x344:
      s->mip = v;
      break;
    case 0x3a0:
      s->pmpcfg0 = v;
      break;
    case 0x3b0:
      s->pmpaddr0 = v;
      break;
    case 0x3b1:
      s->pmpaddr1 = v;
      break;
    case 0x3b2:
      s->pmpaddr2 = v;
      break;
    case 0x3b3:
      s->pmpaddr3 = v;
      break;

      /* linux hacking */
    case 0xc03:
      std::cout << (char)(v&0xff);
      break;
    case 0xc04:
      //s->brk = v&1;
      //if(s->brk) {
      //std::cout << "you have panicd linux, game over\n";
      //}
      break;
    default:
      printf("wr csr id 0x%x unimplemented\n", csr_id);
      undef = true;
      break;
    }
}


class csrrx_op : public riscv_op {
public:
  csrrx_op(sim_op op) : riscv_op(op) {
    this->op_class = oper_type::system;
    op->could_cause_exception = true;
  }
  bool serialize_and_flush() const override {
    return true;
  }
  int get_dest() const override {
    int d = (m->inst>>7) & 31;
    return d==0 ? -1 : d;
  }
  int get_src0() const override {
    return ((m->inst >> 15) & 31);
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
    machine_state.alloc_blocked = true;
    return true;
  }
  bool ready(sim_state &machine_state) const override  {
    if(m->src1_prf != -1 and not(machine_state.gpr_valid.get_bit(m->src1_prf))) {
      return false;
    }    
    return true;
  }
  void execute(sim_state &machine_state) override {
    m->exception = exception_type::serializing;
    int c = (m->inst>>12) & 7;
    uint32_t csr_id = (m->inst>>20);
    bool undef = false;
    switch(c)
      {
      case 1 : {/* CSRRW */
	int64_t v = 0;
	if(m->prf_idx != -1) {
	  v = read_csr(csr_id, &machine_state, undef);
	  if(undef) {
	    die();
	  }
	}
	printf("writing csr %d with value %llx for pc %llx (case 1)\n", csr_id, machine_state.gpr_prf[m->src0_prf], m->pc);
  	printf("case 1 : pc %llx src0 = %d, rs = %d\n", m->pc, get_src0(), ((m->inst >> 15) & 31));
	write_csr(csr_id, &machine_state, machine_state.gpr_prf[m->src0_prf], undef);
	if(undef) {
	  die();
	}
	if(m->prf_idx != -1) {
	  machine_state.gpr_prf[m->prf_idx] = v;
	}
	break;
      }
      case 2: {/* CSRRS */
	int64_t t = read_csr(csr_id, &machine_state, undef);
	if(undef) {
	  die();
	}
	printf("case 2 : pc %llx src0 = %d, rs = %d\n", m->pc, get_src0(), ((m->inst >> 15) & 31));
	
	if(get_src0()) {
	  printf("writing csr %d with value %llx for pc %llx (case 2)\n", csr_id, t|machine_state.gpr_prf[m->src0_prf], m->pc);
	  write_csr(csr_id, &machine_state, t | machine_state.gpr_prf[m->src0_prf], undef);
	  if(undef) die();
	}
	
	if(m->prf_idx != -1) {
	  machine_state.gpr_prf[m->prf_idx] = t;
	}
	break;
      }
	
      default:
	std::cout << "implement case " << c << "\n";
	die()
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
    log_rollback(machine_state);
  }
};
