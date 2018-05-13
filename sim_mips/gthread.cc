#include <iostream>
#include <cassert>
#include "gthread.hh"

extern "C" {
  void start_gthread_asm(uint8_t*,void*,void*);
  void stop_gthread_asm();
  void switch_gthread_asm(uint64_t*,uint64_t*);
  void switch_and_start_gthread_asm(uint64_t*,uint8_t*,void*,void*);
};

std::shared_ptr<gthread> gthread::head = nullptr;
static std::shared_ptr<gthread> curr_thread = nullptr;

int64_t gthread::uuidcnt = 0;

void start_gthreads()  {
  assert(gthread::valid_head());
  curr_thread = gthread::head;
  curr_thread->status = gthread::thread_status::run;
  uint8_t *nstack = curr_thread->stack_ptr;
  //dprintf(2,"stack pointer %p\n", curr_thread->stack_ptr);
  gthread::callback_t fptr = curr_thread->fptr;
  void *arg = curr_thread->arg;
  start_gthread_asm(nstack,reinterpret_cast<void*>(fptr),arg);
}

void gthread_yield() {
  //save current state
  std::shared_ptr<gthread> curr = curr_thread;
  curr->status = gthread::thread_status::ready;
  std::shared_ptr<gthread> next = curr->get_next();
  assert(next);
  bool need_init = (next->status == gthread::thread_status::uninitialized);
  curr_thread = next;
  curr_thread->status = gthread::thread_status::run;
  if(need_init) {
    //dprintf(2,"stack pointer %p\n", next->stack_ptr);
    switch_and_start_gthread_asm(curr->state,
				 next->stack_ptr,
				 reinterpret_cast<void*>(next->fptr),
				 next->arg);
  }
  else {
    //dprintf(2, "curr %p, next %p\n", curr->get_id(), next->get_id());
    switch_gthread_asm(curr->state,next->state);
  }
}

void gthread_terminate() {
  std::shared_ptr<gthread> curr = curr_thread;
  std::shared_ptr<gthread> next = curr->get_next();
  curr->remove_from_list();
  if(gthread::head==nullptr) {
    stop_gthread_asm();
    return;
  }
  assert(next);
  bool need_init = (next->status == gthread::thread_status::uninitialized);
  curr_thread = next;
  curr_thread->status = gthread::thread_status::run;
  if(need_init) {
    switch_and_start_gthread_asm(curr->state,next->stack_ptr,
				 reinterpret_cast<void*>(next->fptr),
				 next->arg);
  }
  else {
    switch_gthread_asm(curr->state,next->state);
  }
}

