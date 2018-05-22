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

state_t *s = nullptr;
int buildArgcArgv(char *filename, char *sysArgs, char ***argv);

void initialize_ooo_core(uint64_t maxicnt, const state_t *s, const sparse_mem *sm);
void run_ooo_core();
void destroy_ooo_core();

int main(int argc, char *argv[]) {
  bool bigEndianMips = true;
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

  //while(s->icnt < (1UL<<28)) {
   //execMips(s);
  //if((s->icnt % 8192) == 0)
  //std::cout << s->icnt << "\n";
  //}
  //std::cout << "fast-forwarded over " << s->icnt << " insns\n";
  
  //sparse_mem *u_arch_mem = new sparse_mem(*sm);
  initialize_ooo_core(maxicnt, s, sm);
  run_ooo_core();
  destroy_ooo_core();


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
