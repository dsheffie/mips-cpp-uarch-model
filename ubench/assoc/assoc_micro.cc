#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <iostream>
#include <chrono>
#include <cstring>
#include <fstream>

#include "assoc_micro.hh"


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

#define IDT_MONITOR_BASE 0xBFC00000
typedef uint32_t (*rdtsc_)(void);
static rdtsc_ rdtsc = reinterpret_cast<rdtsc_>(IDT_MONITOR_BASE + 8*50);


template <int size>
void run(uint8_t *ptr, size_t n_keys, std::ofstream &out) {
  std::vector<uint64_t> keys(n_keys);
  node<size> *nodes = reinterpret_cast<node<size>*>(ptr);
  for(uint64_t i = 0; i < n_keys; i++) {
    keys[i] = i;
  }
  shuffle(keys, n_keys);
  node<size> *h = &nodes[keys[0]];
  node<size> *c = h;  
  h->next = h;
  for(uint64_t i = 1; i < n_keys; i++) {
    node<size> *n = &nodes[keys[i]];
    node<size> *t = c->next;
    c->next = n;
    n->next = t;
    c = n;
  }
  size_t iters = (1UL<<20);
  auto c_start = rdtsc();
  traverse<size>(h, iters);
  auto c_stop = rdtsc();
  double c_t = static_cast<double>(c_stop-c_start);        
  c_t /= iters;
  std::cout << n_keys << "," << c_t <<" cycles\n";
  out << n_keys << "," << c_t <<"\n";
  out.flush();  
}


int main(int argc, char *argv[]) {
  uint8_t *ptr = nullptr;
  const size_t max_mem = 1UL<<26;
  ptr = new uint8_t[max_mem];

  std::ofstream out("cpu.csv");

  for(int i = 1; i < 32; i++) {
    run<(1<<20)>(ptr, i, out);
  }
  
  
  out.close();
  delete [] ptr;
  return 0;
}
