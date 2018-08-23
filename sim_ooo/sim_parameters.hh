#ifndef sim_parameters_hh
#define sim_parameters_hh

#define SIM_PARAM_LIST(X)			\
  X(rob_size)					\
  X(fetchq_size)				\
  X(decodeq_size)				\
  X(fetch_bw)					\
  X(decode_bw)					\
  X(alloc_bw)					\
  X(retire_bw)					\
  X(num_gpr_prf)				\
  X(num_cpr0_prf)				\
  X(num_cpr1_prf)				\
  X(num_fcr1_prf)				\
  X(num_fpu_ports)				\
  X(num_alu_ports)				\
  X(num_load_ports)				\
  X(num_store_ports)				\
  X(num_alu_sched_entries)			\
  X(num_fpu_sched_entries)			\
  X(num_jmp_sched_entries)			\
  X(num_load_sched_entries)			\
  X(num_store_sched_entries)			\
  X(num_system_sched_entries)			\
  X(load_tbl_size)				\
  X(store_tbl_size)				\
  X(taken_branches_per_cycle)

namespace sim_param {
#define ENTRY(X) extern int X;
  SIM_PARAM_LIST(ENTRY);
#undef ENTRY
}

#ifndef SAVE_SIM_PARAM_LIST
#undef SIM_PARAM_LIST
#endif

#endif
