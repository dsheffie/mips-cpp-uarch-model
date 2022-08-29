#include "branch_predictor.hh"
#include "machine_state.hh"
#include "globals.hh"
#include "sim_parameters.hh"

namespace {
  class tage_tbl {
  private:
    static const int tag_bits = 14;
    struct tage_tbl_entry {
      int64_t tag : tag_bits;
      int64_t pred : 3;
      int64_t useful : 2;
    };
    uint64_t lg_entries = 0;
    int tbl_id = -1;
    uint64_t h_bits = 0;
    tage_tbl_entry *tbl = nullptr;
  public:
    tage_tbl(uint64_t lg_entries, int tbl_id, uint64_t h_bits) :
      lg_entries(lg_entries), tbl_id(tbl_id), h_bits(h_bits) {
      tbl = new tage_tbl_entry[1UL<<lg_entries];
      memset(tbl,0,sizeof(tage_tbl_entry)*(1UL<<lg_entries));
    }
    ~tage_tbl() {
      delete [] tbl;
    }
  };
  
  class tage : public branch_predictor {
  private:
    twobit_counter_array *bimode_tbl = nullptr;
    tage_tbl** tagged_tbls = nullptr;
  public:
    tage(sim_state &ms) : branch_predictor(ms) {
      bimode_tbl = new twobit_counter_array(1UL<<sim_param::lg_tage_bimode_tbl_entries);
      tagged_tbls = new tage_tbl*[sim_param::num_tage_tbls];
      uint64_t tagged_len = 1UL<<sim_param::lg_tage_tagged_tbl_entries;
      for(int i = 0; i < sim_param::num_tage_tbls; i++) {
	/* power of 2 geometric series */
	tagged_tbls[i] = new tage_tbl(tagged_len, i, 1UL<<(i+1));
      }
    }
    ~tage() {
      for(int i = 0; i < sim_param::num_tage_tbls; i++) {
	delete [] tagged_tbls;
      }
      delete [] tagged_tbls;
      delete bimode_tbl;
    }
    
  };
  
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
      uint32_t addr = machine_state.fetch_pc;
      idx = ((addr>>2) ^ machine_state.bhr.hash());
      idx &= (1UL << sim_param::lg_pht_entries) - 1;
      //if(addr == 0x21e74) {
      //std::cout << "pred : "
      //<<std::hex << addr << std::dec
      //<< " " << machine_state.bhr
      //<< " maps to idx " << idx << "\n";
      //}
      
      return pht->get_value(idx);
    }
    void update(uint32_t addr, uint64_t idx, bool taken) override {
      // if(addr == 0x21e74) {
      // 	std::cout << "update : "
      // 		  <<std::hex << addr << std::dec
      // 		  << " maps to idx " << idx << "\n";
      // }
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
    gnoalias(sim_state &ms) : branch_predictor(ms) {}
    ~gnoalias() {}
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
