#include <cstdio>
#include <iostream>
#include <set>

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
  
  fetch_queue.resize(16);
  decode_queue.resize(8);
  rob.resize(2);

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

extern "C" {
  void cycle_count(void *arg) {
    while(not(machine_state.terminate_sim)) {
      curr_cycle++;
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
	//dprintf(2, "fetch pc %x\n", machine_state.fetch_pc);
	uint32_t inst = accessBigEndian(mem.get32(machine_state.fetch_pc));
	auto f = new mips_meta_op(machine_state.fetch_pc, inst,
				  machine_state.fetch_pc+4, curr_cycle);
	fetch_queue.push(f);
	machine_state.fetch_pc += 4;
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
	//if(u->op) {
	//dprintf(2, "op at pc %x was decoded\n", u->pc);
	//}
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
	if(busted_alloc_cnt == 16) {
	  dprintf(2, "@ %llu broken decode : %x breaks allocation\n", get_curr_cycle(), u->pc);
	  exit(-1);
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
	    dprintf(2, "want unknown for %x \n", u->pc);
	    break;
	  case mips_op_type::alu:
	    //dprintf(2, "want alu for %x \n", u->pc);
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
	    dprintf(2, "want fp for %x \n", u->pc);
	    break;
	  case mips_op_type::jmp:
	    //dprintf(2, "want jmp for %x \n", u->pc);
	    if(not(machine_state.jmp_rs.full())) {
	      rs_available = true;
	      rs_queue = &(machine_state.jmp_rs);
	    }
	    break;
	  case mips_op_type::mem:
	    //dprintf(2, "want mem rs for %x \n", u->pc);
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
	  //dprintf(2, "no rs for alloc @ %x this cycle \n", u->pc);
	  break;
	}
	
	/* just yield... */
	if(u->op == nullptr) {
	  dprintf(2, "stuck...\n");
	  exit(-1);
	  break;
	}
	if(not(u->op->allocate(machine_state))) {
	  busted_alloc_cnt++;
	  dprintf(2, "allocation failed...\n");
	  break;
	}

	dprintf(2, "@ %llu allocation for %x : prf_idx = %lld\n",
		get_curr_cycle(), u->pc, u->prf_idx);

	busted_alloc_cnt = 0;
	
	std::set<int32_t> gpr_prf_debug;
	for(int i = 0; i < 32; i++) {
	  //	  dprintf(2, "gpr[%d] maps to prf %d\n", i, machine_state.gpr_rat[i]);
	  auto it = gpr_prf_debug.find(machine_state.gpr_rat[i]);
	  gpr_prf_debug.insert(machine_state.gpr_rat[i]);
	}
	
	if(gpr_prf_debug.size() != 32) {
	  for(int i = 0; i < 32; i++) { 
	    dprintf(2, "gpr[%d] maps to prf %d\n", i, machine_state.gpr_rat[i]);
	  }
	  dprintf(2,"found %zu register mappings after %x allocs\n",
		  gpr_prf_debug.size(), u->pc);
	  exit(-1);
	}
	
	
	rs_queue->push(u);
	decode_queue.pop();
	u->alloc_cycle = curr_cycle;
	u->rob_idx = rob.push(u);
	//dprintf(2, "op at pc %x was allocated\n", u->pc);
	alloc_amt++;
      }
      //dprintf(2,"%d instr allocated\n", alloc_amt);
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
      if(not(machine_state.nuke)) {
	//alu loop
	for(int i = 0; i < machine_state.num_alu_rs; i++) {
	  if(not(alu_rs.at(i).empty()) ) {
	    if(alu_rs.at(i).peek()->op->ready(machine_state)) {
	      sim_op u = alu_rs.at(i).pop();
	      u->op->execute(machine_state);
	    }
	  }
	}
	if(not(jmp_rs.empty())) {
	  if(jmp_rs.peek()->op->ready(machine_state)) {
	    sim_op u = jmp_rs.pop();
	    u->op->execute(machine_state);
	  }
	}
	if(not(mem_rs.empty())) {
	  if(mem_rs.peek()->op->ready(machine_state)) {
	    sim_op u = mem_rs.pop();
	    u->op->execute(machine_state);
	  }
	}
	if(not(system_rs.empty())) {
	  if(system_rs.peek()->op->ready(machine_state)) {
	    sim_op u = system_rs.pop();
	    u->op->execute(machine_state);
	  }	
	}
      }
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
	    dprintf(2, "@ %llu : op @ %x is broken\n", get_curr_cycle(), rob.at(i)->pc);
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
      while(not(rob.empty()) and (retire_amt < retire_bw)) {
	
	u = rob.peek();
	//if(u->op->ready(machine_state)) {
	//dprintf(2, "head of rob could execute\n");
	//}
	
	if(not(u->is_complete)) {
	  u = nullptr;
	  break;
	}
	if(u->complete_cycle == curr_cycle) {
	  u = nullptr;
	  break;
	}


	 
	u->op->retire(machine_state);
	machine_state.retire_log.push_back(std::pair<uint32_t,uint32_t>(u->inst, u->pc));
	
	if(u->branch_exception) {
	  if(u->op->retired == false) {
	    dprintf(2,"-> %lu : how is this possible?\n", get_curr_cycle());
	    exit(-1);
	  }
	  break;
	}
	retire_amt++;
	rob.pop();
	
	if(u->pc == 0xa0020028) {
	  dprintf(2, "bad op alloc'd @ fetch_cycle %llu, decode_cycle %llu, alloc_cycle %llu\n", 
		  u->fetch_cycle, u->decode_cycle,u->alloc_cycle);
	  exit(-1);
	}	

	dprintf(2, "head of rob retiring for %x\n", u->pc);

	delete u;
	u = nullptr;
      }

      if(u!=nullptr and u->branch_exception) {
	dprintf(2, "%lu @ EXCEPTION @ %x, u = %p, complete %d, delay %d\n",
		get_curr_cycle(), u->pc, u, u->is_complete, u->has_delay_slot);
	rob.pop();
	int exception_cycles = 0;
	if(u->has_delay_slot) {
	  while(not(rob.empty())) {
	    auto uu = rob.peek();
	    dprintf(2, "waiting for %x to complete in delay slot, complete %d cycle %lld\n", 
		    uu->pc, uu->is_complete, get_curr_cycle());
	    while(not(uu->is_complete) or (uu->complete_cycle == get_curr_cycle())) {
	      dprintf(2, "waiting for %x to complete in delay slot, complete %d cycle %lld\n", 
		      uu->pc, uu->is_complete, get_curr_cycle());
	      exception_cycles++;
	      gthread_yield();
	    }
	    if(uu->branch_exception) {
	      dprintf(2, "branch exception in delay slot\n");
	      exit(-1);
	    }
	    dprintf(2, "branch delay insn @ %x retiring in exception cleanup\n",
		    uu->pc);
	    machine_state.retire_log.push_back(std::pair<uint32_t,uint32_t>(uu->inst, uu->pc));
	    uu->op->retire(machine_state);
	    rob.pop();
	    delete uu;
	    break;
	  }
	}
	machine_state.nuke = true;
	machine_state.fetch_pc = u->correct_pc;
	delete u;

	dprintf(2,"read ptr %d, write ptr %d\n", rob.get_read_idx(),
		rob.get_write_idx());

	for(size_t i = 0; i < rob.size(); i++) {
	  if(rob.at(i)) {
	    dprintf(2, "%d %x\n", i, rob.at(i)->pc);
	  }
	}
	
	int64_t i = rob.get_write_idx(), c = 0;
	while(true) {
	  auto uu = rob.at(i);
	  if(uu) {
	    dprintf(2, "rob slot %d from %x has prf id %d, prev prf id %d\n",
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
	    dprintf(2, "@ %llu : yield for undo\n", get_curr_cycle());
	    exception_cycles++;
	    gthread_yield();
	  }
	}
	rob.clear();

	if(exception_cycles == 0) {
	  exception_cycles++;
	  gthread_yield();
	}

	
	dprintf(2, "@ %llu : exception cleared in %d cycles, new pc %x!\n",
		get_curr_cycle(), exception_cycles, machine_state.fetch_pc);

	dprintf(2, "%lu gprs in use..\n", machine_state.gpr_freevec.popcount());

	std::set<int32_t> gpr_prf_debug;
	for(int i = 0; i < 32; i++) {
	  auto it = gpr_prf_debug.find(machine_state.gpr_rat[i]);
	  gpr_prf_debug.insert(machine_state.gpr_rat[i]);
	}
	dprintf(2,"found %zu register mappings\n",
		gpr_prf_debug.size());
	
	if(machine_state.gpr_freevec.popcount() != 32 or gpr_prf_debug.size() != 32) {
	  
	  for(int i = 0; i < machine_state.num_gpr_prf_; i++) {
	    if(not(machine_state.gpr_freevec.get_bit(i)))
	      continue;
	    auto it = gpr_prf_debug.find(i);
	    if(it == gpr_prf_debug.end()) {
	      dprintf(2, "no mapping to prf %d\n", i);
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
	machine_state.decode_queue.clear();
	machine_state.fetch_queue.clear();
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
    }
    gthread_terminate();
  }
  
};





int main(int argc, char *argv[]) {
  bool bigEndianMips = true;
#ifdef MIPSEL
  bigEndianMips = false;
#endif  
  fprintf(stderr, "%s%s INTERP: built %s %s%s\n",
	  KGRN, bigEndianMips ? "MIPS" : "MIPSEL" ,
	  __DATE__, __TIME__, KNRM);


  int c;
  size_t pgSize = getpagesize();
  char *filename = nullptr;
  char *sysArgs = nullptr;

  while((c=getopt(argc,argv,"a:cf:hi:jo:rt"))!=-1) {
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
    default:
      break;
    }
  }

  if(filename==nullptr) {
    fprintf(stderr, "INTERP : no file\n");
    exit(-1);
  }


  /* Build argc and argv */
  sysArgc = buildArgcArgv(filename,sysArgs,&sysArgv);
  initParseTables();

  sparse_mem *sm = new sparse_mem(1UL<<32);
  s = new state_t(*sm);
  initState(s);

  load_elf(filename, s);
  mkMonitorVectors(s);


  machine_state.initialize(sm);
  

  gthread::make_gthread(&cycle_count, nullptr);
  gthread::make_gthread(&fetch, nullptr);
  gthread::make_gthread(&decode, nullptr);
  gthread::make_gthread(&allocate, nullptr);
  gthread::make_gthread(&execute, nullptr);
  gthread::make_gthread(&complete, nullptr);
  gthread::make_gthread(&retire, nullptr);
  
  start_gthreads();
  for(auto &p : machine_state.retire_log) {
    std::cout << std::hex << p.second << std::dec << " : " 
	      << getAsmString(p.first, p.second) << "\n";
  }
  std::cout << "SIMULATION COMPLETE : "
	    << machine_state.icnt << " inst retired\n";
  return 0;
  
  double runtime = timestamp();
  while(s->brk==0)
    execMips(s);
  runtime = timestamp()-runtime;
  fprintf(stderr, "%sINTERP: %g sec, %zu ins executed, %g megains / sec%s\n", 
	  KGRN, runtime, (size_t)s->icnt, s->icnt / (runtime*1e6), KNRM);
  
  if(sysArgs)
    free(sysArgs);
  if(sysArgv) {
    for(int i = 0; i < sysArgc; i++) {
      delete [] sysArgv[i];
    }
    delete [] sysArgv;
  }

  std::cerr << sm->count() << " pages touched\n";
  delete s;
  delete sm;
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
