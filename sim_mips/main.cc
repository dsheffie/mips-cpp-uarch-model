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

char **sysArgv = nullptr;
int sysArgc = 0;
bool enClockFuncts = false;

static state_t *s =0;
int buildArgcArgv(char *filename, char *sysArgs, char ***argv);

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
