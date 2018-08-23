#ifndef sim_parameters_hh
#define sim_parameters_hh

namespace sim_param {
  extern int rob_size;
  extern int fetchq_size;
  extern int decodeq_size;
  
  extern int fetch_bw;
  extern int decode_bw;
  extern int alloc_bw;
  extern int retire_bw;
  
  extern int num_gpr_prf;
  extern int num_cpr0_prf;
  extern int num_cpr1_prf;
  extern int num_fcr1_prf;
  extern int num_fpu_ports;
  extern int num_alu_ports;
  
  extern int num_load_ports;
  extern int num_store_ports;
  
  extern int num_alu_sched_entries; 
  extern int num_fpu_sched_entries;
  extern int num_jmp_sched_entries; 
  extern int num_load_sched_entries; 
  extern int num_store_sched_entries;
  extern int num_system_sched_entries;
  
  extern int load_tbl_size;
  extern int store_tbl_size;
  extern int taken_branches_per_cycle;
}
#endif
