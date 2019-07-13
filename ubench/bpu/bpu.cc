#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <cassert>
#include <iostream>
#include <unistd.h>

#define IDT_MONITOR_BASE 0xBFC00000

typedef uint32_t (*rdtsc_)(void);
typedef void (*nuke_caches_)(void);
typedef void (*flush_line_)(void*);

static rdtsc_ read_cycle_counter = (rdtsc_)(IDT_MONITOR_BASE + 8*50);
static nuke_caches_ nuke_caches = (nuke_caches_)(IDT_MONITOR_BASE + 8*51);
static flush_line_ flush_line = (flush_line_)(IDT_MONITOR_BASE + 8*52);

extern "C" {
  uint32_t func(uint32_t x, uint32_t len, uint32_t iters);
};

uint32_t xorshift32(uint32_t x, uint32_t t) {
  for(int i = 0; i <= t; i++) {
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
  }
  return x;
}


int main() {
  uint32_t z = func(1, 24, 1<<28);
  std::cout << "asm x = " << z << "\n";
  return 0;
}
