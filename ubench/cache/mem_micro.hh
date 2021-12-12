#ifndef __mem_micro__
#define __mem_micro__

#include <cstdint>
#include <cstdlib>
#include <type_traits>

static const size_t UNROLL_AMT = 4;


#define BS_PRED(SZ) (std::is_integral<T>::value && (sizeof(T)==SZ))

template <typename T, typename std::enable_if<BS_PRED(8),T>::type* = nullptr>
constexpr uint64_t ptr_key() {return 0x1234567076543210UL; }


template <typename T, typename std::enable_if<BS_PRED(4),T>::type* = nullptr>
constexpr uint32_t ptr_key() {return 0x12345670U; }

#undef BS_PRED


struct node {
  node *next;
};

static const void* failed_mmap = reinterpret_cast<void *>(-1);

template <bool enable>
static inline node* xor_ptr(node *ptr) {
  if(enable) {
    size_t p = reinterpret_cast<size_t>(ptr);
    p ^= ptr_key<size_t>();
    return reinterpret_cast<node*>(p);
  }
  else {
    return ptr;
  }
}

template <bool xor_ptrs> node *traverse(node *n, size_t iters);

#endif
