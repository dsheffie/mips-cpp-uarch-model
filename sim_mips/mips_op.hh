#ifndef __mipsop_hh__
#define __mipsop_hh__

#include <memory>
#include "sim_queue.hh"
#include "sim_bitvec.hh"
#include "mips_encoding.hh"

enum class mips_op_type { unknown, alu, fp, jmp, mem };

class mips_op;

struct mips_meta_op : std::enable_shared_from_this<mips_meta_op> {
  uint32_t pc = 0;
  uint32_t inst = 0;
  uint32_t fetch_npc = 0;  
  uint64_t fetch_cycle = 0;
  uint64_t decode_cycle = 0;
  uint64_t alloc_cycle = 0;
  uint64_t complete_cycle = 0;
  
  /* finished execution */
  bool complete = false;
  int32_t rob_idx = -1;
  /* result will get written to prf idx */
  int32_t prf_idx = -1;
  /* previous prf writer (will return to freelist when this inst retires */
  int32_t prev_prf_idx = -1;

  mips_op* op = nullptr;
  
  mips_meta_op(uint32_t pc, uint32_t inst,  uint32_t fetch_npc, uint32_t fetch_cycle) :
    pc(pc), inst(inst), fetch_npc(fetch_npc), fetch_cycle(fetch_cycle) {}

  ~mips_meta_op();
  std::shared_ptr<mips_meta_op> getptr() {
    return shared_from_this();
  }
};

typedef mips_meta_op* sim_op;

struct sim_state {
  bool terminate_sim = false;
  uint32_t fetch_pc = 0;
  uint32_t retire_pc = 0;
  
  uint32_t gpr_rat[32];
  uint32_t cpr0_rat[32];
  uint32_t cpr1_rat[32];
  uint32_t fcr1_rat[5];

  int num_gpr_prf_ = -1;
  int num_cpr0_prf_ = -1;
  int num_cpr1_prf_ = -1;
  int num_fcr1_prf_ = -1;
  
  int32_t *gpr_prf = nullptr;
  uint32_t *cpr0_prf = nullptr;
  uint32_t *cpr1_prf = nullptr;
  uint32_t *fcr1_prf = nullptr;
  
  sim_bitvec gpr_freevec;
  sim_bitvec cpr0_freevec;
  sim_bitvec cpr1_freevec;
  sim_bitvec fcr1_freevec;

  sim_queue<sim_op> fetch_queue;
  sim_queue<sim_op> decode_queue;
  sim_queue<sim_op> rob;
    
  void initialize_rat_mappings() {
    for(int i = 0; i < 32; i++) {
      gpr_rat[i] = i;
      gpr_freevec.set_bit(i);
      cpr0_rat[i] = i;
      cpr0_freevec.set_bit(i);
      cpr1_rat[i] = i;
      cpr1_freevec.set_bit(i);
    }
    for(int i = 0; i < 5; i++) {
      fcr1_rat[i] = i;
      fcr1_freevec.set_bit(i);
    }
  }
  
  void initialize();
};


class mips_op {
public:
  sim_op m = nullptr;
  mips_op_type op_class = mips_op_type::unknown;
  mips_op(sim_op m) : m(m) {}
  virtual ~mips_op() {}
  virtual void allocate(sim_state &machine_state) = 0;
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
  
  
  mips_op_type get_op_class() const {
    return op_class;
  }
};

mips_op* decode_insn(sim_op m_op);


#endif
