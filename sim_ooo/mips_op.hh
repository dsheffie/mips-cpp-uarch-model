#ifndef __mipsop_hh__
#define __mipsop_hh__

#include <memory>
#include <vector>
#include <list>
#include <cassert>
#include "sparse_mem.hh"
#include "sim_queue.hh"
#include "sim_bitvec.hh"
#include "sim_list.hh"
#include "sim_stack.hh"
#include "mips_encoding.hh"

uint64_t get_curr_cycle();

enum class mips_op_type { unknown, alu, fp, jmp, load, store, system };

inline bool is_jr(uint32_t inst) {
  uint32_t opcode = inst>>26;
  uint32_t funct = inst & 63;
  return (opcode==0) and (funct == 0x08);
}

inline bool is_jal(uint32_t inst) {
  uint32_t opcode = inst>>26;
  return (opcode == 3);
}

inline bool is_j(uint32_t inst) {
  uint32_t opcode = inst>>26;
  return (opcode == 2);
}

inline uint32_t get_jump_target(uint32_t pc, uint32_t inst) {
  assert(is_jal(inst) or is_j(inst));
  static const uint32_t pc_mask = (~((1U<<28)-1));
  uint32_t jaddr = (inst & ((1<<26)-1)) << 2;
  return ((pc + 4)&pc_mask) | jaddr;
}

inline bool is_branch(uint32_t inst) {
  uint32_t opcode = inst>>26;
  switch(opcode)
    {
    case 0x01:
    case 0x04:
    case 0x05:
    case 0x06:
    case 0x07:
      return true;
    default:
      break;
    }
  return false;
}

inline bool is_likely_branch(uint32_t inst) {
  uint32_t opcode = inst>>26;
  switch(opcode)
    {
    case 0x14:
    case 0x16:
    case 0x15:
    case 0x17:
      return true;
    default:
      break;
    }
  return false;
}

inline uint32_t get_branch_target(uint32_t pc, uint32_t inst) {
  int16_t himm = (int16_t)(inst & ((1<<16) - 1));
  int32_t imm = ((int32_t)himm) << 2;
  return  pc+4+imm; 
}

inline std::ostream &operator<<(std::ostream &out, mips_op_type ot) {
  switch(ot)
    {
    case mips_op_type::unknown:
      out << "unknown";
      break;
    case mips_op_type::alu:
      out << "alu";
      break;
    case mips_op_type::fp:
      out << "fp";
      break;
    case mips_op_type::jmp:
      out << "jmp";
      break;
    case mips_op_type::load:
      out << "load";
      break;
    case mips_op_type::store:
      out << "store";
      break;
    case mips_op_type::system:
      out << "system";
      break;
    }
  return out;
}

class mips_op;

struct mips_meta_op {
  uint32_t pc = 0;
  uint32_t inst = 0;
  uint32_t fetch_npc = 0;  
  uint64_t fetch_cycle = 0;
  bool predict_taken = false;
  bool pop_return_stack = false;
  int64_t alloc_id = -1;
  int64_t decode_cycle = -1;
  int64_t alloc_cycle = -1;
  int64_t ready_cycle = -1;
  int64_t dispatch_cycle = -1;
  int64_t complete_cycle = -1;
  int64_t retire_cycle = -1;
  int64_t aux_cycle = -1;
  /* finished execution */
  bool is_complete = false;
  bool could_cause_exception = false;
  bool branch_exception = false;
  bool load_exception = false;
  bool is_branch_or_jump = false;
  bool is_jal = false, is_jr = false;
  bool has_delay_slot = false;
  bool likely_squash = false;
  uint32_t correct_pc = 0;
  bool is_store = false, is_fp_store = false;

  int32_t rob_idx = -1;
  /* result will get written to prf idx */
  int32_t prf_idx = -1, aux_prf_idx=-1;
  /* previous prf writer (will return to freelist when this inst retires */
  int32_t prev_prf_idx = -1, aux_prev_prf_idx = -1;
  int32_t src0_prf = -1, src1_prf = -1, src2_prf = -1, src3_prf = -1, src4_prf = -1;
  int32_t load_tbl_idx = -1, store_tbl_idx = -1;
  int32_t hi_prf_idx = -1, lo_prf_idx = -1;
  int32_t prev_hi_prf_idx = -1, prev_lo_prf_idx = -1;
  
  mips_op* op = nullptr;
  bool push_return_stack = false;
  int64_t return_stack_idx = -1;

  void reinit(uint32_t pc,
	      uint32_t inst,
	      uint32_t fetch_cycle) {
    this->pc = pc;
    this->inst = inst;
    this->fetch_cycle = fetch_cycle;

    fetch_npc = 0;  
    predict_taken = false;
    pop_return_stack = false;
    alloc_id = -1;
    decode_cycle = -1;
    alloc_cycle = -1;
    ready_cycle = -1;
    dispatch_cycle = -1;
    complete_cycle = -1;
    retire_cycle = -1;
    aux_cycle = -1;
    is_complete = false;
    could_cause_exception = false;
    branch_exception = false;
    load_exception = false;
    is_branch_or_jump = false;
    is_jal = false;
    is_jr = false;
    has_delay_slot = false;
    likely_squash = false;
    correct_pc = 0;
    is_store = false;
    is_fp_store = false;

    rob_idx = -1;
    prf_idx = -1;
    aux_prf_idx=-1;
    prev_prf_idx = -1;
    aux_prev_prf_idx = -1;
    src0_prf = -1;
    src1_prf = -1;
    src2_prf = -1;
    src3_prf = -1;
    src4_prf = -1;
    load_tbl_idx = -1;
    store_tbl_idx = -1;
    hi_prf_idx = -1;
    lo_prf_idx = -1;
    prev_hi_prf_idx = -1;
    prev_lo_prf_idx = -1;

    op = nullptr;
    return_stack_idx = -1;
    push_return_stack = false;
  }

  mips_meta_op(uint32_t pc,
	       uint32_t inst,
	       uint32_t fetch_cycle) :
    pc(pc), inst(inst),
    fetch_npc(0),
    fetch_cycle(fetch_cycle),
    predict_taken(false),
    pop_return_stack(false)  {}
  
  mips_meta_op(uint32_t pc,
	       uint32_t inst,
	       uint32_t fetch_npc,
	       uint32_t fetch_cycle,
	       bool predict_taken,
	       bool pop_return_stack) :
    pc(pc), inst(inst),
    fetch_npc(fetch_npc),
    fetch_cycle(fetch_cycle),
    predict_taken(predict_taken),
    pop_return_stack(pop_return_stack)  {}
  
  void release();
  ~mips_meta_op();
};

typedef mips_meta_op* sim_op;


class branch_table {
protected:
  struct btb_entry {
    uint32_t pc;
  };
};

struct state_t;
class simCache;

struct sim_state {
  bool terminate_sim = false;
  bool nuke = false;
  uint32_t delay_slot_npc = 0;
  uint32_t fetch_pc = 0;

  uint64_t last_retire_cycle = 0;
  uint32_t last_retire_pc = 0;

  uint32_t last_compare_pc = 0;
  uint64_t last_compare_icnt = 0;
  
  /* hi and lo in grf too */
  int32_t gpr_rat[34];
  int32_t cpr0_rat[32];
  int32_t cpr1_rat[32];
  int32_t fcr1_rat[5];

  int num_gpr_prf_ = -1;
  int num_cpr0_prf_ = -1;
  int num_cpr1_prf_ = -1;
  int num_fcr1_prf_ = -1;
  
  int32_t arch_grf[34] = {0};
  uint32_t arch_grf_last_pc[34] = {0};

  uint32_t arch_cpr1[32] = {0};
  uint32_t arch_cpr1_last_pc[32] = {0};

  uint32_t arch_fcr1[5] = {0};
  uint32_t arch_fcr1_last_pc[5] = {0};
  
  int32_t *gpr_prf = nullptr;
  uint32_t *cpr0_prf = nullptr;
  uint32_t *cpr1_prf = nullptr;
  uint32_t *fcr1_prf = nullptr;

  mips_meta_op **load_tbl = nullptr;
  mips_meta_op **store_tbl = nullptr;
  
  sim_bitvec gpr_freevec;
  sim_bitvec cpr0_freevec;
  sim_bitvec cpr1_freevec;
  sim_bitvec fcr1_freevec;
  sim_bitvec load_tbl_freevec;
  sim_bitvec store_tbl_freevec;
  
  sim_bitvec gpr_valid;
  sim_bitvec cpr0_valid;
  sim_bitvec cpr1_valid;
  sim_bitvec fcr1_valid;

  sim_queue<sim_op> fetch_queue;
  sim_queue<sim_op> decode_queue;
  sim_queue<sim_op> rob;

  sim_bitvec alu_alloc;
  sim_bitvec fpu_alloc;
  sim_bitvec load_alloc;
  sim_bitvec store_alloc;
  
  int num_alu_rs = -1;
  int num_fpu_rs = -1;
  int num_load_rs = -1;
  int num_store_rs = -1;
  
  typedef sim_list<sim_op> rs_type;
  std::vector<rs_type> alu_rs;
  std::vector<rs_type> fpu_rs;
  rs_type jmp_rs;
  std::vector<rs_type> load_rs;
  std::vector<rs_type> store_rs;
  rs_type system_rs;
  
  sparse_mem *mem = nullptr;
  
  uint64_t icnt = 0, maxicnt = ~(0UL), skipicnt = 0;
  uint64_t n_branches = 0, n_jumps = 0;
  uint64_t mispredicted_branches = 0;
  uint64_t mispredicted_jumps = 0;
  uint64_t mispredicted_jrs = 0;
  uint64_t mispredicted_jalrs = 0;
  uint64_t nukes = 0, branch_nukes = 0, load_nukes = 0;

  uint64_t total_ready_insns = 0;
  uint64_t total_allocated_insns = 0;
  uint64_t total_dispatched_insns = 0;
  uint64_t total_sched_insns = 0;
  
  sim_stack_template<uint32_t> return_stack;

  state_t *ref_state = nullptr;
  
  sparse_mem *oracle_mem = nullptr;
  state_t *oracle_state = nullptr;

  simCache *l1d = nullptr;

  
  bool log_execution = false;
  bool use_interp_check = false;

  void initialize_rat_mappings();
  void initialize();
  void copy_state(const state_t *s);

  ~sim_state() {
    if(gpr_prf) delete [] gpr_prf;
    if(cpr0_prf) delete [] cpr0_prf;
    if(cpr1_prf) delete [] cpr1_prf;
    if(fcr1_prf) delete [] fcr1_prf;
    if(load_tbl) delete [] load_tbl;
    if(store_tbl) delete [] store_tbl;
  }
};


class mips_op {
protected:
  bool getConditionCode(uint32_t cr, uint32_t cc) {
    return ((cr & (1U<<cc)) >> cc) & 0x1;
  }
public:
  sim_op m = nullptr;
  bool retired = false;
  mips_op_type op_class = mips_op_type::unknown;
  mips_op(sim_op m) : m(m), retired(false) {}
  virtual ~mips_op() {}
  virtual bool allocate(sim_state &machine_state);
  virtual void execute(sim_state &machine_state);
  virtual void complete(sim_state &machine_state);
  virtual bool retire(sim_state &machine_state);
  virtual bool ready(sim_state &machine_state) const;
  virtual void undo(sim_state &machine_state);
  virtual bool stop_sim() const {
    return false;
  }
  virtual int get_dest() const {
    return -1;
  }
  virtual int get_src0() const {
    return -1;
  }
  virtual int get_src1() const {
    return -1;
  }
  virtual int get_src2() const {
    return -1;
  }
  virtual int get_src3() const {
    return -1;
  }
  
  mips_op_type get_op_class() const {
    return op_class;
  }
};

class mips_store : public mips_op {
protected:
  itype i_;
  int32_t imm = -1;
  uint32_t effective_address = ~0;
public:
  mips_store(sim_op op) : mips_op(op), i_(op->inst) {
    this->op_class = mips_op_type::store;
    int16_t himm = static_cast<int16_t>(m->inst & ((1<<16) - 1));
    imm = static_cast<int32_t>(himm);
    op->is_store = true;
  }
};

class mips_load : public mips_op {
public:
  enum class load_type {lb,lbu,lh,lhu,lw,ldc1,lwc1,lwl,lwr,bogus};
protected:
  itype i_;
  load_type lt;
  int32_t imm = -1;
  uint32_t effective_address = ~0;
  bool stall_for_load(sim_state &machine_state) const;
public:
  mips_load(sim_op op) : mips_op(op), i_(op->inst), lt(load_type::bogus) {
    this->op_class = mips_op_type::load;
    int16_t himm = static_cast<int16_t>(m->inst & ((1<<16) - 1));
    imm = static_cast<int32_t>(himm);
    op->could_cause_exception = true;
  }
  uint32_t getEA() const {
    return effective_address;
  }
};

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

std::ostream &operator<<(std::ostream &out, const mips_op &op);

mips_op* decode_insn(sim_op m_op);


#endif
