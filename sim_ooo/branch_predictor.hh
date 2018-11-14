#ifndef __bpred_hh__
#define __bpred_hh__

#include <cstdint>
#include "counter2b.hh"

class sim_state;

class branch_predictor {
protected:
  sim_state &machine_state;
public:
  branch_predictor(sim_state &ms);
  virtual ~branch_predictor();
  virtual uint32_t predict(uint64_t &idx) const;
  virtual void update(uint32_t addr, uint64_t idx, bool taken);
};

class gshare : protected branch_predictor {
protected:
  twobit_counter_array *pht;
public:
  gshare(sim_state &ms);
  ~gshare();
  uint32_t predict(uint64_t &idx) const override;
  void update(uint32_t addr, uint64_t idx, bool taken) override;
};


#endif
