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
#include "parseMips.hh"
#include "profileMips.hh"
#include "globals.hh"
#include "gthread.hh"

#include "mips_op.hh"

/* sim parameters */
static int rob_size = 64;
static int fetchq_size = 8;
static int decodeq_size = 8;

static int fetch_bw = 8;
static int decode_bw = 6;
static int alloc_bw = 6;
static int retire_bw = 6;

static int num_gpr_prf = 128;
static int num_cpr0_prf = 64;
static int num_cpr1_prf = 64;
static int num_fcr1_prf = 16;
static int num_fpu_ports = 2;
static int num_alu_ports = 2;
static int num_load_ports = 2;

static int num_alu_sched_entries = 64;
static int num_fpu_sched_entries = 64;
static int num_jmp_sched_entries = 64;
static int num_load_sched_entries = 64;
static int num_store_sched_entries = 64;
static int num_system_sched_entries = 4;

static int taken_branches_per_cycle = 1;

static uint64_t curr_cycle = 0;
extern std::map<uint32_t, uint32_t> branch_target_map;
extern std::map<uint32_t, int32_t> branch_prediction_map;

static std::map<int64_t, uint64_t> insn_lifetime_map;

uint64_t get_curr_cycle() {
  return curr_cycle;
}

class undo_rob_entry : public sim_queue<sim_op>::funcobj {
protected:
  sim_state &machine_state;
public:
  undo_rob_entry(sim_state &machine_state) :
    machine_state(machine_state) {}
  virtual bool operator()(sim_op e){
    if(e) {
      e->op->undo(machine_state);
      delete e;
      return true;
    }
    return false;
  }
};

template <bool enable_oracle>
void fetch(sim_state &machine_state) {
  auto &fetch_queue = machine_state.fetch_queue;
  auto &return_stack = machine_state.return_stack;
  sparse_mem &mem = *(machine_state.mem);
  
  while(not(machine_state.terminate_sim)) {
    int fetch_amt = 0, taken_branches = 0;
    for(; not(fetch_queue.full()) and (fetch_amt < fetch_bw) and not(machine_state.nuke); fetch_amt++) {
      
      if(machine_state.delay_slot_npc) {
	uint32_t inst = accessBigEndian(mem.get32(machine_state.delay_slot_npc));
	auto f = new mips_meta_op(machine_state.delay_slot_npc, inst,
				  machine_state.delay_slot_npc+4,
				  curr_cycle, false, false);
	fetch_queue.push(f);
	machine_state.delay_slot_npc = 0;
	if(enable_oracle) {
	  machine_state.oracle_state->steps--;
	}
	continue;
      }
      
      uint32_t inst = accessBigEndian(mem.get32(machine_state.fetch_pc));
      uint32_t npc = machine_state.fetch_pc + 4;
      bool predict_taken = false;
      bool oracle_taken = false, oracle_nullify = false;

      if(enable_oracle) {
	if(machine_state.oracle_state->steps == 0 and not(machine_state.oracle_state->brk)) {
	  assert(machine_state.oracle_state->pc == machine_state.fetch_pc);
	  machine_state.oracle_state->was_branch_or_jump = false;
	  machine_state.oracle_state->was_likely_branch = false;
	  machine_state.oracle_state->took_branch_or_jump = false;
	  execMips(machine_state.oracle_state);
	  machine_state.oracle_state->steps--;
	  if(machine_state.oracle_state->was_branch_or_jump and
	     machine_state.oracle_state->took_branch_or_jump) {
	    oracle_taken = true;
	  }
	  else if(machine_state.oracle_state->was_likely_branch and
		  not(machine_state.oracle_state->took_branch_or_jump)) {
	    oracle_nullify = true;
	  }
	}
	else {
	  machine_state.oracle_state->steps--;
	}
      }
	
      auto it = branch_prediction_map.find(machine_state.fetch_pc);
      bool used_return_addr_stack = false;
      
      mips_meta_op *f = new mips_meta_op(machine_state.fetch_pc, inst, curr_cycle);
      
      if(enable_oracle) {
	if(oracle_taken) {
	  machine_state.delay_slot_npc = machine_state.fetch_pc + 4;
	  npc = machine_state.oracle_state->pc;
	  predict_taken = true;
	}
	else if(oracle_nullify) {
	  npc = machine_state.fetch_pc + 8;
	}
      }
      else {
	if(is_jr(inst)) {
	  if(not(return_stack.empty())) {
#if 0
	    f->shadow_rstack.copy(return_stack);
	    std::cerr << "JR SHADOW STACK\n";
	    while(not(f->shadow_rstack.empty())) {
	      std::cerr << "\t" << std::hex << f->shadow_rstack.pop() << std::dec << "\n";
	    }
#endif
	    f->shadow_rstack.copy(return_stack);
	    npc = return_stack.pop();
	    machine_state.delay_slot_npc = machine_state.fetch_pc + 4;
	    used_return_addr_stack = true;
#if 0
	    std::cerr << "found jr at fetch pc " << std::hex
		      << machine_state.fetch_pc << ", pop "
		      << npc
		      << std::dec
		      << " @ cycle " << get_curr_cycle()
		      << "\n";
#endif
	  }
	}
	else if(is_jal(inst) or is_j(inst)) {
	  machine_state.delay_slot_npc = machine_state.fetch_pc + 4;
	  npc = get_jump_target(machine_state.fetch_pc, inst);
	  predict_taken = true;
	}
	else if(it != branch_prediction_map.end()) {
	  /* predicted as taken */
	  if(it->second > 1) {
	    machine_state.delay_slot_npc = machine_state.fetch_pc + 4;
	    npc = branch_target_map.at(machine_state.fetch_pc);
	    predict_taken = true;
	  }
	}
	else if(is_likely_branch(inst)) {
	  machine_state.delay_slot_npc = machine_state.fetch_pc + 4;
	  npc = get_branch_target(machine_state.fetch_pc, inst);
	  predict_taken = true;
	}
	else if(is_branch(inst)) {
	  uint32_t target = get_branch_target(machine_state.fetch_pc, inst);
	  if(target < machine_state.fetch_pc) {
	    machine_state.delay_slot_npc = machine_state.fetch_pc + 4;
	    npc = get_branch_target(machine_state.fetch_pc, inst);
	    predict_taken = true;
	  }
	}
      }
	
      f->fetch_npc = npc;
      f->predict_taken = predict_taken;
      f->pop_return_stack = used_return_addr_stack;
      
      fetch_queue.push(f);
      machine_state.fetch_pc = npc;
      if(predict_taken)
	taken_branches++;
      
      if(taken_branches == taken_branches_per_cycle)
	break;
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
      
    while(not(rob.empty()) and (retire_amt < retire_bw)) {
      u = rob.peek();
      empty_cnt = 0;
      if(not(u->is_complete)) {
	if(stuck_cnt > 32) {
	  std::cerr << "STUCK:" << *(u->op) << "\n";
	}
	stuck_cnt++;
	u = nullptr;
	break;
      }
      if(u->complete_cycle == curr_cycle) {
	u = nullptr;
	break;
      }

	
      if(u->branch_exception) {
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
	  gthread_yield();
	}
	sim_op uu = rob.peek_next_pop();
	while(not(uu->is_complete)) {
	  gthread_yield();
	}
	if(uu->branch_exception or uu->load_exception) {
	  machine_state.nukes++;
	  if(uu->branch_exception) {
	    machine_state.branch_nukes++;
	  }
	  else {
	    machine_state.load_nukes++;
	  }
	  exception = true;
	  break;
	}
      }

      if(machine_state.use_interp_check and (s->pc == u->pc)) {
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
	for(int i = 0; i < 32; i++) {
	  if(s->cpr1[i] != machine_state.arch_cpr1[i]) {
	    std::cerr << "uarch cpr1 " << i << " : " 
		      << std::hex << machine_state.arch_cpr1[i] << std::dec << "\n"; 
	    std::cerr << "func cpr1 " << i << " : " 
		      << std::hex << s->cpr1[i] << std::dec << "\n";
	    error = true;
	  }
	}

	for(int i = 0; i < 5; i++) {
	  if(s->fcr1[i] != machine_state.arch_fcr1[i]) {
	    std::cerr << "uarch fcr1 " << i << " : " 
		      << std::hex << machine_state.arch_fcr1[i]
		      << std::dec << "\n"; 
	    std::cerr << "func fcr1 " << i << " : " 
		      << std::hex << s->fcr1[i]
		      << std::dec << "\n";

	    error = true;
	  }
	}
	  
	  	  
	if(u->is_store and false) {
	  error |= (machine_state.mem->equal(s->mem)==false);
	}
	if(error) {
	  std::cerr << "bad insn : " << std::hex << u->pc << ":" << std::dec
		    << getAsmString(u->inst, u->pc)
		    << " after " << machine_state.icnt << " uarch isns and "
		    << s->icnt << " arch isns\n";
	  std::cerr << "known good at pc " << std::hex << machine_state.last_compare_pc
		    << std::dec << " after " << machine_state.last_compare_icnt
		    << " isnsns\n";
	  std::cerr << "execMips call site = " << s->call_site << "\n";
	  machine_state.terminate_sim = true;
	  break;
	}
	else {
	  machine_state.last_compare_pc = u->pc;
	  machine_state.last_compare_icnt = machine_state.icnt;
	}
	s->call_site = __LINE__;
	execMips(s);
      }

      //std::cerr << "retire for " << *(u->op) << "\n";
      u->op->retire(machine_state);
      stuck_cnt = 0;
	
      insn_lifetime_map[u->retire_cycle - u->fetch_cycle]++;
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
#if 0
      std::cerr << (u->branch_exception ? "BRANCH" : "LOAD")
		<< " EXCEPTION @ cycle " << get_curr_cycle()
		<< " for " << *(u->op)
		<< " fetched @ cycle " << u->fetch_cycle
		<< " with prf idx  " << u->prf_idx
		<< "\n";
#endif
      assert(u->could_cause_exception);
      machine_state.return_stack.copy(u->shadow_rstack);
	
      //while(!u->shadow_rstack.empty()) {
      //std::cerr << std::hex << u->shadow_rstack.pop() << std::dec << "\n";
      //}
	
      if((retire_amt - retire_bw) < 2) {
	gthread_yield();
	retire_amt = 0;
      }
	
      bool is_load_exception = u->load_exception;
      uint32_t exc_pc = u->pc;
      bool delay_slot_exception = false;
      if(u->branch_exception) {
	if(u->has_delay_slot) {
	  /* wait for branch delay instr to allocate */
	  while(rob.peek_next_pop() == nullptr) {
	    gthread_yield();
	    retire_amt = 0;
	  }
	  sim_op uu = rob.peek_next_pop();
	  while(not(uu->is_complete) or (uu->complete_cycle == get_curr_cycle())) {
	    gthread_yield();
	    retire_amt = 0;
	  }
	  if(uu->branch_exception or uu->load_exception) {
	    delay_slot_exception = true;
	  }
	  else {
	    //std::cerr << "retire for " << *(u->op) << "\n";
	    u->op->retire(machine_state);
	    insn_lifetime_map[u->retire_cycle - u->fetch_cycle]++;
	    machine_state.last_retire_cycle = get_curr_cycle();
	    machine_state.last_retire_pc = u->pc;
	    //std::cout << std::hex << u->pc << ":" << std::hex
	    //<< getAsmString(u->inst, u->pc) << "\n";

	    //std::cerr << "retire for " << *(uu->op) << "\n";
	    uu->op->retire(machine_state);
	    machine_state.last_retire_cycle = get_curr_cycle();
	    machine_state.last_retire_pc = uu->pc;
	    insn_lifetime_map[uu->retire_cycle - uu->fetch_cycle]++;
	    //std::cout << std::hex << uu->pc << ":" << std::hex
	    //<< getAsmString(uu->inst, uu->pc) << "\n";
	    if(machine_state.use_interp_check and (s->pc == u->pc)) {
	      s->call_site = __LINE__;
	      execMips(s);
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
	  insn_lifetime_map[u->retire_cycle - u->fetch_cycle]++;
	  machine_state.last_retire_cycle = get_curr_cycle();
	  machine_state.last_retire_pc = u->pc;
	  if(machine_state.use_interp_check and (s->pc == u->pc)) {
	    s->call_site = __LINE__;
	    execMips(s);
	  }
	  rob.pop();
	  retire_amt++;
	}
      }
      machine_state.nuke = true;
      stuck_cnt = 0;
      if(delay_slot_exception) {
	machine_state.fetch_pc = u->pc;
      }
      else {
	machine_state.fetch_pc = u->branch_exception ? u->correct_pc : u->pc;
	if(u->branch_exception) {
	  delete u;
	}
      }

      undo_rob_entry undo_rob(machine_state);
      int64_t c = rob.traverse_and_apply(undo_rob);
      int64_t sleep_cycles = (c + retire_bw - 1) / retire_bw;
      for(int64_t i = 0; i < sleep_cycles; i++) {
	gthread_yield();
      }
      rob.clear();
	
      std::set<int32_t> gpr_prf_debug;
      for(int i = 0; i < 34; i++) {
	auto it = gpr_prf_debug.find(machine_state.gpr_rat[i]);
	gpr_prf_debug.insert(machine_state.gpr_rat[i]);
      }
	
      if(gpr_prf_debug.size() != 34) {
	  
	for(int i = 0; i < machine_state.num_gpr_prf_; i++) {
	  if(not(machine_state.gpr_freevec.get_bit(i)))
	    continue;
	  auto it = gpr_prf_debug.find(i);
	  if(it == gpr_prf_debug.end()) {
	    dprintf(log_fd, "no mapping to prf %d\n", i);
	  }
	}
	exit(-1);
      }

      for(size_t i = 0; i < machine_state.fetch_queue.size(); i++) {
	auto f = machine_state.fetch_queue.at(i);
	if(f) {
	  delete f;
	}
      }
      for(size_t i = 0; i < machine_state.decode_queue.size(); i++) {
	auto d = machine_state.decode_queue.at(i);
	if(d) {
	  delete d;
	}
      }
      //machine_state.return_stack.clear();
      machine_state.decode_queue.clear();
      machine_state.fetch_queue.clear();
      machine_state.delay_slot_npc = 0;
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
      machine_state.store_rs.clear();
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

      if(enable_oracle) {
	machine_state.oracle_state->copy(machine_state.ref_state);
	machine_state.oracle_mem->copy(machine_state.ref_state->mem);
      }
	
      gthread_yield();
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
			 bool use_oracle,
			 uint64_t skipicnt, uint64_t maxicnt,
			 state_t *s, const sparse_mem *sm) {
  while(s->icnt < skipicnt) {
    execMips(s);
  }
  if(false and (skipicnt==0)) {
    while(s->syscall==0) {
      execMips(s);
    }
    s->pc+=4;
  }
  
  //s->debug = 1;
  machine_state.ref_state = s;

  if(use_oracle) {
    machine_state.oracle_mem = new sparse_mem(*sm);
    machine_state.oracle_state = new state_t(*machine_state.oracle_mem);
    machine_state.oracle_state->copy(s);
  }
  machine_state.mem = new sparse_mem(*sm);
  machine_state.initialize();
  machine_state.maxicnt = maxicnt;
  machine_state.skipicnt = s->icnt;
  machine_state.use_interp_check = true;
  machine_state.copy_state(s);
  //u_arch_mem->mark_pages_as_no_write();
}


void destroy_ooo_core(sim_state &machine_state) {
  for(size_t i = 0; i < machine_state.fetch_queue.size(); i++) {
    auto f = machine_state.fetch_queue.at(i);
    if(f) {
      delete f;
    }
  }
  for(size_t i = 0; i < machine_state.decode_queue.size(); i++) {
    auto d = machine_state.decode_queue.at(i);
    if(d) {
      delete d;
    }
  }
  for(size_t i = 0; i < machine_state.rob.size(); i++) {
    auto r = machine_state.rob.at(i);
    if(r) {
      delete r;
    }
  }
  if(machine_state.oracle_mem) {
    delete machine_state.oracle_mem;
  }
  if(machine_state.oracle_state) {
    delete machine_state.oracle_state;
  }
  delete machine_state.mem;
  gthread::free_threads();
}



extern "C" {
  void cycle_count(void *arg) {
    sim_state &machine_state = *reinterpret_cast<sim_state*>(arg);
    uint64_t prev_icnt = 0;
    static const uint64_t hinterval = 1UL<<20;
    while(not(machine_state.terminate_sim)) {
      //dprintf(log_fd, "cycle %llu : icnt %llu\n", curr_cycle, machine_state.icnt);
      curr_cycle++;
      uint64_t delta = curr_cycle - machine_state.last_retire_cycle;
      if(delta > 1024) {
	dprintf(2, "no retirement in 1024 cycles, last pc = %x!\n",
		machine_state.last_retire_pc);
	machine_state.terminate_sim = true;
      }
      if(curr_cycle % hinterval == 0) {
	uint64_t curr_icnt = (machine_state.icnt-machine_state.skipicnt);
	double ipc = static_cast<double>(curr_icnt) / curr_cycle;
	double wipc = static_cast<double>(curr_icnt-prev_icnt) / hinterval;
	std::cout << "heartbeat : " << curr_cycle << " cycles, "
		  << curr_icnt << " insns retired, avg ipc "
		  << ipc << ", window ipc "
		  << wipc <<"\n";
	prev_icnt = curr_icnt;
      }
      //if(curr_cycle >= 256) {
      //machine_state.terminate_sim = true;
      //}
      gthread_yield();
    }
    gthread_terminate();
  }
  
  void fetch(void *arg) {
    sim_state &machine_state = *reinterpret_cast<sim_state*>(arg);
    if(machine_state.oracle_mem) {
      fetch<true>(machine_state);
    }
    else {
      fetch<false>(machine_state);
    }
  }
  void decode(void *arg) {
    sim_state &machine_state = *reinterpret_cast<sim_state*>(arg);
    auto &fetch_queue = machine_state.fetch_queue;
    auto &decode_queue = machine_state.decode_queue;
    auto &return_stack = machine_state.return_stack;
    while(not(machine_state.terminate_sim)) {
      int decode_amt = 0;
      while(not(fetch_queue.empty()) and not(decode_queue.full()) and (decode_amt < decode_bw) and not(machine_state.nuke)) {
	auto u = fetch_queue.peek();
	if(not((u->fetch_cycle+1) < curr_cycle)) {
	  break;
	}
	fetch_queue.pop();
	u->decode_cycle = curr_cycle;
	u->op = decode_insn(u);
	decode_queue.push(u);
	if(u->is_jal) {
	  if(not(return_stack.full())) {
#if 0
	    std::cerr << std::hex
		      << u->pc 
		      << " : push return address " 
		      << u->pc+8 
		      << std::dec
		      << " @ cycle " << get_curr_cycle()
		      << "\n";
#endif
	    u->push_return_stack = true;
	    return_stack.push(u->pc + 8);
	  }
	}
	if(u->could_cause_exception and not u->is_jr) {
	  u->shadow_rstack.copy(return_stack);
	}
	if(u->is_branch_or_jump) {
	  break;
	}
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
    int64_t alloc_counter = 0;
    while(not(machine_state.terminate_sim)) {
      int alloc_amt = 0;
      std::map<mips_op_type, int> alloc_histo;
      alu_alloc.clear();
      fpu_alloc.clear();
      load_alloc.clear();
      while(not(decode_queue.empty()) and not(rob.full()) and
	    (alloc_amt < alloc_bw) and not(machine_state.nuke)) {
	auto u = decode_queue.peek();
	if(u->op == nullptr) {
	  std::cout << "decode failed : " << std::hex << u->pc << ":" << std::dec
		    << getAsmString(u->inst, u->pc) << "\n";
	  break;
	}
	if(u->decode_cycle == curr_cycle) {
	  break;
	}
	bool jmp_avail = true, store_avail = true, system_avail = true;
	sim_state::rs_type *rs_queue = nullptr;
	bool rs_available = false;

	switch(u->op->get_op_class())
	  {
	  case mips_op_type::unknown:
	    dprintf(log_fd, "want unknown for %x \n", u->pc);
	    die();
	  case mips_op_type::alu: {
	    int64_t p = alu_alloc.find_first_unset();
	    if(p!=-1 and not(machine_state.alu_rs.at(p).full())) {
		rs_available = true;
		rs_queue = &(machine_state.alu_rs.at(p));
		alu_alloc.set_bit(p);
		alloc_histo[u->op->get_op_class()]++;
	    }
	  }
	    break;
	  case mips_op_type::fp: {
	    int64_t p = fpu_alloc.find_first_unset();
	    if(p!=-1 and not(machine_state.fpu_rs.at(p).full())) {
	      rs_available = true;
	      rs_queue = &(machine_state.fpu_rs.at(p));
	      fpu_alloc.set_bit(p);
	      alloc_histo[u->op->get_op_class()]++;
	    }
	  }
	    break;
	  case mips_op_type::jmp:
	    if(jmp_avail and not(machine_state.jmp_rs.full())) {
	      rs_available = true;
	      rs_queue = &(machine_state.jmp_rs);
	      alloc_histo[u->op->get_op_class()]++;
	      jmp_avail = false;
	    }
	    break;
	  case mips_op_type::load: {
	    int64_t p = load_alloc.find_first_unset();
	    if(p!=-1 and not(machine_state.load_rs.at(p).full())) {
		rs_available = true;
		rs_queue = &(machine_state.load_rs.at(p));
		load_alloc.set_bit(p);
		alloc_histo[u->op->get_op_class()]++;
	    }
	    break;
	  }
	  case mips_op_type::store:
	    if(store_avail and not(machine_state.store_rs.full())) {
	      rs_available = true;
	      rs_queue = &(machine_state.store_rs);
	      alloc_histo[u->op->get_op_class()]++;
	      store_avail = false;
	    }
	    break;
	  case mips_op_type::system:
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
		    << " resources\n";
#endif
	  break;
	}
	
	/* just yield... */
	assert(u->op != nullptr);

	if(not(u->op->allocate(machine_state))) {
	  break;
	}
	u->alloc_id = alloc_counter++;
#if 0
	std::set<int32_t> gpr_prf_debug;
	for(int i = 0; i < 32; i++) {
	  //	  dprintf(log_fd, "gpr[%d] maps to prf %d\n", i, machine_state.gpr_rat[i]);
	  auto it = gpr_prf_debug.find(machine_state.gpr_rat[i]);
	  gpr_prf_debug.insert(machine_state.gpr_rat[i]);
	}
	
	if(gpr_prf_debug.size() != 32) {
	  for(int i = 0; i < 32; i++) { 
	    dprintf(log_fd, "gpr[%d] maps to prf %d\n", i, machine_state.gpr_rat[i]);
	  }
	  dprintf(log_fd,"found %zu register mappings after %x allocs\n",
		  gpr_prf_debug.size(), u->pc);
	  exit(-1);
	}
#endif
	//std::cout << "allocated " << *(u->op) << " with prf " << u->prf_idx << "\n";
	rs_queue->push(u);
	decode_queue.pop();
	u->alloc_cycle = curr_cycle;
	u->rob_idx = rob.push(u);
	//dprintf(log_fd, "op at pc %x was allocated\n", u->pc);
	alloc_amt++;
      }

      if(false and get_curr_cycle()%8192 == 0 and alloc_amt != 0) {
	std::cout << "alloc amt " << alloc_amt << "\n";
	for(auto &ap : alloc_histo) {
	  std::cout << "\talloc'd " << ap.second << " of " << ap.first << "\n";
	}
      }


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
	//alu loop (OoO scheduler)
#define OOO_SCHED(RS) {							\
	  for(auto it = RS.begin(); it != RS.end(); it++) {		\
	    sim_op u = *it;						\
	    if((u->ready_cycle==-1) and u->op->ready(machine_state)) {	\
	      u->ready_cycle = get_curr_cycle();			\
	    }								\
	  }								\
	  for(auto it = RS.begin(); it != RS.end(); it++) {		\
	    sim_op u = *it;						\
	    if(u->op->ready(machine_state)) {				\
	      RS.erase(it);						\
	      u->op->execute(machine_state);				\
	      exec_cnt++;						\
	      break;							\
	    }								\
	  }								\
	}
	
#define INORDER_SCHED(RS) {						\
	  for(auto it = RS.begin(); it != RS.end(); it++) {		\
	    sim_op u = *it;						\
	    if((u->ready_cycle==-1) and u->op->ready(machine_state)) {	\
	      u->ready_cycle = get_curr_cycle();			\
	    }								\
	  }								\
	  if(not(RS.empty())) {						\
	    if(RS.peek()->op->ready(machine_state)) {			\
	      sim_op u = RS.pop();					\
	      u->op->execute(machine_state);				\
	      exec_cnt++;						\
	    }								\
	  }								\
	}
	
	for(int i = 0; i < machine_state.num_alu_rs; i++) {
	  OOO_SCHED(alu_rs.at(i));
	}
	OOO_SCHED(jmp_rs);
	for(int i = 0; i < machine_state.num_load_rs; i++) {
	  OOO_SCHED(load_rs.at(i));
	}
	/* not really out-of-order as stores are processed
	 * at retirement */
	OOO_SCHED(store_rs);
	
	for(int i = 0; i < machine_state.num_fpu_rs; i++) {
	  OOO_SCHED(fpu_rs.at(i));
	}

	INORDER_SCHED(system_rs);

#undef OOO_SCHED
#undef INORDER_SCHED
      }
      gthread_yield();
    }
    gthread_terminate();
  }
  void complete(void *arg) {
    sim_state &machine_state = *reinterpret_cast<sim_state*>(arg);
    auto &rob = machine_state.rob;
    while(not(machine_state.terminate_sim)) {
      for(size_t i = 0; not(machine_state.nuke) and (i < rob.size()); i++) {
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
  gpr_prf[gpr_rat[32]] = s->lo;
  gpr_prf[gpr_rat[33]] = s->hi;

  for(int i = 0; i < 32; i++) {
    cpr0_prf[cpr0_rat[i]] = s->cpr0[i];
  }
  for(int i = 0; i < 32; i++) {
    cpr1_prf[cpr1_rat[i]] = s->cpr1[i];
    arch_cpr1[i] = s->cpr1[i];    
  }
  for(int i = 0; i < 5; i++) {
    fcr1_prf[fcr1_rat[i]] = s->fcr1[i];
    arch_fcr1[i] = s->fcr1[i];
  }
  icnt = s->icnt;
}


void sim_state::initialize_rat_mappings() {
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

void sim_state::initialize() {
  num_gpr_prf_ = num_gpr_prf;
  num_cpr0_prf_ = num_cpr0_prf;
  num_cpr1_prf_ = num_cpr1_prf;
  num_fcr1_prf_ = num_fcr1_prf;
  
  gpr_prf = new int32_t[num_gpr_prf_];
  memset(gpr_prf, 0, sizeof(int32_t)*num_gpr_prf_);
  cpr0_prf = new uint32_t[num_cpr0_prf_];
  memset(cpr0_prf, 0, sizeof(uint32_t)*num_cpr0_prf_);  
  cpr1_prf = new uint32_t[num_cpr1_prf_];
  memset(cpr1_prf, 0, sizeof(uint32_t)*num_cpr1_prf_);
  fcr1_prf = new uint32_t[num_fcr1_prf_];
  memset(fcr1_prf, 0, sizeof(uint32_t)*num_fcr1_prf_);
  
  gpr_freevec.clear_and_resize(num_gpr_prf);
  cpr0_freevec.clear_and_resize(num_cpr0_prf);
  cpr1_freevec.clear_and_resize(num_cpr1_prf);
  fcr1_freevec.clear_and_resize(num_fcr1_prf);
  
  load_tbl_freevec.clear_and_resize(8);
  load_tbl = new mips_meta_op*[8];

  store_tbl_freevec.clear_and_resize(64);
  store_tbl = new mips_meta_op*[64];
  
  for(size_t i = 0; i < load_tbl_freevec.size(); i++) {
    load_tbl[i] = nullptr;
  }
  for(size_t i = 0; i < store_tbl_freevec.size(); i++) {
    store_tbl[i] = nullptr;
  }
  
  gpr_valid.clear_and_resize(num_gpr_prf);
  cpr0_valid.clear_and_resize(num_cpr0_prf);
  cpr1_valid.clear_and_resize(num_cpr1_prf);
  fcr1_valid.clear_and_resize(num_fcr1_prf);
  
  fetch_queue.resize(fetchq_size);
  decode_queue.resize(decodeq_size);
  rob.resize(rob_size);

  num_alu_rs = num_alu_ports;
  num_fpu_rs = num_fpu_ports;
  num_load_rs = num_load_ports;
  
  alu_rs.resize(num_alu_ports);
  for(int i = 0; i < num_alu_ports; i++) {
    alu_rs.at(i).resize(num_alu_sched_entries);
  }

  fpu_rs.resize(num_fpu_ports);
  for(int i = 0; i < num_fpu_ports; i++) {
    fpu_rs.at(i).resize(num_fpu_sched_entries);
  }

  load_rs.resize(num_load_ports);
  for(int i = 0; i < num_load_ports; i++) {
    load_rs.at(i).resize(num_load_sched_entries);
  }
  jmp_rs.resize(num_jmp_sched_entries);
  store_rs.resize(num_store_sched_entries);
  system_rs.resize(num_system_sched_entries);

  alu_alloc.clear_and_resize(num_alu_rs);
  fpu_alloc.clear_and_resize(num_fpu_rs);
  load_alloc.clear_and_resize(num_load_rs);
  initialize_rat_mappings();
}


void run_ooo_core(sim_state &machine_state) {
  gthread::make_gthread(&retire, reinterpret_cast<void*>(&machine_state));
  gthread::make_gthread(&complete,reinterpret_cast<void*>(&machine_state));
  gthread::make_gthread(&execute, reinterpret_cast<void*>(&machine_state));
  gthread::make_gthread(&allocate, reinterpret_cast<void*>(&machine_state));
  gthread::make_gthread(&decode, reinterpret_cast<void*>(&machine_state));
  gthread::make_gthread(&fetch, reinterpret_cast<void*>(&machine_state));
  gthread::make_gthread(&cycle_count, reinterpret_cast<void*>(&machine_state));
  double now = timestamp();
  start_gthreads();
  now = timestamp() - now;

  uint64_t total_insns =  machine_state.icnt - machine_state.skipicnt;
  std::cout << "executed " << total_insns << " insns\n";
  
  double ipc = static_cast<double>(machine_state.icnt-machine_state.skipicnt) /
    get_curr_cycle();

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
  std::cout << "SIMULATION COMPLETE : "
	    << (machine_state.icnt-machine_state.skipicnt)
	    << " inst retired in "
	    << get_curr_cycle() << " cycles\n";

  std::cout << ipc << " instructions/cycle\n";
  std::cout << machine_state.n_branches << " branches\n";
  std::cout << machine_state.mispredicted_branches 
	    << " mispredicted branches\n";

  
  std::cout << machine_state.n_jumps << " jumps\n";
  std::cout << machine_state.mispredicted_jumps 
	    << " mispredicted jumps\n";
  
  std::cout << machine_state.nukes << " nukes\n";
  std::cout << machine_state.branch_nukes << " branch nukes\n";
  std::cout << machine_state.load_nukes << " load nukes\n";
  std::cout << "CHECK INSN CNT : "
	    << machine_state.ref_state->icnt << "\n";

  if(get_curr_cycle() != 0) {
    double avg_latency = 0;
    for(auto &p : insn_lifetime_map) {
      //std::cout << p.first << " cycles : " << p.second << " insns\n";
      avg_latency += (p.first * p.second);
    }
    avg_latency /= get_curr_cycle();
    std::cout << avg_latency << " cycles is the average instruction lifetime\n";
  }
  
  uint64_t total_branches_and_jumps = machine_state.n_branches + machine_state.n_jumps;
  uint64_t total_mispredicted = machine_state.mispredicted_branches + machine_state.mispredicted_jumps;
  double prediction_rate = static_cast<double>(total_branches_and_jumps - total_mispredicted) /
    total_branches_and_jumps;  
  std::cout << (prediction_rate*100.0) << "\% of branches and jumps predicted correctly\n";
  
  std::cout << ((machine_state.icnt-machine_state.skipicnt)/now)
	    << " simulated instructions per second\n";

  
}  
