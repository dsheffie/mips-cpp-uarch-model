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
  branch_predictor(sim_state &ms);
  virtual ~branch_predictor();
  virtual uint32_t predict(uint64_t &idx) const;
  virtual void update(uint32_t addr, uint64_t idx, bool taken);
};

class gshare : public branch_predictor {
protected:
  twobit_counter_array *pht;
public:
  gshare(sim_state &ms);
  ~gshare();
  uint32_t predict(uint64_t &idx) const override;
  void update(uint32_t addr, uint64_t idx, bool taken) override;
};

class gselect : public branch_predictor {
protected:
  twobit_counter_array *pht;
public:
  gselect(sim_state &ms);
  ~gselect();
  uint32_t predict(uint64_t &idx) const override;
  void update(uint32_t addr, uint64_t idx, bool taken) override;
};

class gnoalias : public branch_predictor {
protected:
  std::map<uint64_t, uint8_t> pht;
public:
  gnoalias(sim_state &ms);
  ~gnoalias();
  uint32_t predict(uint64_t &idx) const override;
  void update(uint32_t addr, uint64_t idx, bool taken) override;
};

class bimode : public branch_predictor {
protected:
  const uint32_t lg_d_pht_entries;
  const uint32_t lg_c_pht_entries;
  twobit_counter_array *c_pht, *t_pht, *nt_pht;
  
public:
  bimode(sim_state &ms);
  ~bimode();
  uint32_t predict(uint64_t &idx) const override;
  void update(uint32_t addr, uint64_t idx, bool taken) override;
};


#endif
