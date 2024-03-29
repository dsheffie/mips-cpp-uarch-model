#ifndef __bpred_hh__
#define __bpred_hh__

#include <cstdint>
#include <map>
#include "counter2b.hh"

class sim_state;

class branch_predictor {
protected:
  sim_state &machine_state;
public:
  static branch_predictor* get_predictor(int id, sim_state &ms);
  branch_predictor(sim_state &ms) : machine_state(ms) {}
  virtual ~branch_predictor() {}
  virtual uint32_t predict(uint64_t &idx) const = 0;
  virtual void update(uint32_t addr, uint64_t idx, bool taken) = 0;
};




#endif
