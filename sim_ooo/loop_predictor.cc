#include <cassert>
#include <iostream>

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

bool loop_predictor::predict(uint32_t pc, int32_t &count) {
  assert(valid_loop_branch(pc));
  entry &e = arr[(pc>>2) & (n_entries-1)];
  bool p = true;
  count = e.count;
  if(e.count == e.limit) {
    p = false;
    e.count = 0;
  }
  else {
    e.count++;
  }
  //std::cout << "e.count = " << e.count << ",count = " << count << "\n";
  return p;
    
}


void loop_predictor::update(uint32_t pc, bool take_br, bool prediction, int32_t count) {
  entry &e = arr[(pc>>2) & (n_entries-1)];
  bool pc_match = (e.pc == pc);

  if(not(pc_match)) {
    e.clear();
  }

  switch(e.state)
    {
    case entry_state::invalid:
      e.pc = pc;
      e.state = entry_state::training;
      e.limit = 1;
      break;
    case entry_state::training:
      if(not(take_br)) {
	/*
	std::cout << "Branch at " << std::hex << pc << std::dec << " locked with trip count of "
		  << e.limit << "\n";
	*/
	e.state = entry_state::locked;
      }
      else {
	e.limit++;
      }
      break;
    case entry_state::locked:
      //std::cout << "Prediction for branch at " << std::hex << pc << std::dec
      //<< " was "
      //<< ((take_br == prediction)?"correct":"wrong")
      //	<< ", count = " << count
      //	<< "\n";

      if(take_br xor prediction) {
	e.state = entry_state::invalid;
	//exit(-1);
      }

      break;
    }
  
  return;
}
