#ifndef __INTERPRET_HH__
#define __INTERPRET_HH__

#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <ostream>
#include <map>
#include <string>

#include "riscv.hh"

#define MARGS 20

/* stolen from libgloss-htif : syscall.h */
#define SYS_getcwd 17
#define SYS_fcntl 25
#define SYS_mkdirat 34
#define SYS_unlinkat 35
#define SYS_linkat 37
#define SYS_renameat 38
#define SYS_ftruncate 46
#define SYS_faccessat 48
#define SYS_chdir 49
#define SYS_open   55
#define SYS_openat 56
#define SYS_close 57
#define SYS_lseek 62
#define SYS_read 63
#define SYS_write 64
#define SYS_pread 67
#define SYS_pwrite 68
#define SYS_stat 78
#define SYS_fstatat 79
#define SYS_fstat 80
#define SYS_exit 93
#define SYS_gettimeofday 94
#define SYS_times 95
#define SYS_lstat 1039
#define SYS_getmainvars 2011


struct state_t{
  uint64_t pc;
  uint64_t last_pc;
  uint64_t last_call;
  int64_t gpr[32];
  uint8_t *mem;
  uint8_t brk;
  uint8_t bad_addr;
  uint64_t epc;
  uint64_t maxicnt;
  uint64_t icnt;

  bool took_exception;
  riscv::riscv_priv priv;
  
  /* lots of CSRs */
  int64_t mstatus;
  int64_t misa;
  int64_t mideleg;
  int64_t medeleg;
  int64_t mscratch;
  int64_t mhartid;
  int64_t mtvec;
  int64_t mcounteren;
  int64_t mie;
  int64_t mip;
  int64_t mcause;
  int64_t mepc;
  int64_t mtval;
  int64_t sscratch;
  int64_t scause;
  int64_t stvec;
  int64_t sepc;
  int64_t sie;
  int64_t sip;
  int64_t stval;
  int64_t satp;
  int64_t scounteren;
  int64_t pmpaddr0;
  int64_t pmpaddr1;
  int64_t pmpaddr2;
  int64_t pmpaddr3;
  int64_t pmpcfg0;
  int64_t mtimecmp;  
  int xlen() const {
    return 64;
  }
  int64_t get_time() const;
  void sext_xlen(int64_t x, int i) {
    gpr[i] = (x << (64-xlen())) >> (64-xlen());
  }
  uint32_t get_reg_u32(int id) const {
    return *reinterpret_cast<const uint32_t*>(&gpr[id]);
  }
  int32_t get_reg_i32(int id) const {
    return *reinterpret_cast<const int32_t*>(&gpr[id]);
  }
  uint64_t get_reg_u64(int id) const {
    return *reinterpret_cast<const uint64_t*>(&gpr[id]);
  }
  bool unpaged_mode() const {
    if((priv == riscv::priv_machine) or (priv == riscv::priv_hypervisor)) {
      return true;
    }
    if( ((satp >> 60)&0xf) == 0) {
      return true;
    }
    return false;
  }
  bool memory_map_check(uint64_t pa, bool store = false, int64_t x = 0);  
  int8_t load8(uint64_t pa);
  int64_t load8u(uint64_t pa);  
  int16_t load16(uint64_t pa);
  int64_t load16u(uint64_t pa);   
  int32_t load32(uint64_t pa);
  int64_t load32u(uint64_t pa);  
  int64_t load64(uint64_t pa);
  void store8(uint64_t pa,  int8_t x);
  void store16(uint64_t pa, int16_t x); 
  void store32(uint64_t pa, int32_t x);
  void store64(uint64_t pa, int64_t x);

  
  uint64_t translate(uint64_t ea, int &fault, int sz,
		     bool store = false, bool fetch = false) const;

   
};

std::ostream &operator<<(std::ostream &out, const state_t & s);

namespace riscv {
  void initState(state_t *s);
  void runRiscv(state_t *s, uint64_t dumpIcnt);
  void handle_syscall(state_t *s, uint64_t tohost);

  struct stat32_t {
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
  };
};

void execRiscv(state_t *s);

#endif
