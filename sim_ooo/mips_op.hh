#ifndef __mipsop_hh__
#define __mipsop_hh__

#include <memory>
#include <vector>
#include <list>
#include <cassert>
#include "globals.hh"
#include "sparse_mem.hh"
#include "sim_queue.hh"
#include "sim_bitvec.hh"
#include "sim_list.hh"
#include "sim_stack.hh"
#include "mips_encoding.hh"
#include "branch_predictor.hh"
#include "loop_predictor.hh"
#include "counter2b.hh"
#include "perceptron.hh"


enum class mips_op_type { unknown, alu, fp, jmp, load, store, system };

enum class exception_type {none, load, branch};

struct state_t;
class sim_state;
class simCache;


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

class mips_meta_op {
public:
  uint32_t pc = 0;
  uint32_t inst = 0;
  uint32_t fetch_npc = 0;  
  bool predict_taken = false;
  int32_t prediction = 0;
  uint64_t pht_idx = 0;
  bool pop_return_stack = false;
  int64_t alloc_id = -1;
  int64_t fetch_cycle = -1;
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
  exception_type exception = exception_type::none;
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
  int32_t src0_prf = -1, src1_prf = -1, src2_prf = -1, src3_prf = -1, src4_prf = -1, src5_prf = -1;
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
    prediction = 0;
    pht_idx = 0;
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
    exception = exception_type::none;
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
    src5_prf = -1;
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
  virtual void log_retire(sim_state &machine_state) const;
  virtual void log_undo(sim_state &machine_state) const;
  virtual int64_t get_latency() const;
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
