#ifndef __gthreadhh__
#define __gthreadhh__

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>

void start_gthreads();
void gthread_yield();
void gthread_terminate();

class gthread : public std::enable_shared_from_this<gthread> {
private:
  typedef void (*callback_t)(void*);
  static const size_t stack_sz = 1<<20;
  enum class thread_status {uninitialized,ready,run};
  static std::shared_ptr<gthread> head;
  static int64_t uuidcnt;
  int64_t id = -1;
  callback_t fptr = nullptr;
  void *arg = nullptr;
  uint8_t *stack_ptr = nullptr;
  thread_status status = thread_status::uninitialized;
  std::shared_ptr<gthread> next = nullptr;
  std::shared_ptr<gthread> prev = nullptr;
  uint64_t state[7] = {0};
  uint8_t stack_alloc[stack_sz] = {0};
  void remove_from_list() {
    if(next) {
      next->prev = prev;
    }
    if(prev) {
      prev->next = next;
    }
    if(head == shared_from_this()) {
      head = next;
    }
  }
  static bool valid_head() {
    return (head != nullptr);
  }
  void insert_into_list() {
    if(head==nullptr) {
      head = shared_from_this();
    }
    else {
      std::shared_ptr<gthread> ptr = head;
      while(ptr->next != nullptr) {
	ptr = ptr->next;
      }
      ptr->next = shared_from_this();
      prev = ptr;
    }
  }
  std::shared_ptr<gthread> get_next() const {
    if(next==nullptr)
      return head;
    else
      return next;
  }
  gthread(callback_t fptr, void *arg) : id(uuidcnt++), fptr(fptr),
					arg(arg), stack_ptr(stack_alloc + stack_sz - 8) {}
public:
  static void make_gthread(callback_t fptr, void *arg) {
    /* delegate ctor to helper class */
    class gthread_ctor : public gthread {
    public:
      gthread_ctor(callback_t fptr, void *arg) : gthread(fptr, arg) {}
    };
    auto t = std::make_shared<gthread_ctor>(fptr, arg);
    t->insert_into_list();
  }
  friend void start_gthreads();
  friend void gthread_yield();
  friend void gthread_terminate();
};





#endif
