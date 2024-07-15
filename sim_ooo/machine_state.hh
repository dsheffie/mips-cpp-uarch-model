 #ifndef __machinestatehh__
#define __machinestatehh__

#include "globals.hh"
#include "sparse_mem.hh"
#include "sim_queue.hh"
#include "sim_bitvec.hh"
#include "sim_list.hh"
#include "sim_stack.hh"
#include "branch_predictor.hh"
#include "loop_predictor.hh"
#include "counter2b.hh"
#include "perceptron.hh"
#include "pipeline_record.hh"

#include <array>

struct state_t;
class simCache;
class mips_meta_op;

class sim_state {
public:
  const static int max_op_lat = 128;
  bool terminate_sim = false;
  bool nuke = false;
  bool alloc_blocked = false;
  bool fetch_blocked = false;
  uint32_t fetch_pc = 0;

  uint64_t last_retire_cycle = 0;
  uint32_t last_retire_pc = 0;

  uint32_t last_compare_pc = 0;
  uint64_t last_compare_icnt = 0;

  static const int num_gpr_regs = 34;
  static const int num_cpr0_regs = 32;
  static const int num_cpr1_regs = 32;
  static const int num_fcr1_regs = 5;
  
  /* hi and lo in grf too */
  int32_t gpr_rat[num_gpr_regs];
  int32_t cpr0_rat[num_cpr0_regs];
  int32_t cpr1_rat[num_cpr1_regs];
  int32_t fcr1_rat[num_fcr1_regs];

  int32_t gpr_rat_retire[num_gpr_regs];
  int32_t cpr0_rat_retire[num_cpr0_regs];
  int32_t cpr1_rat_retire[num_cpr1_regs];
  int32_t fcr1_rat_retire[num_fcr1_regs];

  int num_gpr_prf_ = -1;
  int num_cpr0_prf_ = -1;
  int num_cpr1_prf_ = -1;
  int num_fcr1_prf_ = -1;
  
  int32_t arch_grf[num_gpr_regs] = {0};
  uint32_t arch_grf_last_pc[num_gpr_regs] = {0};

  uint32_t arch_cpr1[num_cpr1_regs] = {0};
  uint32_t arch_cpr1_last_pc[num_cpr1_regs] = {0};

  uint32_t arch_fcr1[num_fcr1_regs] = {0};
  uint32_t arch_fcr1_last_pc[num_fcr1_regs] = {0};
  
  int64_t *gpr_prf = nullptr;


  mips_meta_op **load_tbl = nullptr;
  mips_meta_op **store_tbl = nullptr;
  
  sim_bitvec gpr_freevec;
  sim_bitvec gpr_freevec_retire;
  sim_bitvec load_tbl_freevec;
  sim_bitvec store_tbl_freevec;
  
  sim_bitvec gpr_valid;

  sim_queue<mips_meta_op*> fetch_queue;
  sim_queue<mips_meta_op*> decode_queue;
  sim_queue<mips_meta_op*> rob;

  sim_bitvec alu_alloc;
  sim_bitvec fpu_alloc;
  sim_bitvec load_alloc;
  sim_bitvec store_alloc;
  
  int num_alu_rs = -1;
  int num_fpu_rs = -1;
  int num_load_rs = -1;
  int num_store_rs = -1;
  
  typedef sim_list<mips_meta_op*> rs_type;
  std::vector<rs_type> alu_rs;
  std::vector<rs_type> fpu_rs;
  rs_type jmp_rs;
  std::vector<rs_type> load_rs;
  std::vector<rs_type> store_rs;
  rs_type system_rs;
  
  uint8_t *mem = nullptr;

  branch_predictor *branch_pred = nullptr;
  loop_predictor *loop_pred = nullptr;
  /* use smaller data-type */
  sim_bitvec_template<uint8_t> bhr, spec_bhr;
  std::vector<sim_bitvec> bht;

  std::array<int,max_op_lat> wr_ports;
  
  uint64_t icnt = 0, maxicnt = ~(0UL), skipicnt = 0;
  uint64_t n_branches = 0, n_jumps = 0;
  uint64_t mispredicted_branches = 0;
  uint64_t mispredicted_jumps = 0;
  uint64_t mispredicted_jrs = 0;
  uint64_t mispredicted_jalrs = 0;
  uint64_t nukes = 0, branch_nukes = 0, load_nukes = 0;
  uint64_t fetched_insns = 0;
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
  pipeline_logger *sim_records = nullptr;
  
  void initialize_rat_mappings();
  void initialize();
  void copy_state(const state_t *s);

  ~sim_state() {
    if(gpr_prf) delete [] gpr_prf;
    if(load_tbl) delete [] load_tbl;
    if(store_tbl) delete [] store_tbl;
  }
};


#endif
