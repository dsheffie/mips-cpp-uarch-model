#include <iostream>
#include <ostream>
#include <fstream>
#include <cstdlib>
#include <cstdio>
#include <cassert>
#include <cstring>
#include <map>
#include <sys/time.h>

#define IDT_MONITOR_BASE 0xBFC00000
typedef uint32_t (*rdtsc_)(void);
typedef void (*disasm_)(uint8_t*, uint32_t);
typedef void (*nuke_caches_)(void);
typedef uint32_t (*icnt_)(void);

static rdtsc_ rdtsc = reinterpret_cast<rdtsc_>(IDT_MONITOR_BASE + 8*50);
static disasm_ disasm = reinterpret_cast<disasm_>(IDT_MONITOR_BASE + 8*40);
static nuke_caches_ nuke_caches = reinterpret_cast<nuke_caches_>(IDT_MONITOR_BASE + 8*51);
static icnt_ icnt = reinterpret_cast<icnt_>(IDT_MONITOR_BASE + 8*53);

template<typename T>
void swap(T &x, T &y) {
  T t = x; x = y; y = t;
}

template <typename T>
void shuffle(T *arr, size_t len) {
  for(size_t i = 0; i < len; i++) {
    size_t j = i + (rand() % (len-i));
    swap(arr[i], arr[j]);
  }
}

static const int bufsz = 16384;
typedef int (*ubench_t)(void*,void*,int);

ubench_t make_code(uint8_t *buf, int32_t num_nops, int32_t unroll) {
  int offs = 0;
  union {
    int32_t i32;
    uint8_t bytes[4];
  } uu;
  union {
    int16_t i16;
    uint8_t bytes[2];
  } xx;
  memset(buf, 0, bufsz);
  //00c01021 	move	v0,a2
  buf[offs++] = 0x00;
  buf[offs++] = 0xc0;
  buf[offs++] = 0x10;
  buf[offs++] = 0x21;

  //24c6ffff 	addiu	a2,a2,-1
  buf[offs++] = 0x24;
  buf[offs++] = 0xc6;
  xx.i16 = -unroll;
  buf[offs++] = xx.bytes[0];
  buf[offs++] = xx.bytes[1];
   
  
  const int target = offs;
  //8c840000 	lw	a0,0(a0)
  buf[offs++] = 0x8c;
  buf[offs++] = 0x84;
  buf[offs++] = 0x00;
  buf[offs++] = 0x00;

  //addiu a3, a3, 01
#define inc_a3() {	\
    buf[offs++] = 0x24; \
    buf[offs++] = 0xe7;	\
    buf[offs++] = 0x00;	\
    buf[offs++] = 0x01;	\
  }
  
#define nop() {					\
    buf[offs++] = 0x00;				\
    buf[offs++] = 0x00;				\
    buf[offs++] = 0x00;				\
    buf[offs++] = 0x00;				\
  }
  
  for(int n = 0; n < num_nops; n++) {
    nop();
  }
  //  8ca50000 	lw	a1,0(a1)
  buf[offs++] = 0x8c; buf[offs++] = 0xa5;
  buf[offs++] = 0x00; buf[offs++] = 0x00;
  
  for(int n = 0; n < num_nops; n++) {
    nop();
  }
  
  
    
  //14c0fffe 	bnezl	a2,4 <bar+0x4>
  const int pc4 = offs+4;
  buf[offs++] = 0x54;
  buf[offs++] = 0xc0;
  uu.i32 = (target - pc4)/4;
  buf[offs++] = uu.bytes[2];
  buf[offs++] = uu.bytes[3];

  //24c6ffff 	addiu	a2,a2,-1
  buf[offs++] = 0x24;
  buf[offs++] = 0xc6;
  xx.i16 = -unroll;
  buf[offs++] = xx.bytes[0];
  buf[offs++] = xx.bytes[1];
   
  //8:	03e00008 	jr	ra
  buf[offs++] = 0x03;
  buf[offs++] = 0xe3;
  buf[offs++] = 0x00;
  buf[offs++] = 0x08;
  //nop
  buf[offs++] = 0x00;
  buf[offs++] = 0x00;
  buf[offs++] = 0x00;
  buf[offs++] = 0x00;

  assert(offs < bufsz);
  //disasm(buf, offs/4);
  return reinterpret_cast<ubench_t>(buf);
}

struct list {
  list *next = nullptr;
};
static list *nodes = nullptr;

std::map<int, uint8_t*> impls;



double avg_time(size_t len, int num_nops, uint32_t &t, int iterations=1024*1024) {
  ubench_t foo = reinterpret_cast<ubench_t>(impls.at(num_nops));
  uint32_t n_idx0 = rand() % len;
  uint32_t n_idx1 = rand() % len;
  t = rdtsc();
  int c = foo(&nodes[n_idx0],&nodes[n_idx1],iterations);
  t = rdtsc() - t;
  return static_cast<double>(t)/c;
}

int main() {
  int len = 1<<24;
  size_t *arr = nullptr;
  arr = (size_t*)malloc(sizeof(size_t)*len);
  nodes = (list*)malloc(sizeof(list)*len);
  
  for(size_t i = 0; i < len; i++) {
    arr[i] = i;
  }
  srand(9);
  shuffle(arr, len);

  for(size_t i = 0; i < (len-1); i++) {
    nodes[arr[i]].next = &nodes[arr[i+1]];
  }
  nodes[arr[len-1]].next = &nodes[arr[0]];
  free(arr);
  
  for(int nops = 1; nops <= 64; nops++) {
    uint8_t *buf = new uint8_t[bufsz];
    make_code(buf,nops,32);
    impls[nops] = buf;
  }
  asm("syscall");
  std::ofstream out("sim.txt");
  for(int nops = 2; nops <= 300; nops++) {
    uint32_t t;
    uint32_t ic = icnt();
    double a = avg_time(len,nops,t);
    ic = icnt() - ic;
    double ipc = static_cast<double>(ic) / t;
    std::cout << nops << " insns, "
	      << a << " avg cycles, "
	      << t << " total cycles, "
	      << ic << " total insns, "
	      << ipc << " insn per cycle"
	      << "\n";
    out << nops << "," << a << "\n";
  }
  out.close();
  
  for(auto &p : impls) {
    delete [] p.second;
  }


  free(nodes);
  return 0;
}
