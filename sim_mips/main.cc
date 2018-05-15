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

int log_fd = 2;
char **sysArgv = nullptr;
int sysArgc = 0;
bool enClockFuncts = false;

static state_t *s =0;
int buildArgcArgv(char *filename, char *sysArgs, char ***argv);


/* sim parameters */
static int fetch_bw = 4;
static int alloc_bw = 2;
static int decode_bw = 2;
static int retire_bw = 2;

static int num_gpr_prf = 64;
static int num_cpr0_prf = 64;
static int num_cpr1_prf = 64;
static int num_fcr1_prf = 16;

static int num_fpu_ports = 1;
static int num_alu_ports = 2;

static int num_alu_sched_entries = 16;
static int num_fpu_sched_entries = 16;
static int num_jmp_sched_entries = 4;
static int num_mem_sched_entries = 16;
static int num_system_sched_entries = 2;

void sim_state::initialize(sparse_mem *mem) {
  this->mem = mem;
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

  gpr_valid.clear_and_resize(num_gpr_prf);
  cpr0_valid.clear_and_resize(num_cpr0_prf);
  cpr1_valid.clear_and_resize(num_cpr1_prf);
  fcr1_valid.clear_and_resize(num_fcr1_prf);
  
  fetch_queue.resize(4);
  decode_queue.resize(4);
  rob.resize(32);

  alu_rs.resize(num_alu_ports);
  for(int i = 0; i < num_alu_ports; i++) {
    alu_rs.at(i).resize(num_alu_sched_entries);
  }
  fpu_rs.resize(num_fpu_ports);

  num_alu_rs = num_alu_ports;
  num_fpu_rs = num_fpu_ports;
  
  for(int i = 0; i < num_fpu_ports; i++) {
    fpu_rs.at(i).resize(num_fpu_sched_entries);
  }
  jmp_rs.resize(num_jmp_sched_entries);
  mem_rs.resize(num_mem_sched_entries);
  system_rs.resize(num_system_sched_entries);
  
  initialize_rat_mappings();
}


static sim_state machine_state;
static uint64_t curr_cycle = 0;

uint64_t get_curr_cycle() {
  return curr_cycle;
}

extern std::map<uint32_t, uint32_t> branch_target_map;

extern "C" {
  void cycle_count(void *arg) {
    while(not(machine_state.terminate_sim)) {
      //dprintf(log_fd, "cycle %llu : icnt %llu\n", curr_cycle, machine_state.icnt);
      curr_cycle++;
      uint64_t delta = curr_cycle - machine_state.last_retire_cycle;
      if(delta > 1024) {
	dprintf(2, "no retirement in 1024 cycles, last pc = %x!\n",
		machine_state.last_retire_pc);
	machine_state.terminate_sim = true;
      }
      if(curr_cycle % (1UL<<18) == 0) {
	dprintf(2, "heartbeat : %llu cycles, %ld instr retired!\n",
		curr_cycle, machine_state.icnt);
      }
      //if(curr_cycle >= 256) {
      //machine_state.terminate_sim = true;
      //}
      gthread_yield();
    }
    gthread_terminate();
  }
  
  void fetch(void *arg) {
    machine_state.fetch_pc = s->pc;
    auto &fetch_queue = machine_state.fetch_queue;

    while(not(machine_state.terminate_sim)) {
      int fetch_amt = 0;
      for(; not(fetch_queue.full()) and (fetch_amt < fetch_bw) and not(machine_state.nuke); fetch_amt++) {
	sparse_mem &mem = s->mem;

	uint32_t inst = accessBigEndian(mem.get32(machine_state.fetch_pc));
	uint32_t npc = machine_state.fetch_pc + 4;
	auto it = branch_target_map.find(machine_state.fetch_pc);
	bool hit = false;
	//if(it != branch_target_map.end()) {
	//machine_state.delay_slot_npc = machine_state.fetch_pc + 4;
	// npc = it->second;
	//hit = true;
	//}
	dprintf(log_fd, "@%llu pc %x : predicting %x (hit %d)\n", 
		get_curr_cycle(), machine_state.fetch_pc, npc, hit);
	auto f = new mips_meta_op(machine_state.fetch_pc, inst, npc, curr_cycle);
	fetch_queue.push(f);
	machine_state.fetch_pc = npc;
      }
      gthread_yield();
    }
    gthread_terminate();
  }
  void decode(void *arg) {
    auto &fetch_queue = machine_state.fetch_queue;
    auto &decode_queue = machine_state.decode_queue;
    
    while(not(machine_state.terminate_sim)) {
      int decode_amt = 0;
      while(not(fetch_queue.empty()) and not(decode_queue.full()) and (decode_amt < decode_bw) and not(machine_state.nuke)) {
	auto u = fetch_queue.peek();
	if(not((u->fetch_cycle+5) < curr_cycle)) {
	  break;
	}
	fetch_queue.pop();
	u->decode_cycle = curr_cycle;
	u->op = decode_insn(u);
	decode_queue.push(u);
      }
      gthread_yield();
    }
    gthread_terminate();
  }

  void allocate(void *arg) {
    auto &decode_queue = machine_state.decode_queue;
    auto &rob = machine_state.rob;
    int busted_alloc_cnt = 0;

    while(not(machine_state.terminate_sim)) {
      int alloc_amt = 0;
      while(not(decode_queue.empty()) and not(rob.full()) and
	    (alloc_amt < alloc_bw) and not(machine_state.nuke)) {
	auto u = decode_queue.peek();
	if(busted_alloc_cnt == 64) {
	  dprintf(/* log_fd*/ 2, "@ %llu broken decode : (pc %x, insn %x) breaks allocation\n", 
		  get_curr_cycle(), u->pc, u->inst);
	  machine_state.terminate_sim = true;
	}
	if(u->op == nullptr) {
	  busted_alloc_cnt++;
	  break;
	}
	if(u->decode_cycle == curr_cycle) {
	  break;
	}
	
	sim_queue<sim_op> *rs_queue = nullptr;
	bool rs_available = false;
	switch(u->op->get_op_class())
	  {
	  case mips_op_type::unknown:
	    dprintf(log_fd, "want unknown for %x \n", u->pc);
	    break;
	  case mips_op_type::alu:
	    //dprintf(log_fd, "want alu for %x \n", u->pc);
	    for(int i = 0; i < machine_state.num_alu_rs; i++) {
	      int p = (i + machine_state.last_alu_rs) % machine_state.num_alu_rs;
	      if(not(machine_state.alu_rs.at(p).full())) {
		machine_state.last_alu_rs = p;
		rs_available = true;
		rs_queue = &(machine_state.alu_rs.at(p));
		break;
	      }
	    }
	    break;
	  case mips_op_type::fp:
	    dprintf(log_fd, "want fp for %x \n", u->pc);
	    break;
	  case mips_op_type::jmp:
	    //dprintf(log_fd, "want jmp for %x \n", u->pc);
	    if(not(machine_state.jmp_rs.full())) {
	      rs_available = true;
	      rs_queue = &(machine_state.jmp_rs);
	    }
	    break;
	  case mips_op_type::mem:
	    //dprintf(log_fd, "want mem rs for %x \n", u->pc);
	    if(not(machine_state.mem_rs.full())) {
	      rs_available = true;
	      rs_queue = &(machine_state.mem_rs);
	    }
	    break;
	  case mips_op_type::system:
	    if(not(machine_state.system_rs.full())) {
	      rs_available = true;
	      rs_queue = &(machine_state.system_rs);
	    }
	    break;
	  }
	
	if(not(rs_available)) {
	  //dprintf(log_fd, "no rs for alloc @ %x this cycle \n", u->pc);
	  break;
	}
	
	/* just yield... */
	if(u->op == nullptr) {
	  dprintf(log_fd, "stuck...\n");
	  exit(-1);
	  break;
	}
	if(not(u->op->allocate(machine_state))) {
	  busted_alloc_cnt++;
	  dprintf(log_fd, "allocation failed...\n");
	  break;
	}

	dprintf(log_fd, "@ %llu allocation for %x : prf_idx = %lld\n",
		get_curr_cycle(), u->pc, u->prf_idx);

	busted_alloc_cnt = 0;
	
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
	
	
	rs_queue->push(u);
	decode_queue.pop();
	u->alloc_cycle = curr_cycle;
	u->rob_idx = rob.push(u);
	//dprintf(log_fd, "op at pc %x was allocated\n", u->pc);
	alloc_amt++;
      }
      dprintf(log_fd,"%d instr allocated, decode queue empty %d, rob full %d\n", 
	      alloc_amt, decode_queue.empty(), rob.full());
      gthread_yield();
    }
    gthread_terminate();
  }

  void execute(void *arg) {
    auto & alu_rs = machine_state.alu_rs;
    auto & fpu_rs = machine_state.fpu_rs;
    auto & jmp_rs = machine_state.jmp_rs;
    auto & mem_rs = machine_state.mem_rs;
    auto & system_rs = machine_state.system_rs;
    while(not(machine_state.terminate_sim)) {
      int exec_cnt = 0;
      if(not(machine_state.nuke)) {
	//alu loop
	for(int i = 0; i < machine_state.num_alu_rs; i++) {
	  if(not(alu_rs.at(i).empty()) ) {
	    if(alu_rs.at(i).peek()->op->ready(machine_state)) {
	      sim_op u = alu_rs.at(i).pop();
	      u->op->execute(machine_state);
	      exec_cnt++;
	    }
	  }
	}
	if(not(jmp_rs.empty())) {
	  if(jmp_rs.peek()->op->ready(machine_state)) {
	    sim_op u = jmp_rs.pop();
	    u->op->execute(machine_state);
	    exec_cnt++;
	  }
	}
	if(not(mem_rs.empty())) {
	  if(mem_rs.peek()->op->ready(machine_state)) {
	    sim_op u = mem_rs.pop();
	    u->op->execute(machine_state);
	    exec_cnt++;
	  }
	}
	if(not(system_rs.empty())) {
	  if(system_rs.peek()->op->ready(machine_state)) {
	    sim_op u = system_rs.pop();
	    u->op->execute(machine_state);
	    exec_cnt++;
	  }	
	}
      }
      dprintf(log_fd, "%d instructions began exec @ %llu\n", exec_cnt, get_curr_cycle());
      gthread_yield();
    }
    gthread_terminate();
  }
  void complete(void *arg) {
    auto &rob = machine_state.rob;
    while(not(machine_state.terminate_sim)) {
      for(size_t i = 0; not(machine_state.nuke) and (i < rob.size()); i++) {
	if((rob.at(i) != nullptr) and not(rob.at(i)->is_complete)) {
	  if(rob.at(i)->op == nullptr) {
	    dprintf(log_fd, "@ %llu : op @ %x is broken\n", get_curr_cycle(), rob.at(i)->pc);
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
    auto &rob = machine_state.rob;
    while(not(machine_state.terminate_sim)) {
      int retire_amt = 0;
      sim_op u = nullptr;
      bool stop_sim = false;
      while(not(rob.empty()) and (retire_amt < retire_bw)) {
	
	u = rob.peek();
	//if(u->op->ready(machine_state)) {
	//dprintf(log_fd, "head of rob could execute\n");
	//}
	
	if(not(u->is_complete)) {
	  u = nullptr;
	  break;
	}
	if(u->complete_cycle == curr_cycle) {
	  u = nullptr;
	  break;
	}
	if(s->pc == u->pc) {
	  bool error = false;
	  for(int i = 0; i < 32; i++) {
	    if(s->gpr[i] != machine_state.arch_grf[i]) {
	      std::cout << "uarch reg " << getGPRName(i) << " : " 
			<< std::hex << machine_state.arch_grf[i] << std::dec << "\n"; 
	      std::cout << "func reg " << getGPRName(i) << " : " 
			<< std::hex << s->gpr[i] << std::dec << "\n"; 
	      error = true;
	    }
	  }
	  for(int i = 0; i < 32; i++) {
	    if(s->cpr1[i] != machine_state.arch_cpr1[i]) {
	      std::cout << "uarch cpr1 " << i << " : " 
			<< std::hex << machine_state.arch_cpr1[i] << std::dec << "\n"; 
	      std::cout << "func cpr1 " << i << " : " 
			<< std::hex << s->cpr1[i] << std::dec << "\n"; 
	      error = true;
	    }
	  }
	  if(error) {
	    dprintf(2, "%x : UARCH and FUNC simulator mismatch after %llu func and %llu uarch insn!\n",
		    u->pc, s->icnt, machine_state.icnt);
	    exit(-1);
	  }
	  execMips(s);
	}
	u->op->retire(machine_state);
	machine_state.log_insn(u->inst, u->pc, u->exec_parity);
	machine_state.last_retire_cycle = get_curr_cycle();
	machine_state.last_retire_pc = u->pc;
	
	if(u->branch_exception) {
	  if(u->op->retired == false) {
	    dprintf(log_fd,"-> %lu : how is this possible?\n", get_curr_cycle());
	    exit(-1);
	  }
	  break;
	}

	dprintf(log_fd, "head of rob retiring for %x, rob.full() = %d\n", u->pc, rob.full());
	retire_amt++;
	rob.pop();
	
	stop_sim = u->op->stop_sim();

	delete u;
	u = nullptr;
	if(stop_sim) {
	  break;
	}
      }

      if(u!=nullptr and u->branch_exception) {
	dprintf(log_fd, "%lu @ EXCEPTION @ %x, rob.full = %d, rob.empty() = %d, complete %d, delay %d\n",
		get_curr_cycle(), u->pc, rob.full(), rob.empty(), u->is_complete, u->has_delay_slot);
	rob.pop();
	int exception_cycles = 0;
	if(u->has_delay_slot and u->likely_squash) {
	  dprintf(log_fd,"impossible to have both delay lot and likely squash!!!\n");
	  exit(-1);
	}
	if(u->has_delay_slot) {
	  /* wait for branch delay instr to allocate */
	  while(rob.empty()) {
	    exception_cycles++;
	    gthread_yield();
	  }

	  while(not(rob.empty())) {
	    auto uu = rob.peek();
	    dprintf(log_fd, "waiting for %x to complete in delay slot, complete %d cycle %lld\n", 
		    uu->pc, uu->is_complete, get_curr_cycle());
	    while(not(uu->is_complete) or (uu->complete_cycle == get_curr_cycle())) {
	      dprintf(log_fd, "waiting for %x to complete in delay slot, complete %d cycle %lld\n", 
		      uu->pc, uu->is_complete, get_curr_cycle());
	      exception_cycles++;
	      gthread_yield();
	    }
	    if(uu->branch_exception) {
	      dprintf(log_fd, "branch exception in delay slot\n");
	      exit(-1);
	    }
	    dprintf(log_fd, "branch delay insn @ %x retiring in exception cleanup\n",
		    uu->pc);

	    machine_state.log_insn(uu->inst, uu->pc, uu->exec_parity);
	    uu->op->retire(machine_state);
	    machine_state.last_retire_cycle = get_curr_cycle();
	    machine_state.last_retire_pc = uu->pc;
	    if(s->pc == uu->pc) {
	      execMips(s);
	    }
	    rob.pop();
	    delete uu;
	    break;
	  }
	}
	machine_state.nuke = true;
	machine_state.fetch_pc = u->correct_pc;
	delete u;

	dprintf(log_fd,"read ptr %d, write ptr %d\n", rob.get_read_idx(),
		rob.get_write_idx());

	for(size_t i = 0; i < rob.size(); i++) {
	  if(rob.at(i)) {
	    dprintf(log_fd, "%d %x\n", i, rob.at(i)->pc);
	  }
	}
	
	int64_t i = rob.get_write_idx(), c = 0;
	while(true) {
	  auto uu = rob.at(i);
	  if(uu) {
	    dprintf(log_fd, "rob slot %d from %x has prf id %d, prev prf id %d\n",
		    i, uu->pc, uu->prf_idx, uu->prev_prf_idx);
	    uu->op->undo(machine_state);
	    delete uu;
	    rob.at(i) = nullptr;
	  }
	  
	  if(i == rob.get_read_idx()) {
	    break;
	  }
	  i--;
	  if(i < 0) {
	    i = rob.size()-1;
	  }
	  c++;
	  if(c % retire_bw == 0) {
	    dprintf(log_fd, "@ %llu : yield for undo\n", get_curr_cycle());
	    exception_cycles++;
	    gthread_yield();
	  }
	}
	rob.clear();

	if(exception_cycles == 0) {
	  exception_cycles++;
	  gthread_yield();
	}

	
	dprintf(log_fd, "@ %llu : exception cleared in %d cycles, new pc %x!\n",
		get_curr_cycle(), exception_cycles, machine_state.fetch_pc);

	dprintf(log_fd, "%lu gprs in use..\n", machine_state.gpr_freevec.popcount());

	std::set<int32_t> gpr_prf_debug;
	for(int i = 0; i < 34; i++) {
	  auto it = gpr_prf_debug.find(machine_state.gpr_rat[i]);
	  gpr_prf_debug.insert(machine_state.gpr_rat[i]);
	}
	dprintf(log_fd,"found %zu register mappings\n",
		gpr_prf_debug.size());
	
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
	//for(uint64_t i = 0; i < rob.size(); i++) {
	//int64_t p = (i + rob.get_read_idx()) % rob.size();
	//}

	if(exception_cycles == 0) {
	  gthread_yield();
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
	
	machine_state.decode_queue.clear();
	machine_state.fetch_queue.clear();
	machine_state.delay_slot_npc = 0;
	for(int i = 0; i < machine_state.num_alu_rs; i++) {
	  machine_state.alu_rs.at(i).clear();
	}
	for(int i = 0; i < machine_state.num_fpu_rs; i++) {
	  machine_state.fpu_rs.at(i).clear();
	}
	machine_state.jmp_rs.clear();
	machine_state.mem_rs.clear();
	machine_state.system_rs.clear();

	machine_state.nuke = false;
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
  
};





int main(int argc, char *argv[]) {
  bool bigEndianMips = true;
#ifdef MIPSEL
  bigEndianMips = false;
#endif  
  fprintf(stderr, "%s%s UARCH SIM: built %s %s%s\n",
	  KGRN, bigEndianMips ? "MIPS" : "MIPSEL" ,
	  __DATE__, __TIME__, KNRM);


  int c;
  size_t pgSize = getpagesize();
  char *filename = nullptr;
  char *sysArgs = nullptr;
  uint64_t maxicnt = ~(0UL);
  while((c=getopt(argc,argv,"a:cf:m:"))!=-1) {
    switch(c) {
    case 'a':
      sysArgs = strdup(optarg);
      break;
    case 'c':
      enClockFuncts = true;
      break;
    case 'f':
      filename = optarg;
      break;
    case 'm':
      maxicnt = atoi(optarg);
      break;
    default:
      break;
    }
  }

  if(filename==nullptr) {
    fprintf(stderr, "INTERP : no file\n");
    exit(-1);
  }

  log_fd = open("/dev/null", O_WRONLY);

  /* Build argc and argv */
  sysArgc = buildArgcArgv(filename,sysArgs,&sysArgv);
  initParseTables();

  sparse_mem *sm = new sparse_mem(1UL<<32);
  s = new state_t(*sm);
  initState(s);

  load_elf(filename, s);
  mkMonitorVectors(s);

  sparse_mem *u_arch_mem = new sparse_mem(*sm);
  machine_state.initialize(u_arch_mem);
  machine_state.maxicnt = maxicnt;

  gthread::make_gthread(&cycle_count, nullptr);
  gthread::make_gthread(&fetch, nullptr);
  gthread::make_gthread(&decode, nullptr);
  gthread::make_gthread(&allocate, nullptr);
  gthread::make_gthread(&execute, nullptr);
  gthread::make_gthread(&complete, nullptr);
  gthread::make_gthread(&retire, nullptr);

  double now = timestamp();
  start_gthreads();
  now = timestamp() - now;

  uint32_t parity = 0;
  for(int i = 0; i < 32; i++) {
    parity ^= machine_state.arch_grf[i];
  }

  for(int i = 0; i < 32; i++) {
    std::cout << "reg " << getGPRName(i) << " : " 
	      << std::hex << machine_state.arch_grf[i] << std::dec << "\n"; 
  }
  std::cout << "parity = " << std::hex <<  parity << std::dec << "\n";

  for(int i = 0; i < 32; i++) {
    std::cout << "reg " << getGPRName(i) << " writer pc : " 
	      << std::hex << machine_state.arch_grf_last_pc[i] << std::dec << "\n"; 
  }
  if(machine_state.log_execution) {
    std::ofstream os("log.txt", std::ios::out);
    for(auto &p : machine_state.retire_log) {
      os << std::hex << p.pc << std::dec << " : " 
	 << getAsmString(p.inst, p.pc)
	 << "\n";
    }
    os.close();
  }
  std::cout << "SIMULATION COMPLETE : "
	    << machine_state.icnt << " inst retired in "
	    << get_curr_cycle() << " cycles\n";


  std::cout << machine_state.n_branches << " branches\n";
  std::cout << machine_state.mis_predicted_branches 
	    << " mispredicted branches\n";
  std::cout << machine_state.n_jumps << " jumps\n";
  std::cout << "CHECK INSN CNT : "
	    << s->icnt << "\n";

    std::cout << (machine_state.icnt/now)
	      << " simulated instructions per second\n";

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

  delete s;
  delete sm;
  delete u_arch_mem;
  
  if(sysArgs)
    free(sysArgs);
  if(sysArgv) {
    for(int i = 0; i < sysArgc; i++) {
      delete [] sysArgv[i];
    }
    delete [] sysArgv;
  }

  return 0;
}

int buildArgcArgv(char *filename, char *sysArgs, char ***argv)
{
  int cnt = 0;
  std::vector<std::string> args;
  char **largs = 0;
  args.push_back(std::string(filename));

  char *ptr = 0;
  if(sysArgs)
    ptr = strtok(sysArgs, " ");

  while(ptr && (cnt<MARGS))
    {
      args.push_back(std::string(ptr));
      ptr = strtok(nullptr, " ");
      cnt++;
    }
  largs = new char*[args.size()];
  for(size_t i = 0; i < args.size(); i++)
    {
      std::string s = args[i];
      size_t l = strlen(s.c_str());
      largs[i] = new char[l+1];
      memset(largs[i],0,sizeof(char)*(l+1));
      memcpy(largs[i],s.c_str(),sizeof(char)*l);
    }
  *argv = largs;
  return (int)args.size();
}
