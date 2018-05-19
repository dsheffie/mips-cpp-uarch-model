#ifndef __gthreadhh__
#define __gthreadhh__

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>

void start_gthreads();
void gthread_yield();
void gthread_terminate();

class alignas(64) gthread : public std::enable_shared_from_this<gthread> {
 public:
  typedef std::shared_ptr<gthread> gthread_ptr;
private:
  typedef void (*callback_t)(void*);
  static const size_t stack_sz = 1<<24;
  enum class thread_status {uninitialized,ready,run};
  static gthread_ptr head;
  static int64_t uuidcnt;
  int64_t id = -1;
  callback_t fptr = nullptr;
  void *arg = nullptr;
  uint8_t *stack_ptr = nullptr;
  thread_status status = thread_status::uninitialized;
  gthread_ptr next = nullptr;
  gthread_ptr prev = nullptr;
  uint64_t state[7] = {0};
  uint8_t stack_alloc[stack_sz] __attribute__((aligned(64))) = {0};
  int64_t get_id() const {
    return id;
  }
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
      gthread_ptr ptr = head;
      while(ptr->next != nullptr) {
	ptr = ptr->next;
      }
      ptr->next = shared_from_this();
      prev = ptr;
    }
  }
  gthread_ptr get_next() const {
    if(next==nullptr)
      return head;
    else
      return next;
  }
  gthread(callback_t fptr, void *arg) : id(uuidcnt++), fptr(fptr),
					arg(arg), stack_ptr(stack_alloc + stack_sz - 16) {}
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
