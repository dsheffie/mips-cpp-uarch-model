#ifndef __PROFILEMIPS__
#define __PROFILEMIPS__

#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <cassert>
#include "mips_encoding.hh"

/* from gdb simulator */
#define RSVD_INSTRUCTION           (0x00000005)
#define RSVD_INSTRUCTION_MASK      (0xFC00003F)
#define RSVD_INSTRUCTION_ARG_SHIFT 6
#define RSVD_INSTRUCTION_ARG_MASK  0xFFFFF  
#define IDT_MONITOR_BASE           0xBFC00000
#define IDT_MONITOR_SIZE           2048
#define MARGS 20

#define K1SIZE  (0x80000000)

typedef struct {
  uint32_t tv_sec;
  uint32_t tv_usec;
} timeval32_t;

typedef struct {
  uint32_t tms_utime;
  uint32_t tms_stime;
  uint32_t tms_cutime;
  uint32_t tms_cstime;
} tms32_t;

typedef struct {
  uint16_t st_dev;
  uint16_t st_ino;
  uint32_t st_mode;
  uint16_t st_nlink;
  uint16_t st_uid;
  uint16_t st_gid;
  uint16_t st_rdev;
  uint32_t st_size;
  uint32_t _st_atime;
  uint32_t st_spare1;
  uint32_t _st_mtime;
  uint32_t st_spare2;
  uint32_t _st_ctime;
  uint32_t st_spare3;
  uint32_t st_blksize;
  uint32_t st_blocks;
  uint32_t st_spare4[2];
} stat32_t;




class sparse_mem {
public:
  static const uint64_t pgsize = 4096;
private:
  size_t npages = 0;
  uint8_t **mem = nullptr;
  bool is_zero(uint64_t p) const {
    for(uint64_t i = 0; i < pgsize; i++) {
      if(mem[p][i])
	return false;
    }
    return true;
  }
public:
  sparse_mem(uint64_t nbytes) {
    npages = (nbytes+pgsize-1) / pgsize;
    mem = new uint8_t*[npages];
    memset(mem, 0, sizeof(uint8_t*)*npages);
  }
  sparse_mem(const sparse_mem &other) {
    npages = other.npages;
    for(size_t i = 0; i < npages; i++) {
      if(other.mem[i]) {
	mem[i] = new uint8_t[pgsize];
	memcpy(mem[i], other.mem[i], pgsize);
      }
    }
  }
  ~sparse_mem() {
    for(size_t i = 0; i < npages; i++) {
      if(mem[i]) {
	delete [] mem[i];
      }
    }
    delete [] mem;
  }
  uint8_t & at(uint64_t addr) {
    uint64_t paddr = addr / pgsize;
    uint64_t baddr = addr % pgsize;
    if(mem[paddr]==nullptr) {
      mem[paddr] = new uint8_t[pgsize];
      memset(mem[paddr],0,pgsize);
    }
    return mem[paddr][baddr];
  }
  uint8_t * operator[](uint64_t addr) {
    uint64_t paddr = addr / pgsize;
    uint64_t baddr = addr % pgsize;
    if(mem[paddr]==nullptr) {
      mem[paddr] = new uint8_t[pgsize];
      memset(mem[paddr],0,pgsize);
    }
    return &mem[paddr][baddr];
  }
  void prefault(uint64_t addr) {
    uint64_t paddr = addr / pgsize;
    if(mem[paddr]==nullptr) {
      mem[paddr] = new uint8_t[pgsize];
      memset(mem[paddr],0,pgsize);
    }
  }
  uint32_t& get32(uint64_t byte_addr) {
    uint64_t paddr = byte_addr / pgsize;
    uint64_t baddr = byte_addr % pgsize;
#if 0
    if(mem[paddr]==nullptr) {
      std::cerr << "ACCESS TO INVALID PAGE 0x"
	<< std::hex
	<< paddr*pgsize
	<< std::dec
	<< "\n";
	   exit(-1);
    }
#endif
    return *reinterpret_cast<uint32_t*>(mem[paddr]+baddr);
  }
  uint32_t get32(uint64_t byte_addr) const {
    uint64_t paddr = byte_addr / pgsize;
    uint64_t baddr = byte_addr % pgsize;
#if 0
    if(mem[paddr]==nullptr) {
      std::cerr << "ACCESS TO INVALID PAGE 0x"
	<< std::hex
	<< paddr*pgsize
	<< std::dec
	<< "\n";
	   exit(-1);
    }
#endif
    return *reinterpret_cast<uint32_t*>(mem[paddr]+baddr);
  }
  uint8_t * operator+(uint32_t disp) {
    return (*this)[disp];
  }
  uint64_t count() const {
    uint64_t c = 0;
    for(size_t i = 0; i < npages; i++) {
      c += (mem[i] != nullptr);
    }
    return c;
  }
};

template <typename T, size_t N>
struct sim_array {
  T arr[N];
  sim_array() {
    memset(arr, 0, sizeof(T)*N);
  }
};

struct state_t {
  sparse_mem &mem;
  uint32_t pc;
  int32_t lo;
  int32_t hi;
  uint64_t icnt;
  uint8_t brk;
  int32_t gpr[32];
  uint32_t cpr0[32];
  uint32_t cpr1[32];
  uint32_t fcr1[5];
  state_t(sparse_mem &mem) : mem(mem), pc(0),
			     lo(0), hi(0),
			     icnt(0), brk(0) {
    memset(gpr, 0, sizeof(int32_t)*32);
    memset(cpr0, 0, sizeof(uint32_t)*32);
    memset(cpr1, 0, sizeof(uint32_t)*32);
    memset(fcr1, 0, sizeof(uint32_t)*5);
  }
};

void initState(state_t *s);
void execMips(state_t *s);
void mkMonitorVectors(state_t *s);
#endif
