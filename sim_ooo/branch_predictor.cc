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
  pht = new twobit_counter_array(sim_param::num_pht_entries);
}

gshare::~gshare() {
  delete pht;
}

uint32_t gshare::predict(uint64_t &idx) const {
  idx = ((machine_state.fetch_pc>>2) ^ machine_state.bhr.to_integer());
  idx &= (sim_param::num_pht_entries-1);
  return pht->get_value(idx);
}

void gshare::update(uint32_t addr, uint64_t idx, bool taken) {
  pht->update(idx, taken);
}


