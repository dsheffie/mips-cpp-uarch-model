#ifndef __perceptron_hh__
#define __perceptron_hh__

#include "sim_bitvec.hh"
#include "helper.hh"

class perceptron {
public:
  typedef int T;
private:
  T threshold;
  int n_entries = -1, h_length = -1;
  T **table = nullptr;
public:
  perceptron(T threshold, int n_entries, int h_length);
  ~perceptron();
  T predict(uint32_t pc, const sim_bitvec &history) const;
  void update(T prediction, bool take_br, uint32_t pc, const sim_bitvec &history) ;
  T get_threshold() const {
    return threshold;
  }
};

#endif
