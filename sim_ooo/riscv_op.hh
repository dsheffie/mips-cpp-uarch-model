#ifndef __mipsop_hh__
#define __mipsop_hh__

#include <memory>
#include <vector>
#include <list>
#include <cassert>
#include "state.hh"
#include "globals.hh"
#include "sparse_mem.hh"
#include "sim_queue.hh"
#include "sim_bitvec.hh"
#include "sim_list.hh"
#include "sim_stack.hh"

#include "riscv.hh"

#include "branch_predictor.hh"
#include "loop_predictor.hh"
#include "counter2b.hh"
#include "perceptron.hh"


enum class oper_type { unknown, alu, fp, jmp, load, store, system };

enum class exception_type {none, load, branch};

struct state_t;
class sim_state;
class simCache;


class riscv_op;

class meta_op {
public:
  uint64_t fetch_icnt = 0;
  uint64_t pc = 0, fetch_npc = 0;
  uint32_t inst = 0;
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
  uint64_t correct_pc = 0;
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
  
  riscv_op* op = nullptr;
  bool push_return_stack = false;
  int64_t return_stack_idx = -1;

  void reinit(uint64_t pc,
	      uint32_t inst,
	      uint64_t fetch_cycle) {
    fetch_icnt = 0;
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

  meta_op(uint64_t fetch_icnt,
	  uint64_t pc,
	  uint32_t inst,
	  uint64_t fetch_cycle) :
    fetch_icnt(fetch_icnt),
    pc(pc),
    inst(inst),
    fetch_npc(0),
    fetch_cycle(fetch_cycle),
    predict_taken(false),
    pop_return_stack(false)  {}
  
  meta_op(uint64_t fetch_icnt,
	       uint64_t pc,
	       uint32_t inst,
	       uint64_t fetch_npc,
	       uint64_t fetch_cycle,
	       bool predict_taken,
	       bool pop_return_stack) :
    fetch_icnt(fetch_icnt),
    pc(pc),
    inst(inst),
    fetch_npc(fetch_npc),
    fetch_cycle(fetch_cycle),
    predict_taken(predict_taken),
    pop_return_stack(pop_return_stack)  {}
  
  void release();
  ~meta_op();

};

typedef meta_op* sim_op;

class riscv_op {
public:
  sim_op m = nullptr;
  bool retired = false;
  riscv::riscv_t di;
  oper_type op_class = oper_type::unknown;
  riscv_op(sim_op m) : m(m), retired(false), di(m->inst) {}
  virtual ~riscv_op() {}
  virtual bool allocate(sim_state &machine_state);
  virtual void execute(sim_state &machine_state);
  virtual void complete(sim_state &machine_state);
  virtual bool retire(sim_state &machine_state);
  virtual bool ready(sim_state &machine_state) const;
  virtual void rollback(sim_state &machine_state);
  virtual void log_retire(sim_state &machine_state) const;
  virtual void log_rollback(sim_state &machine_state) const;
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
  int count_rd_ports() const {
    return (get_src0()!=-1) + (get_src1()!=-1) + (get_src2()!=-1) + (get_src3()!=-1);
  }
  oper_type get_op_class() const {
    return op_class;
  }
};

class riscv_store : public riscv_op {
public:
  enum class store_type {sb,sh,sw,sd,bogus};
  static constexpr store_type stypes[] = {store_type::sb,store_type::sh,store_type::sw,store_type::sd};
protected:
  store_type st;
  int64_t store_data;
  int64_t imm = -1;
  uint64_t effective_address = ~0;

public:
  riscv_store(sim_op op, store_type st) : riscv_op(op), st(st), store_data(0) {
    this->op_class = oper_type::store;
    int32_t disp = di.s.imm4_0 | (di.s.imm11_5 << 5);
    disp |= ((m->inst>>31)&1) ? 0xfffff000 : 0x0;
    imm =  ((static_cast<int64_t>(disp) << 32) >> 32);
    op->is_store = true;
    op->could_cause_exception = true;
  }
  int get_src0() const override {
    return di.s.rs1;
  }
  int get_src1() const override {
    return di.s.rs2;
  }
  bool allocate(sim_state &machine_state) override;
  bool ready(sim_state &machine_state) const override;
  void execute(sim_state &machine_state) override;
  bool retire(sim_state &machine_state) override;
  int64_t get_latency() const override;
  
};

class riscv_load : public riscv_op {
public:
  enum class load_type {lb,lbu,lh,lhu,lw,ld,bogus};
protected:
  load_type lt;
  int64_t imm = -1;
  uint64_t effective_address = ~0;
  bool stall_for_load(sim_state &machine_state) const;
public:
  riscv_load(sim_op op, load_type lt) : riscv_op(op), lt(lt) {
    this->op_class = oper_type::load;
    op->could_cause_exception = true;
    int32_t disp = di.l.imm11_0;
    if((m->inst>>31)&1) {
      disp |= 0xfffff000;
    }
    imm= ((static_cast<int64_t>(disp) << 32) >> 32);
  }
  uint64_t getEA() const {
    return effective_address;
  }
};

std::ostream &operator<<(std::ostream &out, const riscv_op &op);

riscv_op* decode_insn(sim_op m_op);


#endif
