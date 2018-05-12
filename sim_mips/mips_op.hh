#ifndef __mipsop_hh__
#define __mipsop_hh__

#include <memory>
#include <vector>
#include "sparse_mem.hh"
#include "sim_queue.hh"
#include "sim_bitvec.hh"
#include "mips_encoding.hh"

uint64_t get_curr_cycle();

enum class mips_op_type { unknown, alu, fp, jmp, mem, system };

class mips_op;

struct mips_meta_op : std::enable_shared_from_this<mips_meta_op> {
  uint32_t pc = 0;
  uint32_t inst = 0;
  uint32_t fetch_npc = 0;  
  uint64_t fetch_cycle = 0;
  bool predict_taken = false;
  
  uint64_t decode_cycle = 0;
  uint64_t alloc_cycle = 0;
  uint64_t complete_cycle = 0;
  /* finished execution */
  bool is_complete = false;
  bool branch_exception = false;

  bool has_delay_slot = false;
  uint32_t correct_pc = 0;
  
  int32_t rob_idx = -1;
  /* result will get written to prf idx */
  int64_t prf_idx = -1;
  /* previous prf writer (will return to freelist when this inst retires */
  int64_t prev_prf_idx = -1;
  int64_t src0_prf = -1, src1_prf = -1, src2_prf = -1, src3_prf = -1;
  
  mips_op* op = nullptr;
  
  mips_meta_op(uint32_t pc, uint32_t inst,  uint32_t fetch_npc, uint32_t fetch_cycle) :
    pc(pc), inst(inst), fetch_npc(fetch_npc), fetch_cycle(fetch_cycle), predict_taken(false)  {

  }

  ~mips_meta_op();
  std::shared_ptr<mips_meta_op> getptr() {
    return shared_from_this();
  }
};

typedef mips_meta_op* sim_op;

struct sim_state {
  bool terminate_sim = false;
  bool nuke = false;
  uint32_t fetch_pc = 0;
  uint32_t retire_pc = 0;
  
  int32_t gpr_rat[32];
  int32_t cpr0_rat[32];
  int32_t cpr1_rat[32];
  int32_t fcr1_rat[5];

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

  sim_bitvec gpr_valid;
  sim_bitvec cpr0_valid;
  sim_bitvec cpr1_valid;
  sim_bitvec fcr1_valid;

  sim_queue<sim_op> fetch_queue;
  sim_queue<sim_op> decode_queue;
  sim_queue<sim_op> rob;

  int last_alu_rs = 0;
  int last_fpu_rs = 0;
  int num_alu_rs = -1;
  int num_fpu_rs = -1;
  std::vector<sim_queue<sim_op>> alu_rs;
  std::vector<sim_queue<sim_op>> fpu_rs;
  sim_queue<sim_op> jmp_rs;
  sim_queue<sim_op> mem_rs;
  sim_queue<sim_op> system_rs;

  sparse_mem *mem = nullptr;
  
  void initialize_rat_mappings() {
    for(int i = 0; i < 32; i++) {
      gpr_rat[i] = i;
      gpr_freevec.set_bit(i);
      gpr_valid.set_bit(i);
      cpr0_rat[i] = i;
      cpr0_freevec.set_bit(i);
      cpr0_valid.set_bit(i);
      cpr1_rat[i] = i;
      cpr1_freevec.set_bit(i);
      cpr1_valid.set_bit(i);
    }
    for(int i = 0; i < 5; i++) {
      fcr1_rat[i] = i;
      fcr1_freevec.set_bit(i);
      fcr1_valid.set_bit(i);
    }
  }
  
  void initialize(sparse_mem *mem);
};


class mips_op {
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
