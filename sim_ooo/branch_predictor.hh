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
  static const int fetch_state_sz = 1024;
  static branch_predictor* get_predictor(int id, sim_state &ms);
  branch_predictor(sim_state &ms) : machine_state(ms) {}
  virtual ~branch_predictor() {}
  virtual uint32_t predict(uint32_t pc, uint32_t fetch_token) = 0;
  virtual void update(uint32_t pc, uint32_t fetch_token, uint32_t target, bool taken) = 0;
};




#endif
