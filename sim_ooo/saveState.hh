#ifndef __SAVE_STATE_HH__
#define __SAVE_STATE_HH__

#include <string>
#include "state.hh"

void loadState(state_t &s, const std::string &filename, bool clr_icnt = false);

#endif
