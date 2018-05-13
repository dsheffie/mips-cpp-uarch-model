#ifndef __sim_bitvec_hh__
#define __sim_bitvec_hh__

#include <cstdlib>
#include <cstring>

class sim_bitvec {
private:
  static const size_t bpw = 8*sizeof(uint64_t);
  static const uint64_t all_ones = ~(0UL);
  uint64_t n_bits = 0, n_words = 0;
  uint64_t *arr = nullptr;
public:
  sim_bitvec(uint64_t n_bits = 64) : n_bits(n_bits), n_words((n_bits + bpw - 1) / bpw) {
    arr = new uint64_t[n_words];
    memset(arr, 0, sizeof(uint64_t)*n_words);
  }
  ~sim_bitvec() {
    delete [] arr;
  }
  void clear_and_resize(uint64_t n_bits) {
    delete [] arr;
    this->n_bits = n_bits;
    this->n_words = (n_bits + bpw - 1) / bpw;
    arr = new uint64_t[n_words];
    memset(arr, 0, sizeof(uint64_t)*n_words);
  }
  bool get_bit(size_t idx) const {
    uint64_t w_idx = idx / bpw;
    uint64_t b_idx = idx % bpw;
    return (arr[w_idx] >> b_idx)&0x1;
  }
  void set_bit(size_t idx) {
    assert(idx < n_bits);
    uint64_t w_idx = idx / bpw;
    uint64_t b_idx = idx % bpw;
    arr[w_idx] |= (1UL << b_idx);
  }
  void clear_bit(size_t idx) {
    assert(idx < n_bits);
    uint64_t w_idx = idx / bpw;
    uint64_t b_idx = idx % bpw;
    arr[w_idx] &= ~(1UL << b_idx);
  }
  uint64_t popcount() const {
    uint64_t c = 0;
    if((bpw*n_words) != n_bits) {
      dprintf(2, "implement clean-up\n", n_words);
      exit(-1);
    }
    else {
      for(uint64_t w = 0; w < n_words; w++) {
	c += __builtin_popcountll(arr[w]);
      }
    }
    return c;
  }
  int64_t find_first_unset() const {
    for(uint64_t w = 0; w < n_words; w++) {
      if(arr[w] == all_ones)
	continue;
      else if(arr[w]==0) {
	return bpw*w;
      }
      else {
	uint64_t x = ~arr[w];
	uint64_t idx = bpw*w + (__builtin_ffsl(x)-1);
	if(idx < n_bits)
	  return idx;
      }
    }
    return -1;    
  }
};

#endif
