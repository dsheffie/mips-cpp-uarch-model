#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <iostream>
#include <chrono>
#include <cstring>
#include <fstream>

#include "mem_micro.hh"

#define IDT_MONITOR_BASE 0xBFC00000
typedef uint32_t (*rdtsc_)(void);
static rdtsc_ rdtsc = reinterpret_cast<rdtsc_>(IDT_MONITOR_BASE + 8*50);

template <typename T>
void swap(T &x, T &y) {
  T t = x;
  x = y; y = t;
}

template <typename T>
void shuffle(std::vector<T> &vec, size_t len) {
  for(size_t i = 0; i < len; i++) {
    size_t j = i + (rand() % (len - i));
    swap(vec[i], vec[j]);
  }
}
  
int main(int argc, char *argv[]) {
  static const uint64_t max_keys = 1UL<<23;
  static const bool xor_pointers = false;
  
  node *nodes = nullptr;

  nodes = new node[max_keys];
  
  std::ofstream out("cpu.csv");
  std::vector<uint64_t> keys(max_keys);
  
  for(uint64_t n_keys = 1UL<<8; n_keys <= max_keys; n_keys *= 2) {
    
    for(uint64_t i = 0; i < n_keys; i++) {
      keys[i] = i;
    }
    
    shuffle(keys, n_keys);
    node *h = &nodes[keys[0]];
    node *c = h;  
    h->next = h;
    for(uint64_t i = 1; i < n_keys; i++) {
      node *n = &nodes[keys[i]];
      node *t = c->next;
      c->next = n;
      n->next = t;
      c = n;
    }
    
    if(xor_pointers) {
      for(uint64_t i = 0; i < n_keys; i++) {
	nodes[i].next = xor_ptr<true>(nodes[i].next);
      }
    }
    
    size_t iters = n_keys*16;
    if(iters < (1UL<<20)) {
      iters = (1UL<<20);
    }
    auto c_start = rdtsc();
    traverse<false>(h, iters);
    auto c_stop = rdtsc();
    double c_t = static_cast<double>(c_stop-c_start);        
    c_t /= iters;
    std::cout << (n_keys*sizeof(node)) << "," << c_t <<" cycles\n";
    out << (n_keys*sizeof(node)) << "," << c_t << "\n";
    out.flush();
  }
  out.close();
  delete [] nodes;
  return 0;
}
