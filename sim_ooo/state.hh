#ifndef __state_hh__
#define __state_hh__

#include <fstream>
#include "interpret.hh"


static const int HWINDOW = (1<<16);
static const uint32_t K1SIZE = 0x80000000;

class simCache;

struct history_t {
  uint32_t fetch_pc = 0;
  uint32_t next_pc = 0;
  bool was_branch_or_jump = false;
  bool was_likely_branch = false;
  bool took_branch_or_jump = false;
  uint64_t icnt;
};

#endif
