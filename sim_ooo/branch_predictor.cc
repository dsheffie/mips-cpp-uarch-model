#include "branch_predictor.hh"
#include "mips_op.hh"
#include "globals.hh"
#include "sim_parameters.hh"

branch_predictor::branch_predictor(sim_state &ms) :
  machine_state(ms) {
}

branch_predictor::~branch_predictor() {}

uint32_t branch_predictor::predict(uint64_t &idx) const {
  return 0;
}

void branch_predictor::update(uint32_t addr, uint64_t idx, bool taken) {}

gshare::gshare(sim_state &ms) : branch_predictor(ms) {
  pht = new twobit_counter_array(1UL << sim_param::lg_pht_entries);
}

gshare::~gshare() {
  delete pht;
}

uint32_t gshare::predict(uint64_t &idx) const {
  idx = ((machine_state.fetch_pc>>2) ^ machine_state.bhr.to_integer());
  idx &= (1UL << sim_param::lg_pht_entries) - 1;
  return pht->get_value(idx);
}

void gshare::update(uint32_t addr, uint64_t idx, bool taken) {
  pht->update(idx, taken);
}


gselect::gselect(sim_state &ms) : branch_predictor(ms) {
  pht = new twobit_counter_array(1UL << sim_param::lg_pht_entries);
}

gselect::~gselect() {
  delete pht;
}

uint32_t gselect::predict(uint64_t &idx) const {
  uint64_t addr = static_cast<uint64_t>(machine_state.fetch_pc>>2);
  addr &= (1UL << sim_param::gselect_addr_bits) - 1;
  uint64_t hbits = static_cast<uint64_t>(machine_state.bhr.to_integer()) << sim_param::gselect_addr_bits;
  idx = addr | hbits;
  idx &= (1UL << sim_param::lg_pht_entries)-1;
  return pht->get_value(idx);
}

void gselect::update(uint32_t addr, uint64_t idx, bool taken) {
  pht->update(idx, taken);
}


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


