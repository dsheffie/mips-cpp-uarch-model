#include <cstdio>
#include <iostream>
#include <set>
#include <fstream>
#include <map>

#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <signal.h>
#include <cxxabi.h>

#include <cstdint>
#include <cstdlib>
#include <string>
#include <cstring>
#include <cassert>

#include "loadelf.hh"
#include "helper.hh"
#include "disassemble.hh"
#include "interpret.hh"
#include "globals.hh"
#include "gthread.hh"
#include "mips_op.hh"
#include "sim_parameters.hh"
#include "sim_cache.hh"
#include "machine_state.hh"

extern std::map<uint32_t, uint32_t> branch_target_map;
extern std::map<uint32_t, int32_t> branch_prediction_map;
static std::map<int64_t, int64_t> insn_lifetime_map;


static inline bool is_likely_branch(uint32_t inst) {
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

static inline bool is_nonlikely_branch(uint32_t inst) {
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


class rollback_rob_entry : public sim_queue<sim_op>::funcobj {
protected:
  sim_state &machine_state;
public:
  rollback_rob_entry(sim_state &machine_state) :
    machine_state(machine_state) {}
  virtual bool operator()(sim_op e){
    if(e) {
      e->op->rollback(machine_state);
      delete e;
      return true;
    }
    return false;
  }
};

void fetch(sim_state &machine_state) {
  auto &fetch_queue = machine_state.fetch_queue;
  auto &return_stack = machine_state.return_stack;
  uint8_t *mem = machine_state.mem;
  
  while(not(machine_state.terminate_sim)) {
    int fetch_amt = 0, taken_branches = 0;
    for(; not(fetch_queue.full()) and (fetch_amt < sim_param::fetch_bw) and not(machine_state.nuke) and not(machine_state.fetch_blocked); ) {
      
      
      uint32_t inst = *reinterpret_cast<uint32_t*>(mem + machine_state.fetch_pc);
      uint32_t npc = machine_state.fetch_pc + 4;
      bool predict_taken = false;

      bool used_return_addr_stack = false;
      
      mips_meta_op *f = new mips_meta_op(machine_state.fetched_insns,
					 machine_state.fetch_pc,
					 inst,
					 global::curr_cycle);
      //bool backwards_br = (get_branch_target(machine_state.fetch_pc, inst) < machine_state.fetch_pc);
      
      // if(false) {

      // }
      // else {
      // 	f->prediction = machine_state.branch_pred->predict(f->pht_idx);
	
      // 	if(is_jr(inst)) {
      // 	  f->return_stack_idx = return_stack.get_tos_idx();
      // 	  npc = return_stack.pop();
      // 	  machine_state.delay_slot_npc = machine_state.fetch_pc + 4;
      // 	  used_return_addr_stack = true;
      // 	}
      // 	else if(is_jal(inst)) {
      // 	  machine_state.delay_slot_npc = machine_state.fetch_pc + 4;
      // 	  npc = get_jump_target(machine_state.fetch_pc, inst);
      // 	  predict_taken = true;
      // 	  f->return_stack_idx = return_stack.get_tos_idx();
      // 	  return_stack.push(machine_state.fetch_pc + 8);
      // 	  fetch_amt++;
      // 	}
      // 	else if(is_j(inst)) {
      // 	  machine_state.delay_slot_npc = machine_state.fetch_pc + 4;
      // 	  npc = get_jump_target(machine_state.fetch_pc, inst);
      // 	  predict_taken = true;
      // 	}
      // 	else if(it != branch_prediction_map.end()) {
      // 	  predict_taken = (f->prediction > 1);

      // 	  /* check if backwards branch with valid loop predictor entry */	  
      // 	  if(backwards_br and (machine_state.loop_pred !=nullptr) ) {
      // 	    if(machine_state.loop_pred->valid_loop_branch(machine_state.fetch_pc)) {
      // 	      predict_taken = machine_state.loop_pred->predict(machine_state.fetch_pc, f->prediction);
      // 	    }
      // 	  }

	  
      // 	  if(predict_taken) {
      // 	    machine_state.delay_slot_npc = machine_state.fetch_pc + 4;
      // 	    npc = branch_target_map.at(machine_state.fetch_pc);
      // 	  }
      // 	}
      // 	else if(is_nonlikely_branch(inst)) {
      // 	  uint32_t target = get_branch_target(machine_state.fetch_pc, inst);
      // 	  if(target < machine_state.fetch_pc) {
      // 	    machine_state.delay_slot_npc = machine_state.fetch_pc + 4;
      // 	    npc = get_branch_target(machine_state.fetch_pc, inst);
      // 	    predict_taken = true;
      // 	  }
      // 	}
      // }
      f->fetch_npc = npc;
      f->predict_taken = predict_taken;
      f->pop_return_stack = used_return_addr_stack;
      
      fetch_queue.push(f);
      fetch_amt++;
      machine_state.fetched_insns++;
      machine_state.fetch_pc = npc;
      if(predict_taken)
	taken_branches++;
    }

    gthread_yield();
  }
  gthread_terminate();
}

template<bool enable_oracle>
void retire(sim_state &machine_state) {
  state_t *s = machine_state.ref_state;
  auto &rob = machine_state.rob;
  int stuck_cnt = 0, empty_cnt = 0;
  uint64_t num_retired_insns = 0;
  while(not(machine_state.terminate_sim)) {
    int retire_amt = 0;
    sim_op u = nullptr;
    bool stop_sim = false;
    bool exception = false;
      
    if(rob.empty()) {
      empty_cnt++;
      if(empty_cnt > 64) {
	std::cerr << "empty ROB for 64 cycles at " << get_curr_cycle() << "\n";
      }
    }
      
    while(not(rob.empty()) and (retire_amt < sim_param::retire_bw) and (machine_state.icnt < machine_state.maxicnt)) {
      u = rob.peek();
      empty_cnt = 0;
      if(not(u->is_complete)) {
	//if(stuck_cnt > 32) {
	//std::cerr << "STUCK:" << *(u->op)
	//	    << "," << u
	//	    << "\n";
	//}
	stuck_cnt++;
	u = nullptr;
	break;
      }
      if(u->complete_cycle == global::curr_cycle) {
	u = nullptr;
	break;
      }

	
      if(u->exception==exception_type::branch) {
	machine_state.nukes++;
	machine_state.branch_nukes++;
	exception = true;
	break;
      }
      else if(u->load_exception) {
	machine_state.nukes++;
	machine_state.load_nukes++;
	exception = true;
	break;
      }

      if(u->has_delay_slot) {
	while(rob.peek_next_pop() == nullptr) {
	  stuck_cnt++;
	  gthread_yield();
	}
	sim_op uu = rob.peek_next_pop();
	while(not(uu->is_complete)) {
	  stuck_cnt++;
	  gthread_yield();
	}
	if(u->exception==exception_type::branch or uu->load_exception) {
	  machine_state.nukes++;
	  if(uu->exception == exception_type::branch) {
	    machine_state.branch_nukes++;
	  }
	  else {
	    machine_state.load_nukes++;
	  }
	  exception = true;
	  break;
	}
      }

      if(global::use_interp_check and (s->pc == u->pc)) {
	assert(not(exception));
	bool error = false;
	for(int i = 0; i < 32; i++) {
	  if(s->gpr[i] != machine_state.arch_grf[i]) {
	    std::cerr << "uarch reg " << getGPRName(i) << " : " 
		      << std::hex << machine_state.arch_grf[i] << std::dec << "\n"; 
	    std::cerr << "func reg " << getGPRName(i) << " : " 
		      << std::hex << s->gpr[i] << std::dec << "\n"; 
	    error = true;
	  }
	}
	
	if(error) {
	  std::cerr << "bad insn : " << std::hex << u->pc << ":" << std::dec
		    << getAsmString(u->inst, u->pc)
		    << " after " << machine_state.icnt << " uarch isns and "
		    << s->icnt << " arch isns\n";
	  std::cerr << "known good at pc " << std::hex << machine_state.last_compare_pc
		    << std::dec << " after " << machine_state.last_compare_icnt
		    << " isnsns\n";
	  machine_state.terminate_sim = true;
	  break;
	}
	else {
	  machine_state.last_compare_pc = u->pc;
	  machine_state.last_compare_icnt = machine_state.icnt;
	}
	execRiscv(s);
      }

      u->op->retire(machine_state);


      num_retired_insns++;
#if 0
      if(true) {	
	std::cerr << num_retired_insns
		  << " : "
		  << *(u->op)
		  << "\n";
      }
#endif
      stuck_cnt = 0;
      int64_t insn_lifetime = static_cast<int64_t>(u->retire_cycle) - static_cast<int64_t>(u->fetch_cycle);
      insn_lifetime_map[insn_lifetime]++;
      machine_state.last_retire_cycle = get_curr_cycle();
      machine_state.last_retire_pc = u->pc;
	
      stop_sim = u->op->stop_sim();
      delete u;
      u = nullptr;
      retire_amt++;
      rob.pop();
      if(stop_sim) {
	break;
      }

    }


    if(u!=nullptr and exception) {
      assert(u->could_cause_exception);
	
      if((retire_amt - sim_param::retire_bw) < 2) {
	gthread_yield();
	retire_amt = 0;
      }
	
      bool is_load_exception = u->load_exception;
      uint32_t exc_pc = u->pc;
      bool delay_slot_exception = false;
      
      if(u->exception==exception_type::branch) {
	if(u->has_delay_slot) {
	  /* wait for branch delay instr to allocate */
	  machine_state.alloc_blocked = false;
	  while(rob.peek_next_pop() == nullptr) {
	    gthread_yield();
	    retire_amt = 0;
	  }
	  sim_op uu = rob.peek_next_pop();
	  while(not(uu->is_complete) or (uu->complete_cycle == get_curr_cycle())) {
	    gthread_yield();
	    retire_amt = 0;
	  }
	  if(uu->exception==exception_type::branch or uu->load_exception) {
	    delay_slot_exception = true;
	  }
	  else {
	    //std::cerr << "retire for " << *(u->op) << "\n";
	    u->op->retire(machine_state);
	    num_retired_insns++;
	    int64_t lifetime_cycles = static_cast<int64_t>(u->retire_cycle)-static_cast<int64_t>(u->fetch_cycle);
	    //if(lifetime_cycles > 1000) {
	    //std::cerr << "@ cycle " << get_curr_cycle() << ":" << *(u->op) << " has a huge lifetime!\n";
	    // die();
	    //}
	    insn_lifetime_map[lifetime_cycles]++;
	    machine_state.last_retire_cycle = get_curr_cycle();
	    machine_state.last_retire_pc = u->pc;
	    //std::cout << std::hex << u->pc << ":" << std::hex
	    //<< getAsmString(u->inst, u->pc) << "\n";

	    //std::cerr << "retire for " << *(uu->op) << "\n";
	    uu->op->retire(machine_state);
	    num_retired_insns++;
	    machine_state.last_retire_cycle = get_curr_cycle();
	    machine_state.last_retire_pc = uu->pc;
	    lifetime_cycles = static_cast<int64_t>(uu->retire_cycle)-static_cast<int64_t>(uu->fetch_cycle);
	    //if(lifetime_cycles > 1000) {
	    //std::cerr << "@ cycle " << get_curr_cycle() << ":" << *(uu->op) << " has a huge lifetime!\n";
	    //die();
	    //}
	    insn_lifetime_map[lifetime_cycles]++;
	    //std::cout << std::hex << uu->pc << ":" << std::hex
	    //<< getAsmString(uu->inst, uu->pc) << "\n";
	    if(global::use_interp_check and (s->pc == u->pc)) {
	      execRiscv(s);
	    }
	    rob.pop();
	    rob.pop();
	    retire_amt+=2;
	    delete uu;
	  }
	}
	else {
	  //std::cerr << "retire for " << *(u->op) << "\n";
	  u->op->retire(machine_state);
	  num_retired_insns++;
	  int64_t lifetime_cycles = static_cast<int64_t>(u->retire_cycle)-static_cast<int64_t>(u->fetch_cycle);
	  //if(lifetime_cycles > 1000) {
	  //std::cerr << "@ cycle " << get_curr_cycle() << ":" << *(u->op) << " has a huge lifetime!\n";
	  //die();
	  //}
	  insn_lifetime_map[lifetime_cycles]++;
	  machine_state.last_retire_cycle = get_curr_cycle();
	  machine_state.last_retire_pc = u->pc;
	  if(global::use_interp_check and (s->pc == u->pc)) {
	    execRiscv(s);
	  }
	  rob.pop();
	  retire_amt++;
	}
      }
      machine_state.nuke = true;
      stuck_cnt = 0;
      if(delay_slot_exception) {
	assert(not(enable_oracle));
	machine_state.fetch_pc = u->pc;
      }
      else {
	if(u->exception == exception_type::branch) {
	  machine_state.fetch_pc = u->correct_pc;
	  if(enable_oracle) {
	    assert((u->fetch_icnt+1)==machine_state.fetched_insns);
	  }
	  delete u;
	}
	else {
	  machine_state.fetch_pc = u->pc;
	  if(enable_oracle) {
	    machine_state.fetched_insns = u->fetch_icnt;
	  }
	}
      }

      if(machine_state.l1d) {
	machine_state.l1d->nuke_inflight();
      }


      /* quick way to reset rob with flash copied tables */
      int64_t c = 0;
      for(size_t i = 0, len = rob.capacity(); i < len; i++) {
	if(rob.at(i)) {
	  delete rob.at(i);
	  rob.at(i) = nullptr;
	  c++;
	}
      }
      
      memcpy(&machine_state.gpr_rat, &machine_state.gpr_rat_retire,
	     sizeof(int32_t)*sim_state::num_gpr_regs);

      
      machine_state.gpr_freevec.copy(machine_state.gpr_freevec_retire);
      machine_state.spec_bhr.copy(machine_state.bhr);
      
      //std::cerr << "gpr freevec used = "
      //<< machine_state.gpr_freevec.popcount()
      //<< "\n";
      //std::cerr << "cpr1 freevec used = "
      //<< machine_state.cpr1_freevec.popcount()
      
      machine_state.gpr_valid.clear();

      for(int i = 0; i < sim_state::num_gpr_regs; i++) {
	machine_state.gpr_valid.set_bit(machine_state.gpr_rat[i]);
      }

      
      if(sim_param::flash_restart==0) {
	int64_t sleep_cycles = (c + sim_param::retire_bw - 1) / sim_param::retire_bw;
	for(int64_t i = 0; i < sleep_cycles; i++) {
	  gthread_yield();
	}
      }
      rob.clear();
	
      for(size_t i = 0; i < machine_state.fetch_queue.capacity(); i++) {
	auto f = machine_state.fetch_queue.at(i);
	if(f) {
	  delete f;
	}
      }
      for(size_t i = 0; i < machine_state.decode_queue.capacity(); i++) {
	auto d = machine_state.decode_queue.at(i);
	if(d) {
	  delete d;
	}
      }

      //machine_state.return_stack.clear();
      machine_state.decode_queue.clear();
      machine_state.fetch_queue.clear();
      machine_state.delay_slot_npc = 0;
      machine_state.alloc_blocked = false;
      machine_state.fetch_blocked = false;
      for(int i = 0; i < machine_state.num_alu_rs; i++) {
	machine_state.alu_rs.at(i).clear();
      }
      for(int i = 0; i < machine_state.num_fpu_rs; i++) {
	machine_state.fpu_rs.at(i).clear();
      }
      for(int i = 0; i < machine_state.num_load_rs; i++) {
	machine_state.load_rs.at(i).clear();
      }
      machine_state.jmp_rs.clear();
      for(int i = 0; i < machine_state.num_store_rs;i++) {
	machine_state.store_rs.at(i).clear();
      }
      machine_state.system_rs.clear();
      machine_state.load_tbl_freevec.clear();
      machine_state.store_tbl_freevec.clear();
      for(size_t i = 0; i < machine_state.load_tbl_freevec.size(); i++) {
	machine_state.load_tbl[i] = nullptr;
      }
      for(size_t i = 0; i < machine_state.store_tbl_freevec.size(); i++) {
	machine_state.store_tbl[i] = nullptr;
      }
      machine_state.nuke = false;

      // if(enable_oracle) {
      // 	machine_state.oracle_state->copy(machine_state.ref_state);
      // 	machine_state.oracle_mem->copy(machine_state.ref_state->mem);
      // }
	
      gthread_yield();
      //std::cout << "rolling back complete @ cycle " << get_curr_cycle() << "\n";
    }
    else {
      gthread_yield();
    }
    if(machine_state.icnt >= machine_state.maxicnt or stop_sim) {
      machine_state.terminate_sim = true;
    }
  }
  gthread_terminate();
}

void initialize_ooo_core(sim_state &machine_state,
			 simCache *l1d,
			 uint64_t skipicnt, 
			 uint64_t maxicnt,
			 state_t *s) {

  machine_state.l1d = l1d;
  
  if(skipicnt != 0) {
    while((s->icnt < skipicnt) and not(s->brk)) {
      execRiscv(s);
    }
  }
  machine_state.ref_state = s;

  machine_state.mem = new uint8_t[1UL<<32];
  memcpy(machine_state.mem, s->mem, 1UL<<32);
  machine_state.initialize();
  machine_state.maxicnt = maxicnt;
  machine_state.skipicnt = s->icnt;
  machine_state.copy_state(s);

}


void destroy_ooo_core(sim_state &machine_state) {
  for(size_t i = 0; i < machine_state.fetch_queue.capacity(); i++) {
    auto f = machine_state.fetch_queue.at(i);
    if(f) {
      delete f;
    }
  }
  for(size_t i = 0; i < machine_state.decode_queue.capacity(); i++) {
    auto d = machine_state.decode_queue.at(i);
    if(d) {
      delete d;
    }
  }
  for(size_t i = 0; i < machine_state.rob.capacity(); i++) {
    auto r = machine_state.rob.at(i);
    if(r) {
      delete r;
    }
  }

  delete [] machine_state.mem;
  delete machine_state.branch_pred;
  if(machine_state.loop_pred != nullptr) {
    delete machine_state.loop_pred;
  }
  
  gthread::free_threads();
}



extern "C" {
  void cycle_count(void *arg) {
    sim_state &machine_state = *reinterpret_cast<sim_state*>(arg);
    uint64_t prev_icnt = 0;
    uint64_t prev_br_and_jmps = 0;
    uint64_t prev_mispredicts = 0;
    
    simCache *l1d = machine_state.l1d;
    uint64_t last_hits = 0, last_misses = 0;
    while(not(machine_state.terminate_sim)) {
      global::curr_cycle++;
      uint64_t delta = global::curr_cycle - machine_state.last_retire_cycle;
      if((sim_param::mem_latency >= 100) and (delta > (sim_param::mem_latency*2))) {
	std::cerr << "no retirement in "
		  << sim_param::mem_latency*2
		  << " cycles, last pc = "
		  << std::hex
		  << machine_state.last_retire_pc
		  << std::dec
		  << "\n";
	machine_state.terminate_sim = true;
      }
      if((global::curr_cycle & (sim_param::heartbeat-1)) == 0) {
	uint64_t curr_icnt = (machine_state.icnt-machine_state.skipicnt);
	double ipc = static_cast<double>(curr_icnt) / global::curr_cycle;
	double wipc = static_cast<double>(curr_icnt-prev_icnt) / sim_param::heartbeat;

	uint64_t br_and_jmps = machine_state.n_branches + machine_state.n_jumps;
	uint64_t mispredicts = machine_state.mispredicted_branches + machine_state.mispredicted_jumps;

	uint64_t w_br_and_jmps = br_and_jmps - prev_br_and_jmps;
	uint64_t w_mispredicts = mispredicts - prev_mispredicts;
	
	double pr = 1000.0 * (static_cast<double>(mispredicts) / curr_icnt);
	double w_pr = 1000.0 * (static_cast<double>(w_mispredicts) / (curr_icnt-prev_icnt));

	
	*global::sim_log << "c " << global::curr_cycle 
			      << ", i " << curr_icnt
			      << ", a ipc "<< ipc
			      << ", w ipc " << wipc
			      << ", a mpki " << pr
			      << ", w mpki " << w_pr;
	
	if(l1d) {
	  uint64_t hits = l1d->getHits()-last_hits;
	  uint64_t misses = l1d->getMisses()-last_misses;
	  double w_hit_rate = static_cast<double>(hits) / (hits+misses);
	  double hit_rate = static_cast<double>(l1d->getHits()) / (l1d->getHits()+l1d->getMisses());
	  *global::sim_log << ", a dcu " << hit_rate
				<< ", w dcu " << w_hit_rate ;
	  last_hits = l1d->getHits();
	  last_misses = l1d->getMisses();
	}
	*global::sim_log <<"\n";
	global::sim_log->flush();
	prev_icnt = curr_icnt;
	prev_br_and_jmps = br_and_jmps;
	prev_mispredicts = mispredicts;
      }
      gthread_yield();
    }
    gthread_terminate();
  }
  void cache(void *arg) {
    sim_state &machine_state = *reinterpret_cast<sim_state*>(arg);
    if(machine_state.l1d != nullptr)  {
      while(not(machine_state.terminate_sim)) {
	machine_state.l1d->tick();
	gthread_yield();
      }
    }
    gthread_terminate();
  }
  void fetch(void *arg) {
    sim_state &machine_state = *reinterpret_cast<sim_state*>(arg);
    fetch(machine_state);
  }
  void decode(void *arg) {
    sim_state &machine_state = *reinterpret_cast<sim_state*>(arg);
    auto &fetch_queue = machine_state.fetch_queue;
    auto &decode_queue = machine_state.decode_queue;
    auto &return_stack = machine_state.return_stack;
    while(not(machine_state.terminate_sim)) {
      int decode_amt = 0;
      while(not(fetch_queue.empty()) and not(decode_queue.full()) and (decode_amt < sim_param::decode_bw) and not(machine_state.nuke)) {
	auto u = fetch_queue.peek();
	if(not((u->fetch_cycle+1) < global::curr_cycle)) {
	  break;
	}
	fetch_queue.pop();
	u->decode_cycle = global::curr_cycle;
	u->op = decode_insn(u);
	decode_queue.push(u);
	decode_amt++;
      }
      gthread_yield();
    }
    gthread_terminate();
  }

  void allocate(void *arg) {
    sim_state &machine_state = *reinterpret_cast<sim_state*>(arg);
    auto &decode_queue = machine_state.decode_queue;
    auto &rob = machine_state.rob;
    auto &alu_alloc = machine_state.alu_alloc;
    auto &fpu_alloc = machine_state.fpu_alloc;
    auto &load_alloc = machine_state.load_alloc;
    auto &store_alloc = machine_state.store_alloc;
    int64_t alloc_counter = 0;
    while(not(machine_state.terminate_sim)) {
      int alloc_amt = 0;
      std::map<oper_type, int> alloc_histo;
      alu_alloc.clear();
      fpu_alloc.clear();
      load_alloc.clear();
      store_alloc.clear();

      
      while(not(decode_queue.empty())
	    and not(rob.full())
	    and (alloc_amt < sim_param::alloc_bw)
	    and not(machine_state.nuke)
	    and not(machine_state.alloc_blocked)) {
	auto u = decode_queue.peek();

	bool jmp_avail = true, store_avail = true, system_avail = true;
	sim_state::rs_type *rs_queue = nullptr;
	bool rs_available = false;

	if(u->op == nullptr) {
	  std::cout << "u->op == nullptr @ " << get_curr_cycle() << ",pc = "
		    << std::hex << u->pc << std::dec << "\n";
	  die();

	  break;
	}
	else if(u->decode_cycle == global::curr_cycle) {
	  break;
	}

	switch(u->op->get_op_class())
	  {
	  case oper_type::unknown:
	    die();
	  case oper_type::alu: {
	    int64_t p = alu_alloc.find_first_unset_rr();
	    int64_t rs_id = mod(static_cast<int>(p),sim_param::num_alu_ports);
	    if(p!=-1 and not(machine_state.alu_rs.at(rs_id).full())) {
	      rs_available = true;
	      rs_queue = &(machine_state.alu_rs.at(rs_id));
	      alu_alloc.set_bit(p);
	      alloc_histo[u->op->get_op_class()]++;
	    }
	  }
	    break;
	  case oper_type::fp: {
	    int64_t p = fpu_alloc.find_first_unset_rr();
	    int64_t rs_id = mod(static_cast<int>(p),sim_param::num_fpu_ports);
	    if(p!=-1 and not(machine_state.fpu_rs.at(rs_id).full())) {
	      rs_available = true;
	      rs_queue = &(machine_state.fpu_rs.at(rs_id));
	      fpu_alloc.set_bit(p);
	      alloc_histo[u->op->get_op_class()]++;
	    }
	  }
	    break;
	  case oper_type::jmp:
	    if(jmp_avail and not(machine_state.jmp_rs.full())) {
	      rs_available = true;
	      rs_queue = &(machine_state.jmp_rs);
	      alloc_histo[u->op->get_op_class()]++;
	      jmp_avail = false;
	    }
	    break;
	  case oper_type::load: {
	    int64_t p = load_alloc.find_first_unset_rr();
	    int64_t rs_id = mod(static_cast<int>(p),sim_param::num_load_ports);
	    if(p!=-1 and not(machine_state.load_rs.at(rs_id).full())) {
		rs_available = true;
		rs_queue = &(machine_state.load_rs.at(rs_id));
		load_alloc.set_bit(p);
		alloc_histo[u->op->get_op_class()]++;
	    }
	    break;
	  }
	  case oper_type::store: {
	    int64_t p = store_alloc.find_first_unset_rr();
	    int64_t rs_id = mod(static_cast<int>(p),sim_param::num_store_ports);
	    if(p!=-1 and not(machine_state.store_rs.at(rs_id).full())) {
	      rs_available = true;
	      rs_queue = &(machine_state.store_rs.at(rs_id));
	      store_alloc.set_bit(p);
	      alloc_histo[u->op->get_op_class()]++;
	      store_avail = false;
	    }
	    break;
	  }
	  case oper_type::system:
	    if(system_avail and not(machine_state.system_rs.full())) {
	      rs_available = true;
	      rs_queue = &(machine_state.system_rs);
	      alloc_histo[u->op->get_op_class()]++;
	      system_avail = false;
	    }
	    break;
	  }
	
	if(not(rs_available)) {
#if 0
	  std::cout << "can't allocate due to lack of "
	  	    << u->op->get_op_class()
		    << " resources @ cycle "
		    << get_curr_cycle()
		    << "\n";
#endif
	  break;
	}
	
	/* just yield... */
	assert(u->op != nullptr);

	if(not(u->op->allocate(machine_state))) {
#if 0
	  std::cout << "allocation failed @ cycle "
		    << get_curr_cycle()
		    << " for 0x"
		    << std::hex
		    << u->pc
		    << std::dec
		    << "\n";
#endif
	  break;
	}
	u->alloc_id = alloc_counter++;

	rs_queue->push(u);
	decode_queue.pop();
	u->alloc_cycle = global::curr_cycle;
	u->rob_idx = rob.push(u);
	alloc_amt++;
      }
      machine_state.total_allocated_insns += alloc_amt;
      gthread_yield();
    }
    gthread_terminate();
  }

  void execute(void *arg) {
    sim_state &machine_state = *reinterpret_cast<sim_state*>(arg);
    auto & alu_rs = machine_state.alu_rs;
    auto & fpu_rs = machine_state.fpu_rs;
    auto & jmp_rs = machine_state.jmp_rs;
    auto & load_rs = machine_state.load_rs;
    auto & store_rs = machine_state.store_rs;
    auto & system_rs = machine_state.system_rs;
    
    while(not(machine_state.terminate_sim)) {
      int exec_cnt = 0;
      if(not(machine_state.nuke)) {
	machine_state.wr_ports[get_curr_cycle()% sim_state::max_op_lat] = 0;
	//alu loop (OoO scheduler)
#define OOO_SCHED(RS,NUM,PORTS) {					\
	  int ns = 0, rd_ports = 0;					\
	  const uint64_t cs = get_curr_cycle();				\
	  for(auto it = RS.begin(); it != RS.end(); it++) {		\
	    sim_op u = *it;						\
	    bool r = u->op->ready(machine_state);			\
	    if(r) machine_state.total_ready_insns++;			\
	    if((u->ready_cycle==-1) and r) {				\
	      u->ready_cycle = cs;					\
	    }								\
	  }								\
	  for(auto it = RS.begin(); it != RS.end() and (ns <= NUM) and (PORTS>=0); /*nil*/ ) { \
	    sim_op u = *it;						\
	    if(u->op->ready(machine_state) and	(cs >= (sim_param::ready_to_dispatch_latency+u->ready_cycle)) ) { \
	      int num_ports = u->op->count_rd_ports();			\
	      int wb_at_cycle = (get_curr_cycle() + u->op->get_latency()) % sim_state::max_op_lat; \
	      if((PORTS-num_ports) >= 0 /*and machine_state.wr_ports[wb_at_cycle] < 4*/) { \
		it = RS.erase(it);					\
		u->op->execute(machine_state);				\
		u->dispatch_cycle = cs;					\
		exec_cnt++;						\
		ns++;							\
		PORTS-=num_ports;					\
		machine_state.wr_ports[wb_at_cycle]++;			\
	      }								\
	      else {							\
		it++;							\
	      }								\
	    }								\
	    else {							\
	      it++;							\
	    }								\
	  }								\
	}
	
#define INORDER_SCHED(RS) {						\
	  for(auto it = RS.begin(); it != RS.end(); it++) {		\
	    sim_op u = *it;						\
	    bool r = u->op->ready(machine_state);			\
	    if(r) machine_state.total_ready_insns++;			\
	    if((u->ready_cycle==-1) and r) {				\
	      u->ready_cycle = get_curr_cycle();			\
	    }								\
	  }								\
	  if(not(RS.empty())) {						\
	    if(RS.peek()->op->ready(machine_state) and			\
	       (get_curr_cycle() >= (sim_param::ready_to_dispatch_latency+RS.peek()->ready_cycle)) ) { \
	      sim_op u = RS.pop();					\
	      u->op->execute(machine_state);				\
	      u->dispatch_cycle = get_curr_cycle();			\
	      exec_cnt++;						\
	    }								\
	  }								\
	}

	int avail_int_ports = 1024, avail_fp_ports = 1024;
	for(int i = 0; i < machine_state.num_alu_rs; i++) {
	  OOO_SCHED(alu_rs.at(i),sim_param::num_alu_sched_per_cycle,avail_int_ports);
	}
	for(int i = 0; i < machine_state.num_load_rs; i++) {
	  OOO_SCHED(load_rs.at(i),sim_param::num_load_sched_per_cycle,avail_int_ports);
	}
	/* not really out-of-order as stores are processed
	 * at retirement */
	for(int i = 0; i < machine_state.num_store_rs; i++) {
	  OOO_SCHED(store_rs.at(i),sim_param::num_store_sched_per_cycle,avail_int_ports);
	}
	
	for(int i = 0; i < machine_state.num_fpu_rs; i++) {
	  OOO_SCHED(fpu_rs.at(i),sim_param::num_fpu_sched_per_cycle,avail_fp_ports);
	}

	OOO_SCHED(jmp_rs,1,avail_int_ports);
	INORDER_SCHED(system_rs);
#undef OOO_SCHED
#undef INORDER_SCHED
      }
      else {
	std::fill(machine_state.wr_ports.begin(),
		  machine_state.wr_ports.end(),
		  0);
      }
      machine_state.total_dispatched_insns += exec_cnt;
      gthread_yield();
    }
    gthread_terminate();
  }
  void complete(void *arg) {
    sim_state &machine_state = *reinterpret_cast<sim_state*>(arg);
    auto &rob = machine_state.rob;
    while(not(machine_state.terminate_sim)) {
      for(size_t i = 0; not(machine_state.nuke) and (i < rob.capacity()); i++) {
	if((rob.at(i) != nullptr) and not(rob.at(i)->is_complete)) {
	  if(rob.at(i)->op == nullptr) {
	    std::cerr << "@ cycle " <<  get_curr_cycle() << " "
		      << std::hex << rob.at(i)->pc << std::dec << " "
		      <<  getAsmString(rob.at(i)->inst, rob.at(i)->pc)
		      << " breaks retirement\n";
	    exit(-1);
	  }
	  rob.at(i)->op->complete(machine_state);
	}
      }
      gthread_yield();
    }
    gthread_terminate();
  }
  void retire(void *arg) {
    sim_state &machine_state = *reinterpret_cast<sim_state*>(arg);
    if(machine_state.oracle_mem)
      retire<true>(machine_state);
    else
      retire<false>(machine_state);
  }

  
};



void sim_state::copy_state(const state_t *s) {
  fetch_pc = s->pc;
  for(int i = 0; i < 32; i++) {
    gpr_prf[gpr_rat[i]] = s->gpr[i];
    arch_grf[i] = s->gpr[i];
  }

  icnt = s->icnt;
}


void sim_state::initialize_rat_mappings() {
  for(int i = 0; i < 32; i++) {
    gpr_rat[i] = i;
    gpr_rat_retire[i] = i;
    gpr_freevec.set_bit(i);
    gpr_freevec_retire.set_bit(i);
    gpr_valid.set_bit(i);
  }
  /* lo and hi regs */
  for(int i = 32; i < 34; i++) {
    gpr_rat[i] = i;
    gpr_rat_retire[i] = i;
    gpr_freevec.set_bit(i);
    gpr_freevec_retire.set_bit(i);
    gpr_valid.set_bit(i);
  }
}

void sim_state::initialize() {
  num_gpr_prf_ = sim_param::num_gpr_prf;
  
  gpr_prf = new int64_t[num_gpr_prf_];
  memset(gpr_prf, 0, sizeof(int64_t)*num_gpr_prf_);

  
  gpr_freevec.clear_and_resize(sim_param::num_gpr_prf);
  gpr_freevec_retire.clear_and_resize(sim_param::num_gpr_prf);
  
  load_tbl_freevec.clear_and_resize(sim_param::load_tbl_size);
  load_tbl = new mips_meta_op*[sim_param::load_tbl_size];

  store_tbl_freevec.clear_and_resize(sim_param::store_tbl_size);
  store_tbl = new mips_meta_op*[sim_param::store_tbl_size];
  
  for(size_t i = 0; i < load_tbl_freevec.size(); i++) {
    load_tbl[i] = nullptr;
  }
  for(size_t i = 0; i < store_tbl_freevec.size(); i++) {
    store_tbl[i] = nullptr;
  }
  
  gpr_valid.clear_and_resize(sim_param::num_gpr_prf);
  
  fetch_queue.resize(sim_param::fetchq_size);
  decode_queue.resize(sim_param::decodeq_size);
  rob.resize(sim_param::rob_size);

  num_alu_rs = sim_param::num_alu_ports;
  num_fpu_rs = sim_param::num_fpu_ports;
  num_load_rs = sim_param::num_load_ports;
  num_store_rs = sim_param::num_store_ports;
  
  alu_rs.resize(sim_param::num_alu_ports);
  for(int i = 0; i < sim_param::num_alu_ports; i++) {
    alu_rs.at(i).resize(sim_param::num_alu_sched_entries);
  }

  fpu_rs.resize(sim_param::num_fpu_ports);
  for(int i = 0; i < sim_param::num_fpu_ports; i++) {
    fpu_rs.at(i).resize(sim_param::num_fpu_sched_entries);
  }

  load_rs.resize(sim_param::num_load_ports);
  for(int i = 0; i < sim_param::num_load_ports; i++) {
    load_rs.at(i).resize(sim_param::num_load_sched_entries);
  }
  store_rs.resize(sim_param::num_store_ports);
  for(int i = 0; i < sim_param::num_store_ports; i++) {
    store_rs.at(i).resize(sim_param::num_store_sched_entries);
  }
  
  jmp_rs.resize(sim_param::num_jmp_sched_entries);
  system_rs.resize(sim_param::num_system_sched_entries);

  branch_pred = branch_predictor::get_predictor(sim_param::branch_predictor, *this);
    
  if(sim_param::num_loop_entries) {
    loop_pred = new loop_predictor(sim_param::num_loop_entries);
  }
  
  bhr.clear_and_resize(sim_param::bhr_length);
  spec_bhr.clear_and_resize(sim_param::bhr_length);

  bht.resize(sim_param::num_bht_entries);
  for(int i = 0; i < sim_param::num_bht_entries; i++) {
    bht.at(i).clear_and_resize(sim_param::bht_length);
  }
  

  num_alu_rs = sim_param::num_alu_ports;
  num_fpu_rs = sim_param::num_fpu_ports;
  num_load_rs = sim_param::num_load_ports;
  num_store_rs = sim_param::num_store_ports;
  
  
  alu_alloc.clear_and_resize(sim_param::num_alu_ports * sim_param::num_alu_sched_per_cycle);
  fpu_alloc.clear_and_resize(sim_param::num_fpu_ports * sim_param::num_fpu_sched_per_cycle);
  load_alloc.clear_and_resize(sim_param::num_load_ports * sim_param::num_load_sched_per_cycle);
  store_alloc.clear_and_resize(sim_param::num_store_ports * sim_param::num_store_sched_per_cycle);
  
  initialize_rat_mappings();
}


void run_ooo_core(sim_state &machine_state) {
  gthread::make_gthread(&retire, reinterpret_cast<void*>(&machine_state));
  gthread::make_gthread(&complete,reinterpret_cast<void*>(&machine_state));
  gthread::make_gthread(&execute, reinterpret_cast<void*>(&machine_state));
  gthread::make_gthread(&allocate, reinterpret_cast<void*>(&machine_state));
  gthread::make_gthread(&decode, reinterpret_cast<void*>(&machine_state));
  gthread::make_gthread(&fetch, reinterpret_cast<void*>(&machine_state));
  gthread::make_gthread(&cache, reinterpret_cast<void*>(&machine_state));
  gthread::make_gthread(&cycle_count, reinterpret_cast<void*>(&machine_state));
  double now = timestamp();
  start_gthreads();
  now = timestamp() - now;

  
  uint64_t total_insns =  machine_state.icnt - machine_state.skipicnt;
  *global::sim_log << "executed " << total_insns << " insns\n";
  
  double ipc = static_cast<double>(total_insns) / get_curr_cycle();

  double allocd_insns_per_cycle =
    static_cast<double>(machine_state.total_allocated_insns) / get_curr_cycle();
  *global::sim_log << allocd_insns_per_cycle << " insns allocated per cycle\n";
  
  double ready_insns_per_cycle =
    static_cast<double>(machine_state.total_ready_insns) / get_curr_cycle();
  *global::sim_log << ready_insns_per_cycle << " insns ready per cycle\n";

  double dispatched_insns_per_cycle =
    static_cast<double>(machine_state.total_dispatched_insns) / get_curr_cycle();
  *global::sim_log << dispatched_insns_per_cycle << " insns dispatched per cycle\n";

#if 0
  *global::sim_log << "PC : "
		   << std::hex
		   << machine_state.last_retire_pc
		   << std::dec
		   << "\n";
  
  for(int i = 0; i < 32; i++) {
    *global::sim_log << getGPRName(i,0) << " : 0x"
		     << std::hex << machine_state.arch_grf[i] << std::dec
		     << "(" << machine_state.arch_grf[i] << ")\n";
  }
#endif
    
#if 0
  for(int i = 0; i < 32; i++) {
    std::cout << "reg " << getGPRName(i) << " : " 
	      << std::hex << machine_state.arch_grf[i]
	      << "," << s->gpr[i] << std::dec << "\n"; 
  }

  for(int i = 0; i < 32; i++) {
    std::cout << "reg " << getGPRName(i) << " writer pc : " 
	      << std::hex << machine_state.arch_grf_last_pc[i] << std::dec << "\n"; 
  }

  for(int i = 0; i < 32; i++) {
    std::cout << "cpr1 " << i << " : " 
	      << std::hex << machine_state.arch_cpr1[i] <<","
	      << s->cpr1[i] << std::dec << "\n"; 
  }
  for(int i = 0; i < 32; i++) {
    std::cout << "reg " << (i) << " writer pc : " 
	      << std::hex << machine_state.arch_cpr1_last_pc[i] << std::dec << "\n"; 
  }
#endif


  uint64_t retired_insns = (machine_state.icnt-machine_state.skipicnt);
  double nonsquash_fract = 100.0 *static_cast<double>(retired_insns) /
    static_cast<double>(machine_state.fetched_insns);
  
  //*global::sim_log << "SIMULATION COMPLETE : "
  //<< retired_insns
  //<< " inst retired in "
  //<< get_curr_cycle() << " cycles\n";
  
  *global::sim_log << machine_state.fetched_insns
		   << " fetched insns\n";
  *global::sim_log << nonsquash_fract << "% of fetched insns retire\n";
  
  *global::sim_log << ipc << " instructions/cycle\n";
  *global::sim_log << machine_state.n_branches << " branches\n";
  *global::sim_log << machine_state.mispredicted_branches 
	    << " mispredicted branches\n";
  
  double mpki = (machine_state.mispredicted_branches/static_cast<double>(total_insns))*1000.0;
  *global::sim_log << mpki << " MPKI\n";
  
  *global::sim_log << machine_state.n_jumps << " jumps\n";
  *global::sim_log << machine_state.mispredicted_jumps 
	    << " mispredicted jumps\n";
  *global::sim_log << machine_state.mispredicted_jrs 
	    << " mispredicted jrs\n";
  *global::sim_log << machine_state.mispredicted_jalrs 
	    << " mispredicted jalrs\n";

  *global::sim_log << machine_state.nukes << " nukes\n";
  *global::sim_log << machine_state.branch_nukes << " branch nukes\n";
  *global::sim_log << machine_state.load_nukes << " load nukes\n";
  
  //*global::sim_log << "CHECK INSN CNT : "
  //<< machine_state.ref_state->icnt << "\n";

  if(get_curr_cycle() != 0) {
    double avg_latency = 0.0;
    double d_cycles = static_cast<double>(get_curr_cycle());
    for(auto &p : insn_lifetime_map) {
      avg_latency += (static_cast<double>(p.first) * static_cast<double>(p.second)) / d_cycles;
    }
    mapToCSV(insn_lifetime_map,"insn_lifetime_map.csv");
    *global::sim_log << avg_latency << " cycles is the average instruction lifetime\n";
  }
  
  uint64_t total_branches_and_jumps = machine_state.n_branches + machine_state.n_jumps;
  uint64_t total_mispredicted = machine_state.mispredicted_branches + machine_state.mispredicted_jumps;
  double prediction_rate = static_cast<double>(total_branches_and_jumps - total_mispredicted) /
    total_branches_and_jumps;  
  *global::sim_log << (prediction_rate*100.0) << "\% of branches and jumps predicted correctly\n";
  
  *global::sim_log << ((machine_state.icnt-machine_state.skipicnt)/now)
	    << " simulated instructions per second\n";
  *global::sim_log << "simulation took " << now << " seconds\n";
  
}  

