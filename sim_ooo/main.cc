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

#include "sim_parameters.hh"
#include "loadelf.hh"
#include "save_state.hh"
#include "helper.hh"
#include "parseMips.hh"
#include "profileMips.hh"
#include "globals.hh"

#include "mips_op.hh"

char **sysArgv = nullptr;
int sysArgc = 0;
bool enClockFuncts = false;

state_t *s = nullptr;

int sim_param::rob_size = 64;
int sim_param::fetchq_size = 8;
int sim_param::decodeq_size = 8;
  
int sim_param::fetch_bw = 8;
int sim_param::decode_bw = 6;
int sim_param::alloc_bw = 6;
int sim_param::retire_bw = 6;
  
int sim_param::num_gpr_prf = 128;
int sim_param::num_cpr0_prf = 64;
int sim_param::num_cpr1_prf = 64;
int sim_param::num_fcr1_prf = 16;
int sim_param::num_fpu_ports = 2;
int sim_param::num_alu_ports = 2;
  
int sim_param::num_load_ports = 2;
int sim_param::num_store_ports = 1;
  
int sim_param::num_alu_sched_entries = 64;
int sim_param::num_fpu_sched_entries = 64;
int sim_param::num_jmp_sched_entries = 64;
int sim_param::num_load_sched_entries = 64;
int sim_param::num_store_sched_entries = 64;
int sim_param::num_system_sched_entries = 4;
  
int sim_param::load_tbl_size = 16;
int sim_param::store_tbl_size = 16;
int sim_param::taken_branches_per_cycle = 1;

int buildArgcArgv(char *filename, char *sysArgs, char ***argv);

void initialize_ooo_core(sim_state & machine_state,
			 bool use_oracle,
			 bool use_syscall_skip,
			 uint64_t skipicnt, uint64_t maxicnt,
			 state_t *s, const sparse_mem *sm);

void run_ooo_core(sim_state &machine_state);
void destroy_ooo_core(sim_state &machine_state);

int main(int argc, char *argv[]) {
  std::cerr << "MIPS UARCH SIM: built " << __DATE__
	    << " " << __TIME__ << "\n";
  sim_state machine_state;
  int c;
  size_t pgSize = getpagesize();
  char *filename = nullptr;
  char *sysArgs = nullptr;
  uint64_t maxicnt = ~(0UL), skipicnt = 0;
  bool use_checkpoint = false, use_oracle = false, use_syscall_skip = false;
  
  while((c=getopt(argc,argv,"a:cf:m:k:o:p:s:"))!=-1) {
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
    case 'k':
      skipicnt = atoi(optarg);
      break;
    case 'o':
      use_oracle = atoi(optarg);
      break;
    case 'p':
      use_checkpoint = atoi(optarg);
      break;
    case 's':
      use_syscall_skip = atoi(optarg);
      break;
    default:
      break;
    }
  }

  if(filename==nullptr) {
    std::cerr << "UARCH SIM : no file\n";
    return -1;
  }

  /* Build argc and argv */
  sysArgc = buildArgcArgv(filename,sysArgs,&sysArgv);
  initParseTables();

  sparse_mem *sm = new sparse_mem(1UL<<32);
  s = new state_t(*sm);
  initState(s);

  if(not(use_checkpoint)) {
    load_elf(filename, s);
    mkMonitorVectors(s);
  }
  else {
    loadState(*s, filename);
  }

  initialize_ooo_core(machine_state, use_oracle, use_syscall_skip, skipicnt, maxicnt, s, sm);
  run_ooo_core(machine_state);
  destroy_ooo_core(machine_state);


  delete s;
  delete sm;
  
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
