#include <cassert>

#include "loop_predictor.hh"
#include "helper.hh"

loop_predictor::loop_predictor(uint32_t n_entries) : n_entries(n_entries) {
  assert(isPow2(n_entries));
  arr = new entry[n_entries];
  for(uint32_t i = 0; i < n_entries; i++) {
    arr[i].clear();
  }
}

loop_predictor::~loop_predictor() {
  delete [] arr;
}

bool loop_predictor::valid_loop_branch(uint32_t pc) const {
  const entry &e = arr[(pc>>2) & (n_entries-1)];
  return (e.pc == pc) and (e.state == entry_state::locked);
}

bool loop_predictor::predict(uint32_t pc) {
  assert(valid_loop_branch(pc));
  entry &e = arr[(pc>>2) & (n_entries-1)];
  bool p = true;
  if(e.count == e.limit) {
    p = false;
    e.count = 0;
  }
  else {
    e.count++;
  }
  return p;
    
}


void loop_predictor::update(uint32_t pc, bool take_br) {

}
