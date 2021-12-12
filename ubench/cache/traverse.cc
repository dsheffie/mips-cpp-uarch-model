#include "mem_micro.hh"

template <bool xor_ptrs>
node *traverse(node *n, size_t iters) {
  while(iters) {
    n = xor_ptr<xor_ptrs>(n->next);
    n = xor_ptr<xor_ptrs>(n->next);
    n = xor_ptr<xor_ptrs>(n->next);
    n = xor_ptr<xor_ptrs>(n->next);
    iters -= UNROLL_AMT;
  }
  return n;
}


template node* traverse<true>(node*, size_t);
template node* traverse<false>(node*, size_t);
