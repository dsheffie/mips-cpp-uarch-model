#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#include "funcs.h"

#define IDT_MONITOR_BASE 0xBFC00000

typedef uint32_t (*rdtsc_)(void);
typedef void (*nuke_caches_)(void);
typedef void (*flush_line_)(void*);

static rdtsc_ read_cycle_counter = (rdtsc_)(IDT_MONITOR_BASE + 8*50);
static nuke_caches_ nuke_caches = (nuke_caches_)(IDT_MONITOR_BASE + 8*51);
static flush_line_ flush_line = (flush_line_)(IDT_MONITOR_BASE + 8*52);


int main() {
  for(int i = NUM_FUNCS-1; i < NUM_FUNCS; i++) {
    uint32_t c = read_cycle_counter();
    __asm__("syscall");
    int x = funcs[i](1<<16);
    __asm__("syscall");
    c = read_cycle_counter() - c;
    double cc = ((double)c)/x;
    printf("%d,%g\n",i,cc);
  }

  return 0;
}
