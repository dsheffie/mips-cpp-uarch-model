#include "branch_predictor.hh"
#include "machine_state.hh"
#include "globals.hh"
#include "sim_parameters.hh"

// namespace {
//   class tage_tbl {
//   private:
//     static const int tag_bits = 14;
//     struct tage_tbl_entry {
//       int64_t tag : tag_bits;
//       int64_t pred : 3;
//       int64_t useful : 2;
//     };
//     uint64_t lg_entries = 0;
//     int tbl_id = -1;
//     uint64_t h_bits = 0;
//     tage_tbl_entry *tbl = nullptr;
//   public:
//     tage_tbl(uint64_t lg_entries, int tbl_id, uint64_t h_bits) :
//       lg_entries(lg_entries), tbl_id(tbl_id), h_bits(h_bits) {
//       tbl = new tage_tbl_entry[1UL<<lg_entries];
//       memset(tbl,0,sizeof(tage_tbl_entry)*(1UL<<lg_entries));
//     }
//     ~tage_tbl() {
//       delete [] tbl;
//     }
//   };
  
//   class tage : public branch_predictor {
//   private:
//     twobit_counter_array *bimode_tbl = nullptr;
//     tage_tbl** tagged_tbls = nullptr;
//   public:
//     tage(sim_state &ms) : branch_predictor(ms) {
//       bimode_tbl = new twobit_counter_array(1UL<<sim_param::lg_tage_bimode_tbl_entries);
//       tagged_tbls = new tage_tbl*[sim_param::num_tage_tbls];
//       uint64_t tagged_len = 1UL<<sim_param::lg_tage_tagged_tbl_entries;
//       for(int i = 0; i < sim_param::num_tage_tbls; i++) {
// 	/* power of 2 geometric series */
// 	tagged_tbls[i] = new tage_tbl(tagged_len, i, 1UL<<(i+1));
//       }
//     }
//     ~tage() {
//       for(int i = 0; i < sim_param::num_tage_tbls; i++) {
// 	delete [] tagged_tbls;
//       }
//       delete [] tagged_tbls;
//       delete bimode_tbl;
//     }
    
//   };
  
//   class gshare : public branch_predictor {
//   protected:
//     twobit_counter_array *pht;
//   public:
//     gshare(sim_state &ms) : branch_predictor(ms) {
//       pht = new twobit_counter_array(1UL << sim_param::lg_pht_entries);
//     }
//     ~gshare() {
//       delete pht;
//     }
//     uint32_t predict(uint64_t &idx) const override {
//       idx = ((machine_state.fetch_pc>>2) ^ machine_state.bhr.hash());
//       idx &= (1UL << sim_param::lg_pht_entries) - 1;
//       return pht->get_value(idx);
//     }
//     void update(uint32_t addr, uint64_t idx, bool taken) override {
//       pht->update(idx, taken);
//     }
//   };
  
//   class gselect : public gshare {
//   protected:
//     twobit_counter_array *pht;
//   public:
//     gselect(sim_state &ms) : gshare(ms) {}
//     ~gselect() {}
//     uint32_t predict(uint64_t &idx) const override {
//       uint64_t addr = static_cast<uint64_t>(machine_state.fetch_pc>>2);
//       addr &= (1UL << sim_param::gselect_addr_bits) - 1;
//       uint64_t hbits = static_cast<uint64_t>(machine_state.bhr.hash()) << sim_param::gselect_addr_bits;
//       idx = addr | hbits;
//       idx &= (1UL << sim_param::lg_pht_entries)-1;
//       return pht->get_value(idx);
//     }
//   };
  
//   class gnoalias : public branch_predictor {
//   protected:
//     std::map<uint64_t, uint8_t> pht;
//   public:
//     gnoalias(sim_state &ms) : branch_predictor(ms) {}
//     ~gnoalias() {}
//     uint32_t predict(uint64_t &idx) const override;
//     void update(uint32_t addr, uint64_t idx, bool taken) override;
//   };
  
//   class bimode : public branch_predictor {
//   protected:
//     const uint32_t lg_d_pht_entries;
//     const uint32_t lg_c_pht_entries;
//     twobit_counter_array *c_pht, *t_pht, *nt_pht;
    
//   public:
//     bimode(sim_state &ms);
//     ~bimode();
//     uint32_t predict(uint64_t &idx) const override;
//     void update(uint32_t addr, uint64_t idx, bool taken) override;
//   };
  

//   uint32_t gnoalias::predict(uint64_t &idx) const {
//     uint64_t addr = static_cast<uint64_t>(machine_state.fetch_pc>>2);
//     uint64_t hbits = static_cast<uint64_t>(machine_state.bhr.to_integer()) << 32;
//     idx = addr | hbits;
//     const auto it = pht.find(idx);
//     if(it == pht.cend()) {
//       return 0;
//     }
//     return it->second;
//   }
  
//   void gnoalias::update(uint32_t addr, uint64_t idx, bool taken) {
//     uint8_t &e = pht[idx];
//     if(taken) {
//       e = (e==3) ? 3 : (e + 1);
//     }
//     else {
//       e = (e==0) ? 0 : e-1;
//     }
//   }
  
//   bimode::bimode(sim_state &ms) :
//     branch_predictor(ms),
//     lg_d_pht_entries(sim_param::lg_pht_entries-1),
//     lg_c_pht_entries(sim_param::lg_pht_entries-1) {
//     c_pht = new twobit_counter_array(1UL << lg_c_pht_entries);
//     t_pht = new twobit_counter_array(1UL << lg_d_pht_entries);
//     nt_pht = new twobit_counter_array(1UL << lg_d_pht_entries);
//   }
  
//   bimode::~bimode() {
//     delete c_pht;
//     delete t_pht;
//     delete nt_pht;
//   }
  
//   uint32_t bimode::predict(uint64_t &idx) const {
//     uint32_t c_idx = (machine_state.fetch_pc>>2) &
//       ((1U << lg_c_pht_entries) - 1);
//     idx = ((machine_state.fetch_pc>>2) ^ machine_state.bhr.hash());
//     idx &= (1UL << lg_d_pht_entries) - 1;
//     if(c_pht->get_value(c_idx) < 2) {
//       return nt_pht->get_value(idx);
//     }
//     else {
//       return t_pht->get_value(idx);
//     }
//   }
  
//   void bimode::update(uint32_t addr, uint64_t idx, bool taken) {
//     uint32_t c_idx = (addr>>2) & ((1U << lg_c_pht_entries) - 1);
//     if(c_pht->get_value(c_idx) < 2) {
//       if(not(taken and (nt_pht->get_value(idx) > 1))) {
// 	c_pht->update(c_idx, taken);
//       }
//       nt_pht->update(idx, taken);
//     }
//     else {
//       if(not(not(taken) and (t_pht->get_value(idx) < 2))) {
// 	c_pht->update(c_idx, taken);
//       }
//       t_pht->update(idx,taken);
//     }
//   }
// };



/*

This code implements a hashed perceptron branch predictor using geometric
history lengths and dynamic threshold setting.  It was written by Daniel
A. Jiménez in March 2019.


The original perceptron branch predictor is from Jiménez and Lin, "Dynamic
Branch Prediction with Perceptrons," HPCA 2001.

The idea of using multiple independently indexed tables of perceptron weights
is from Jiménez, "Fast Path-Based Neural Branch Prediction," MICRO 2003 and
later expanded in "Piecewise Linear Branch Prediction" from ISCA 2005.

The idea of using hashes of branch history to reduce the number of independent
tables is documented in three contemporaneous papers:

1. Seznec, "Revisiting the Perceptron Predictor," IRISA technical report, 2004.

2. Tarjan and Skadron, "Revisiting the Perceptron Predictor Again," UVA
technical report, 2004, expanded and published in ACM TACO 2005 as "Merging
path and gshare indexing in perceptron branch prediction"; introduces the term
"hashed perceptron."

3. Loh and Jiménez, "Reducing the Power and Complexity of Path-Based Neural
Branch Prediction," WCED 2005.

The ideas of using "geometric history lengths" i.e. hashing into tables with
histories of exponentially increasing length, as well as dynamically adjusting
the theta parameter, are from Seznec, "The O-GEHL Branch Predictor," from CBP
2004, expanded later as "Analysis of the O-GEometric History Length Branch
Predictor" in ISCA 2005.

This code uses these ideas, but prefers simplicity over absolute accuracy (I
wrote it in about an hour and later spent more time on this comment block than
I did on the code). These papers and subsequent papers by Jiménez and other
authors significantly improve the accuracy of perceptron-based predictors but
involve tricks and analysis beyond the needs of a tool like ChampSim that
targets cache optimizations. If you want accuracy at any cost, see the winners
of the latest branch prediction contest, CBP 2016 as of this writing, but
prepare to have your face melted off by the complexity of the code you find
there. If you are a student being asked to code a good branch predictor for
your computer architecture class, don't copy this code; there are much better
sources for you to plagiarize.
*/


// speed for dynamic threshold setting
#define SPEED 18

// 12-bit indices for the tables
#define LOG_TABLE_SIZE 12
#define TABLE_SIZE (1 << LOG_TABLE_SIZE)

// this many 12-bit words will be kept in the global history

#define MAXHIST 232

#define NGHIST_WORDS (MAXHIST / LOG_TABLE_SIZE + 1)

static int history_lengths[] = {0, 3, 4, 6, 8, 10, 14, 19, 26, 36, 49, 67, 91, 125, 170,  MAXHIST};

#define NTABLES ((sizeof(history_lengths)/sizeof(history_lengths[0])))

// tables of 8-bit weights
static int tables[NTABLES][TABLE_SIZE] = {0};
// words that store the global history
static unsigned int ghist_words[NGHIST_WORDS] = {0};
// remember the indices into the tables from prediction to update
static uint64_t indices[branch_predictor::fetch_state_sz][NTABLES] = {0};

// initialize theta to something reasonable,
static int theta = {10};

    // initialize counter for threshold setting algorithm
static int tc = {0};

static uint32_t pcs[branch_predictor::fetch_state_sz] = {~0U};

    // perceptron sum
static int yout[branch_predictor::fetch_state_sz] = {0};

static int hashed_perceptron_predict(uint32_t pc, uint32_t key) {
  // initialize perceptron sum
  ::yout[key] = 0;
  pcs[key] = pc;
  
  // for each table...
  for (int i = 0; i < NTABLES; i++) {
    // n is the history length for this table
    int n = history_lengths[i];
    // hash global history bits 0..n-1 into x by XORing the words from the
    // ghist_words array

    uint64_t x = 0;

    // most of the words are 12 bits long
    
    int most_words = n / LOG_TABLE_SIZE;
    
    // the last word is fewer than 12 bits
    
      int last_word = n % LOG_TABLE_SIZE;

      // XOR up to the next-to-the-last word

      int j;
      for (j = 0; j < most_words; j++)
	x ^= ::ghist_words[j];

      // XOR in the last word

      x ^= ::ghist_words[j] & ((1 << last_word) - 1);

      // XOR in the PC to spread accesses around (like gshare)

      x ^= pc;

      // stay within the table size

      x &= TABLE_SIZE - 1;

      // remember this index for update

      indices[key][i] = x;

      // add the selected weight to the perceptron sum

      yout[key] += ::tables[i][x];
    }
    return yout[key] >= 1;
  }

static void hashed_perceptron_update(uint32_t key, uint32_t pc, uint32_t target, int taken) {
    // was this prediction correct?
    bool correct = taken == (::yout[key] >= 1);


    if(pcs[key] != pc) {
      std::cout << "perceptron update with key " << key << ", pc mismatch\n";
      std::cout << "retirement pc " << std::hex << pc << std::dec << "\n";
      std::cout << "table pc      " << std::hex << pcs[key] << std::dec << "\n"; 
    }
    // insert this branch outcome into the global history

    bool b = taken;
    for (int i = 0; i < NGHIST_WORDS; i++) {

      // shift b into the lsb of the current word

      ::ghist_words[i] <<= 1;
      ::ghist_words[i] |= b;

      // get b as the previous msb of the current word

      b = !!(::ghist_words[i] & TABLE_SIZE);
      ::ghist_words[i] &= TABLE_SIZE - 1;
    }

    // get the magnitude of yout

    int a = (::yout[key] < 0) ? -::yout[key] : ::yout[key];

    // perceptron learning rule: train if misprediction or weak correct prediction

    if (!correct || a < ::theta) {
      // update weights
      for (int i = 0; i < NTABLES; i++) {
	// which weight did we use to compute yout?

	int* c = &::tables[i][::indices[key][i]];

	// increment if taken, decrement if not, saturating at 127/-128

	if (taken) {
	  if (*c < 127)
	    (*c)++;
	} else {
	  if (*c > -128)
	    (*c)--;
	}
      }

      // dynamic threshold setting from Seznec's O-GEHL paper

      if (!correct) {

	// increase theta after enough mispredictions

	::tc++;
	if (::tc >= SPEED) {
	  ::theta++;
	  ::tc = 0;
	}
      } else if (a < ::theta) {

	// decrease theta after enough weak but correct predictions

	::tc--;
	if (::tc <= -SPEED) {
	  ::theta--;
	  ::tc = 0;
	}
      }
    }
}


class hashed_perceptron : public branch_predictor {
public:
  hashed_perceptron(sim_state &ms) : branch_predictor(ms) {}
  
  uint32_t predict(uint32_t pc, uint32_t fetch_token) override {
    uint32_t t = hashed_perceptron_predict(pc, fetch_token);
    //printf("make prediction (%u) for pc %x, fetch token %u\n", t, pc, fetch_token);    
    return t;
  }
  void update(uint32_t pc, uint32_t fetch_token, uint32_t target, bool taken) override {
    //printf("update prediction for pc %x, fetch token %u, t = %u\n", pc, fetch_token, taken);
    hashed_perceptron_update(fetch_token, pc, target, taken);
  }
};
  


branch_predictor* branch_predictor::get_predictor(int id, sim_state &ms) {
  return new hashed_perceptron(ms);
}
