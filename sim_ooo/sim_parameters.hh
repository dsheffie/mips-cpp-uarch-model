#ifndef sim_parameters_hh
#define sim_parameters_hh

#define SIM_PARAM_LIST				\
  SIM_PARAM(rob_size,64,1)			\
  SIM_PARAM(fetchq_size,8,1)			\
  SIM_PARAM(decodeq_size,8,1)			\
  SIM_PARAM(fetch_bw,8,1)			\
  SIM_PARAM(decode_bw,6,1)			\
  SIM_PARAM(alloc_bw,6,1)			\
  SIM_PARAM(retire_bw,6,1)			\
  SIM_PARAM(num_gpr_prf,128,64)			\
  SIM_PARAM(num_cpr0_prf,64,32)			\
  SIM_PARAM(num_cpr1_prf,64,32)			\
  SIM_PARAM(num_fcr1_prf,16,8)			\
  SIM_PARAM(num_fpu_ports,2,1)			\
  SIM_PARAM(num_alu_ports,2,1)			\
  SIM_PARAM(num_load_ports,2,1)			\
  SIM_PARAM(num_store_ports,1,1)		\
  SIM_PARAM(num_alu_sched_entries,64,1)		\
  SIM_PARAM(num_fpu_sched_entries,64,1)		\
  SIM_PARAM(num_jmp_sched_entries,64,1)		\
  SIM_PARAM(num_load_sched_entries,64,1)	\
  SIM_PARAM(num_store_sched_entries,64,1)	\
  SIM_PARAM(num_system_sched_entries,4,1)	\
  SIM_PARAM(load_tbl_size,16,1)			\
  SIM_PARAM(store_tbl_size,16,1)		\
  SIM_PARAM(taken_branches_per_cycle,1,1)	\
  SIM_PARAM(l1d_latency,3,1)

namespace sim_param {
#define SIM_PARAM(A,B,C) extern int A;
  SIM_PARAM_LIST;
#undef SIM_PARAM
}

#ifndef SAVE_SIM_PARAM_LIST
#undef SIM_PARAM_LIST
#endif

#endif


