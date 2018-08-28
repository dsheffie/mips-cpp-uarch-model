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

char **global::sysArgv = nullptr;
int global::sysArgc = 0;
bool global::enClockFuncts = false;

state_t *s = nullptr;

/* linkage */
#define SIM_PARAM(A,B,C) int sim_param::A = C;
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
  bool use_checkpoint = false, use_oracle = false;
  bool use_syscall_skip = false, use_mem_model = false;
  
  po::options_description desc("Options");
  po::variables_map vm;
  
  desc.add_options() 
    ("help,h", "Print help messages") 
    ("args,a", po::value<std::string>(&sysArgs), "arguments to mips binary")
    ("clock,c", po::value<bool>(&global::enClockFuncts)->default_value(false), "enable wall-clock")
    ("file,f", po::value<std::string>(&filename), "mips binary")
    ("skipicnt,k", po::value<uint64_t>(&skipicnt)->default_value(0), "instruction skip count")
    ("maxicnt,m", po::value<uint64_t>(&maxicnt)->default_value(~0UL), "maximum instruction count")
    ("oracle,o", po::value<bool>(&use_oracle)->default_value(false), "use branch oracle")
    ("checkpoint,p", po::value<bool>(&use_checkpoint)->default_value(false), "use a machine checkpoint")
    ("syscallskip,s", po::value<bool>(&use_syscall_skip)->default_value(false), "skip syscalls")
    ("mem_model", po::value<bool>(&use_mem_model)->default_value(false), "use memory model")
#define SIM_PARAM(A,B,C) (#A,po::value<int>(&sim_param::A)->default_value(B), #A)
    SIM_PARAM_LIST;
#undef SIM_PARAM
  ; 
  
  try {
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm); 
  }
  catch(po::error &e) {
    std::cerr << KRED << "command-line error : " << e.what() << KNRM << "\n";
    return -1;
  }

  if(vm.count("help")) {
    std::cout << desc << "\n";
    return 0;
  }
  
  if(filename.size()==0) {
    std::cerr << "UARCH SIM : no file\n";
    return -1;
  }

#define SIM_PARAM(A,B,C) if(sim_param::A < C) {		\
    std::cout << #A << " has out of range value "	\
	      << sim_param::A				\
	      <<" -> reset to default value "		\
	      << B << "\n";				\
    sim_param::A = B;					\
  }
  SIM_PARAM_LIST;
#undef SIM_PARAM

  
  /* Build argc and argv */
  global::sysArgc = buildArgcArgv(filename.c_str(),sysArgs.c_str(),&global::sysArgv);
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
  
  if(global::sysArgv) {
    for(int i = 0; i < global::sysArgc; i++) {
      delete [] global::sysArgv[i];
    }
    delete [] global::sysArgv;
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
