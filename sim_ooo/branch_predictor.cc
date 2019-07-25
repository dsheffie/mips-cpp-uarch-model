#include "branch_predictor.hh"
#include "machine_state.hh"
#include "globals.hh"
#include "sim_parameters.hh"


class gshare : public branch_predictor {
protected:
  twobit_counter_array *pht;
public:
  gshare(sim_state &ms) : branch_predictor(ms) {
    pht = new twobit_counter_array(1UL << sim_param::lg_pht_entries);
  }
  ~gshare() {
    delete pht;
  }
  uint32_t predict(uint64_t &idx) const override {
    idx = ((machine_state.fetch_pc>>2) ^ machine_state.bhr.hash());
    idx &= (1UL << sim_param::lg_pht_entries) - 1;
    return pht->get_value(idx);
  }
  void update(uint32_t addr, uint64_t idx, bool taken) override {
    pht->update(idx, taken);
  }
};

class gselect : public gshare {
protected:
  twobit_counter_array *pht;
public:
  gselect(sim_state &ms) : gshare(ms) {}
  ~gselect() {}
  uint32_t predict(uint64_t &idx) const override {
    uint64_t addr = static_cast<uint64_t>(machine_state.fetch_pc>>2);
    addr &= (1UL << sim_param::gselect_addr_bits) - 1;
    uint64_t hbits = static_cast<uint64_t>(machine_state.bhr.hash()) << sim_param::gselect_addr_bits;
    idx = addr | hbits;
    idx &= (1UL << sim_param::lg_pht_entries)-1;
    return pht->get_value(idx);
  }
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

branch_predictor* branch_predictor::get_predictor(int id, sim_state &ms) {
  switch(id)
    {
    case 6:
      return new gshare(ms);
    case 7:
      return new gselect(ms);
    case 8:
      return new gnoalias(ms);
    case 9:
      return new bimode(ms);
    default:
      break;
    }
  return new gshare(ms);
}
							     

branch_predictor::branch_predictor(sim_state &ms) :
  machine_state(ms) {
}

branch_predictor::~branch_predictor() {}

uint32_t branch_predictor::predict(uint64_t &idx) const {
  return 0;
}

void branch_predictor::update(uint32_t addr, uint64_t idx, bool taken) {}


gnoalias::gnoalias(sim_state &ms) :
  branch_predictor(ms) {}

gnoalias::~gnoalias() {}

uint32_t gnoalias::predict(uint64_t &idx) const {
  uint64_t addr = static_cast<uint64_t>(machine_state.fetch_pc>>2);
  uint64_t hbits = static_cast<uint64_t>(machine_state.bhr.to_integer()) << 32;
  idx = addr | hbits;
  const auto it = pht.find(idx);
  if(it == pht.cend()) {
    return 0;
  }
  return it->second;
}

void gnoalias::update(uint32_t addr, uint64_t idx, bool taken) {
  uint8_t &e = pht[idx];
  if(taken) {
    e = (e==3) ? 3 : (e + 1);
  }
  else {
    e = (e==0) ? 0 : e-1;
  }
}

bimode::bimode(sim_state &ms) :
  branch_predictor(ms),
  lg_d_pht_entries(sim_param::lg_pht_entries-1),
  lg_c_pht_entries(sim_param::lg_pht_entries-1) {
  c_pht = new twobit_counter_array(1UL << lg_c_pht_entries);
  t_pht = new twobit_counter_array(1UL << lg_d_pht_entries);
  nt_pht = new twobit_counter_array(1UL << lg_d_pht_entries);
}

bimode::~bimode() {
  delete c_pht;
  delete t_pht;
  delete nt_pht;
}

uint32_t bimode::predict(uint64_t &idx) const {
  uint32_t c_idx = (machine_state.fetch_pc>>2) &
    ((1U << lg_c_pht_entries) - 1);
  idx = ((machine_state.fetch_pc>>2) ^ machine_state.bhr.hash());
  idx &= (1UL << lg_d_pht_entries) - 1;
  if(c_pht->get_value(c_idx) < 2) {
    return nt_pht->get_value(idx);
  }
  else {
    return t_pht->get_value(idx);
  }
}

void bimode::update(uint32_t addr, uint64_t idx, bool taken) {
  uint32_t c_idx = (addr>>2) & ((1U << lg_c_pht_entries) - 1);
  if(c_pht->get_value(c_idx) < 2) {
    if(not(taken and (nt_pht->get_value(idx) > 1))) {
      c_pht->update(c_idx, taken);
    }
    nt_pht->update(idx, taken);
  }
  else {
    if(not(not(taken) and (t_pht->get_value(idx) < 2))) {
      c_pht->update(c_idx, taken);
    }
    t_pht->update(idx,taken);
  }
  

}
