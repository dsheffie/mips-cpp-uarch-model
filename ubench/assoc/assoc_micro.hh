#ifndef __mem_micro__
#define __mem_micro__

#include <cstdint>

template<int size>
struct node {
  node *next;
  uint8_t pad[size - sizeof(void*)];
};

template <int size>
volatile node<size> *traverse(volatile node<size> *n, uint64_t iters) {
  static_assert(sizeof(node<size>) == size, "size error");
  while(iters) {
    n = n->next;
    n = n->next;
    n = n->next;
    n = n->next;
    iters -= 4;
  }
  return n;
}

#endif
