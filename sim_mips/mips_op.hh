#ifndef __mipsop_hh__
#define __mipsop_hh__

#include <memory>
#include <vector>
#include <list>

#include "sparse_mem.hh"
#include "sim_queue.hh"
#include "sim_bitvec.hh"
#include "sim_list.hh"
#include "mips_encoding.hh"

uint64_t get_curr_cycle();

extern int log_fd;

enum class mips_op_type { unknown, alu, fp, jmp, load, store, system };

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
  
  int64_t decode_cycle = -1;
  int64_t alloc_cycle = -1;
  int64_t ready_cycle = -1;
  int64_t complete_cycle = -1;
  int64_t retire_cycle = -1;
  /* finished execution */
  bool is_complete = false;
  bool branch_exception = false;
  bool load_exception = false;
  bool is_branch_or_jump = false;
  bool has_delay_slot = false;
  bool likely_squash = false;
  uint32_t correct_pc = 0;
  bool is_store = false, is_fp_store = false;
  uint32_t exec_parity = 0;

  int32_t rob_idx = -1;
  /* result will get written to prf idx */
  int64_t prf_idx = -1, aux_prf_idx=-1;
  /* previous prf writer (will return to freelist when this inst retires */
  int64_t prev_prf_idx = -1, aux_prev_prf_idx = -1;
  int64_t src0_prf = -1, src1_prf = -1, src2_prf = -1, src3_prf = -1, src4_prf = -1;
  int64_t load_tbl_idx = -1, store_tbl_idx = -1;
  int64_t hi_prf_idx = -1, lo_prf_idx = -1;
  int64_t prev_hi_prf_idx = -1, prev_lo_prf_idx = -1;
  
  mips_op* op = nullptr;

  mips_meta_op(uint32_t pc, uint32_t inst,  uint32_t fetch_npc,
	       uint32_t fetch_cycle, bool predict_taken) :
    pc(pc), inst(inst), fetch_npc(fetch_npc), fetch_cycle(fetch_cycle), predict_taken(predict_taken)  {

  }
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

  int last_alu_rs = 0;
  int last_fpu_rs = 0;
  int last_load_rs = 0;
  int num_alu_rs = -1;
  int num_fpu_rs = -1;
  int num_load_rs = -1;
  
  typedef sim_list<sim_op> rs_type;
  std::vector<rs_type> alu_rs;
  std::vector<rs_type> fpu_rs;
  rs_type jmp_rs;
  std::vector<rs_type> load_rs;
  rs_type store_rs;
  rs_type system_rs;

  sparse_mem *mem = nullptr;
  uint64_t icnt = 0, maxicnt = ~(0UL);
  uint64_t n_branches = 0, n_jumps = 0;
  uint64_t mispredicted_branches = 0;
  uint64_t mispredicted_jumps = 0;
  uint64_t nukes = 0, branch_nukes = 0, load_nukes = 0;
  
  bool log_execution = false;
  bool use_interp_check = false;
  struct retire_entry {
    uint32_t inst;
    uint32_t pc;
    uint32_t parity;
    retire_entry(uint32_t inst, uint32_t pc, uint32_t parity) : 
      inst(inst), pc(pc), parity(parity) {}
  };
  std::list<retire_entry> retire_log;
  void log_insn(uint32_t insn, uint32_t pc, uint32_t parity = 0) {
    if(log_execution) {
      retire_log.push_back(retire_entry(insn, pc, parity));
    }
  }
  
  
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
    /* lo and hi regs */
    for(int i = 32; i < 34; i++) {
      gpr_rat[i] = i;
      gpr_freevec.set_bit(i);
      gpr_valid.set_bit(i);
    }
    for(int i = 0; i < 5; i++) {
      fcr1_rat[i] = i;
      fcr1_freevec.set_bit(i);
      fcr1_valid.set_bit(i);
    }
  }
  uint32_t gpr_parity() const {
    uint32_t p = 0;
    for(int i = 0; i < 32; i++) {
      p ^= arch_grf[i];
    }
    return p;
  }
  
  void initialize(sparse_mem *mem);
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

mips_op* decode_insn(sim_op m_op);


#endif
