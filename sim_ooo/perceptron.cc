#include <cassert>
#include <cstring>

#include "perceptron.hh"

perceptron::perceptron(T threshold, int n_entries, int h_length) :
  threshold(threshold), n_entries(n_entries), h_length(h_length) {
  assert(isPow2(n_entries));
  table = new T*[n_entries];
  for(int i = 0; i < n_entries; i++) {
    table[i] = new T[h_length];
    memset(table[i], 0, sizeof(T)*h_length);
  }
}

perceptron::~perceptron() {
  for(int i = 0; i < n_entries; i++) {
    delete [] table[i];
  }
  delete [] table;
}


perceptron::T perceptron::predict(uint32_t pc, const sim_bitvec &history) const {
  T *row = table[ pc & (n_entries-1)];
  T sum = row[0];
  for(int i = 1; i < h_length; i++) {
    T x = history.get_bit(i) ? 1 : -1;
    sum += row[i] * x;
  }
  return sum;
}

void perceptron::update(T prediction, bool take_br, uint32_t pc, const sim_bitvec &history) {
  T t = take_br ? 1 : -1;
  T y = (y>threshold) ? 1 : ((y < (-threshold)) ? -1 : 0);
  if(t == y)
    return;

#if 0
  std::cout << "mispredict at PC "
	    << std::hex << pc << std::dec
	    << ", prediction " <<  prediction
	    << ", history "
	    << history
	    << "\n";
#endif
  
  T *row = table[ pc & (n_entries-1)];
  for(int i = 0; i < h_length; i++) {
    T x = history.get_bit(i) ? 1 : -1;
    row[i] += t*x;
  }

}
