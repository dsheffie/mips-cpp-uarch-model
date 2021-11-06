#include <stdio.h>
#include <stdint.h>

#define IDT_MONITOR_BASE 0xBFC00000
typedef uint32_t (*rdtsc_)(void);
typedef uint32_t (*icnt_)(void);

static rdtsc_ rdtsc = (rdtsc_*)(IDT_MONITOR_BASE + 8*50);
static icnt_ icnt = (icnt_*)(IDT_MONITOR_BASE + 8*53);

int goto_test4(int);
int goto_test8(int);
int goto_test16(int);
int goto_test32(int);
int goto_test64(int);
int goto_test128(int);
int goto_test256(int);
int goto_test512(int);
int goto_test1024(int);
int goto_test2048(int);
int goto_test4096(int);
int goto_test8192(int);
int goto_test16384(int);

typedef int (*test_t)(int);
static test_t funcs[] = {goto_test4,
			 goto_test8,
			 goto_test16,
			 goto_test32,
			 goto_test64,
			 goto_test128,
			 goto_test256,
			 goto_test512,
			 goto_test1024,
			 goto_test2048,
			 goto_test4096,
			 goto_test8192,
			 goto_test16384
};

int main() {
  const int n_tests = sizeof(funcs)/sizeof(funcs[0]);
  const int n_iters = 1024;
  for(int i = 0; i < n_tests; ++i) {
    uint32_t now = rdtsc();
    int sz = funcs[i](n_iters);
    now = rdtsc() - now;
    int jumps = n_iters * sz;
    //printf("now = %u, jumps = %d\n", now, n_iters*sz);
    
    double cycles_per_j =  ((double)now) / jumps;
    printf("sz = %d, cycles per jump = %g\n", sz, cycles_per_j);
  }
  return 0;
}
