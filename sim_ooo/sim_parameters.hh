#ifndef sim_parameters_hh
#define sim_parameters_hh

#define SIM_PARAM_LIST							\
  SIM_PARAM(heartbeat,(1<<24),1,true)					\
  SIM_PARAM(flash_restart,1,0,true)					\
  SIM_PARAM(rob_size,16384,1,true)					\
  SIM_PARAM(bob_size,32,1,true)						\
  SIM_PARAM(fetchq_size,256,1,true)					\
  SIM_PARAM(decodeq_size,256,1,true)					\
  SIM_PARAM(fetch_bw,64,1,false)					\
  SIM_PARAM(decode_bw,64,1,false)					\
  SIM_PARAM(alloc_bw,64,1,false)					\
  SIM_PARAM(retire_bw,64,1,false)					\
  SIM_PARAM(num_gpr_prf,16384,64,true)					\
  SIM_PARAM(num_cpr0_prf,1024,32,true)					\
  SIM_PARAM(num_cpr1_prf,1024,32,true)					\
  SIM_PARAM(num_fcr1_prf,16384,8,true)					\
  SIM_PARAM(num_fpu_ports,1,1,false)					\
  SIM_PARAM(num_alu_ports,1,1,false)					\
  SIM_PARAM(num_load_ports,1,1,false)					\
  SIM_PARAM(num_fpu_sched_per_cycle,64,1,false)				\
  SIM_PARAM(num_alu_sched_per_cycle,64,1,false)				\
  SIM_PARAM(num_load_sched_per_cycle,64,1,false)			\
  SIM_PARAM(num_alu_sched_entries,4,1,false)				\
  SIM_PARAM(num_fpu_sched_entries,4,1,false)				\
  SIM_PARAM(num_load_sched_entries,4,1,false)				\
  SIM_PARAM(num_system_sched_entries,1,1,false)				\
  SIM_PARAM(load_tbl_size,256,1,false)					\
  SIM_PARAM(store_tbl_size,256,1,false)					\
  SIM_PARAM(taken_branches_per_cycle,8,1,false)				\
  SIM_PARAM(num_loop_entries,0,0,true)					\
  SIM_PARAM(lg_pht_entries,16,0,false)					\
  SIM_PARAM(gselect_addr_bits,8,0,false)				\
  SIM_PARAM(bhr_length,32,1,true)					\
  SIM_PARAM(bht_length,8,1,true)					\
  SIM_PARAM(num_bht_entries,16,1,true)					\
  SIM_PARAM(l1d_latency,3,1,false)					\
  SIM_PARAM(l1d_assoc,1,1,true)						\
  SIM_PARAM(l1d_sets,1024,1024,true)					\
  SIM_PARAM(l1d_linesize,16,16,true)					\
  SIM_PARAM(l2d_latency,10,1,false)					\
  SIM_PARAM(l2d_assoc,16,1,true)					\
  SIM_PARAM(l2d_sets,256,1,true)					\
  SIM_PARAM(l2d_linesize,64,64,true)					\
  SIM_PARAM(l3d_latency,25,1,false)					\
  SIM_PARAM(l3d_assoc,32,1,true)					\
  SIM_PARAM(l3d_sets,4096,1,true)					\
  SIM_PARAM(l3d_linesize,64,64,true)					\
  SIM_PARAM(mem_latency,4,1,false)					\
  SIM_PARAM(ready_to_dispatch_latency,0,0,false)			\
  SIM_PARAM(l1d_misses_inflight,1,1,false)				\
  SIM_PARAM(branch_predictor,6,0,false)					\
  SIM_PARAM(num_tage_tbls,4,1,false)					\
  SIM_PARAM(lg_tage_bimode_tbl_entries,12,1,false)			\
  SIM_PARAM(lg_tage_tagged_tbl_entries,10,1,false)


namespace sim_param {
#define SIM_PARAM(A,B,C,D) extern int A;
  SIM_PARAM_LIST;
#undef SIM_PARAM

  template <typename T, typename std::enable_if<std::is_integral<T>::value,T>::type* = nullptr>
  static constexpr T log2(T n) {
    return ( (n<2) ? 1 : 1+log2(n/2));
  }  
}

#ifndef SAVE_SIM_PARAM_LIST
#undef SIM_PARAM_LIST
#endif



#endif


