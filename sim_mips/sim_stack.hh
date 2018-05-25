#include <cstdint>
#include <cstring>
#include "helper.hh"

#ifndef __sim_stack_hh__
#define __sim_stack_hh__

template <typename T>
class sim_stack_template {
protected:
  int64_t stack_sz;
  int64_t idx;
  T *stack;
public:
  sim_stack_template(int64_t stack_sz=8) : stack_sz(stack_sz), idx(stack_sz-1) {
    stack = new T[stack_sz];
    memset(stack, 0, sizeof(T)*stack_sz);
  }
  ~sim_stack_template() {
    delete [] stack;
  }
  void resize(int64_t stack_sz) {
    delete [] stack;
    this->stack_sz = stack_sz;
    idx = stack_sz-1;
    stack = new T[stack_sz];
    memset(stack, 0, sizeof(T)*stack_sz);
  }
  void clear() {
    memset(stack, 0, sizeof(T)*stack_sz);
    idx = stack_sz-1;
  }
  bool full() const {
    return idx==-1;
  }
  bool empty() const {
    return idx==(stack_sz-1);
  }
  int64_t size() const {
    return idx+1;
  }
  void push(const T &val) {
    assert(not(full()));
    stack[idx--]=val;
  }
  T pop() {
    assert(not(empty()));
    return stack[++idx];
  }
};

#endif
