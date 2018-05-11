#include <cstdio>
#include <iostream>
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


void sim_state::initialize() {  
  num_gpr_prf_ = num_gpr_prf;
  num_cpr0_prf_ = num_cpr0_prf;
  num_cpr1_prf_ = num_cpr1_prf;
  num_fcr1_prf_ = num_fcr1_prf;
  
  gpr_prf = new int32_t[num_gpr_prf_];
  cpr0_prf = new uint32_t[num_cpr0_prf_];
  cpr1_prf = new uint32_t[num_cpr1_prf_];
  fcr1_prf = new uint32_t[num_fcr1_prf_];
  
  gpr_freevec.clear_and_resize(num_gpr_prf);
  cpr0_freevec.clear_and_resize(num_cpr0_prf);
  cpr1_freevec.clear_and_resize(num_cpr1_prf);
  fcr1_freevec.clear_and_resize(num_fcr1_prf);

  fetch_queue.resize(4);
  decode_queue.resize(8);
  rob.resize(16);
  
  initialize_rat_mappings();
}


static sim_state machine_state;
static uint64_t curr_cycle = 0;


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
      for(; not(fetch_queue.full()) and (fetch_amt < fetch_bw); fetch_amt++) {
	sparse_mem &mem = s->mem;
	uint32_t inst = accessBigEndian(mem.get32(machine_state.fetch_pc));
	auto f = std::make_shared<mips_meta_op>(machine_state.fetch_pc, machine_state.fetch_pc+4, inst, curr_cycle);
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
      while(not(fetch_queue.empty()) and not(decode_queue.full()) and (decode_amt < decode_bw)) {
	auto u = fetch_queue.peek();
	if(u->fetch_cycle == curr_cycle) {
	  break;
	}
	fetch_queue.pop();
	u->decode_cycle = curr_cycle;
	
	decode_queue.push(u);
      }
      gthread_yield();
    }
    gthread_terminate();
  }

  void allocate(void *arg) {
    auto &decode_queue = machine_state.decode_queue;
    auto &rob = machine_state.rob;

    while(not(machine_state.terminate_sim)) {
      int alloc_amt = 0;
      while(not(decode_queue.empty()) and not(rob.full()) and (alloc_amt < alloc_bw)) {
	auto u = decode_queue.peek();
	if(u->decode_cycle == curr_cycle) {
	  break;
	}

	decode_queue.pop();
	u->alloc_cycle = curr_cycle;
	u->rob_idx = rob.push(u);
	alloc_amt++;
      }
      gthread_yield();
    }
    gthread_terminate();
  }

  void retire(void *arg) {
    auto &rob = machine_state.rob;
    while(not(machine_state.terminate_sim)) {
      int retire_amt = 0;
      while(not(rob.empty()) and (retire_amt < retire_bw)) {
	machine_state.terminate_sim = true;
	
	auto u = rob.peek();
	if(not(u->complete)) {
	  break;
	}
	if(u->complete_cycle == curr_cycle) {
	  break;
	}

	
	rob.pop();
	retire_amt++;
      }
      gthread_yield();
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


  machine_state.initialize();
  

  gthread::make_gthread(&cycle_count, nullptr);
  gthread::make_gthread(&fetch, nullptr);
  gthread::make_gthread(&decode, nullptr);
  gthread::make_gthread(&allocate, nullptr);
  gthread::make_gthread(&retire, nullptr);
  
  start_gthreads();
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
