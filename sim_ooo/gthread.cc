#include <iostream>
#include <cassert>
#include "gthread.hh"

extern "C" {
  uint64_t saved_sp;
  void start_gthread_asm(uint8_t*,void*,void*);
  void stop_gthread_asm();
  void switch_gthread_asm(uint64_t*,uint64_t*);
  void switch_and_start_gthread_asm(uint64_t*,uint8_t*,void*,void*);
};

gthread::gthread_ptr gthread::head = nullptr;
static gthread::gthread_ptr curr_thread = nullptr;

std::list<gthread::gthread_ptr> gthread::threads;

int64_t gthread::uuidcnt = 0;

void start_gthreads()  {
  assert(gthread::valid_head());
  curr_thread = gthread::head;
  curr_thread->status = gthread::thread_status::run;
  start_gthread_asm(curr_thread->stack_ptr,reinterpret_cast<void*>(curr_thread->fptr),curr_thread->arg);
}

void gthread_yield() {
  //save current state
  gthread::gthread_ptr curr = curr_thread;
  curr->status = gthread::thread_status::ready;
  gthread::gthread_ptr next = curr->get_next();
  assert(next);
  bool need_init = (next->status == gthread::thread_status::uninitialized);
  curr_thread = next;
  curr_thread->status = gthread::thread_status::run;
  if(need_init) {
    switch_and_start_gthread_asm(curr->state,
				 next->stack_ptr,
				 reinterpret_cast<void*>(next->fptr),
				 next->arg);
  }
  else {
    switch_gthread_asm(curr->state,next->state);
  }
}

void gthread_terminate() {
  gthread::gthread_ptr curr = curr_thread;
  gthread::gthread_ptr next = curr->get_next();
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

