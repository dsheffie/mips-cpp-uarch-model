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
#include <boost/program_options.hpp>

#include "loadelf.hh"
#include "save_state.hh"
#include "helper.hh"
#include "parseMips.hh"
#include "profileMips.hh"
#include "globals.hh"
#include "mips_op.hh"

#define SAVE_SIM_PARAM_LIST
#include "sim_parameters.hh"

char **sysArgv = nullptr;
int sysArgc = 0;
bool enClockFuncts = false;

state_t *s = nullptr;

#define SIM_PARAM(A,B) int sim_param::A =  B;
SIM_PARAM_LIST;
#undef SIM_PARAM

int buildArgcArgv(const char *filename, const char *sysArgs, char ***argv);

void initialize_ooo_core(sim_state & machine_state,
			 bool use_oracle,
			 bool use_syscall_skip,
			 uint64_t skipicnt, uint64_t maxicnt,
			 state_t *s, const sparse_mem *sm);

void run_ooo_core(sim_state &machine_state);
void destroy_ooo_core(sim_state &machine_state);

int main(int argc, char *argv[]) {
  namespace po = boost::program_options;
  
  std::cerr << "MIPS UARCH SIM: built " << __DATE__
	    << " " << __TIME__ << "\n";
  sim_state machine_state;

  std::string filename, sysArgs;
  uint64_t maxicnt = ~(0UL), skipicnt = 0;
  bool use_checkpoint = false, use_oracle = false, use_syscall_skip = false;

  try {
    po::options_description desc("Options");
    desc.add_options() 
      ("help,h", "Print help messages") 
      ("args,a", po::value<std::string>(&sysArgs), "arguments to mips binary")
      ("clock,c", po::value<bool>(&enClockFuncts), "enable wall-clock")
      ("file,f", po::value<std::string>(&filename), "mips binary")
      ("skipicnt,k", po::value<uint64_t>(&skipicnt), "instruction skip count")
      ("maxicnt,m", po::value<uint64_t>(&maxicnt), "maximum instruction count")
      ("oracle,o", po::value<bool>(&use_oracle), "use branch oracle")
      ("checkpoint,p", po::value<bool>(&use_checkpoint), "use a machine checkpoint")
      ("syscallskip,s", po::value<bool>(&use_syscall_skip), "skip syscalls")
#define SIM_PARAM(A,B) (#A,po::value<int>(&sim_param::A), #A)
      SIM_PARAM_LIST;
#undef SIM_PARAM
      ; 
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm); 
  }
  catch(po::error &e) {
    std::cerr << KRED << "command-line error : " << e.what() << KNRM << "\n";
    return -1;
  }
  
  if(filename.size()==0) {
    std::cerr << "UARCH SIM : no file\n";
    return -1;
  }

  /* Build argc and argv */
  sysArgc = buildArgcArgv(filename.c_str(),sysArgs.c_str(),&sysArgv);
  initParseTables();

  sparse_mem *sm = new sparse_mem(1UL<<32);
  s = new state_t(*sm);
  initState(s);

  if(not(use_checkpoint)) {
    load_elf(filename.c_str(), s);
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
  
  if(sysArgv) {
    for(int i = 0; i < sysArgc; i++) {
      delete [] sysArgv[i];
    }
    delete [] sysArgv;
  }


  return 0;
}

int buildArgcArgv(const char *filename, const char *sysArgs, char ***argv) {
  int cnt = 0;
  std::vector<std::string> args;
  char **largs = 0;
  args.push_back(std::string(filename));

  char *ptr = nullptr, *sa = nullptr;
  if(sysArgs) {
    sa = strdup(sysArgs);
    ptr = strtok(sa, " ");
  }

  while(ptr && (cnt<MARGS)) {
    args.push_back(std::string(ptr));
    ptr = strtok(nullptr, " ");
    cnt++;
  }
  largs = new char*[args.size()];
  for(size_t i = 0; i < args.size(); i++) {
    std::string s = args[i];
    size_t l = strlen(s.c_str());
    largs[i] = new char[l+1];
    memset(largs[i],0,sizeof(char)*(l+1));
    memcpy(largs[i],s.c_str(),sizeof(char)*l);
  }
  *argv = largs;
  if(sysArgs) {
    free(sa);
  }
  return (int)args.size();
}
