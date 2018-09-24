#ifndef __GLOBALS_HH__
#define __GLOBALS_HH__

#include <cstdint>
#include <ostream>

namespace global {
  extern bool enClockFuncts;
  extern int sysArgc;
  extern char **sysArgv;
  extern std::ostream *sim_log;
  extern bool use_interp_check;
  extern uint64_t curr_cycle;
};

inline uint64_t get_curr_cycle() {
  return global::curr_cycle;
}


#endif
