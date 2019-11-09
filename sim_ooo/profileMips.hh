#ifndef __PROFILEMIPS__
#define __PROFILEMIPS__

#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <cassert>
#include "mips_encoding.hh"
#include "sparse_mem.hh"

const static int MARGS = 20;
static const int HWINDOW = (1<<16);

class simCache;

struct timeval32_t {
  uint32_t tv_sec;
  uint32_t tv_usec;
};

struct tms32_t {
  uint32_t tms_utime;
  uint32_t tms_stime;
  uint32_t tms_cutime;
  uint32_t tms_cstime;
};

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


struct history_t {
  uint32_t fetch_pc = 0;
  uint32_t next_pc = 0;
  bool was_branch_or_jump = false;
  bool was_likely_branch = false;
  bool took_branch_or_jump = false;
  uint64_t icnt;
};

struct state_t {
  sparse_mem &mem;
  uint32_t pc;
  int32_t lo;
  int32_t hi;
  uint64_t icnt;
  uint8_t brk;
  uint8_t syscall;
  int call_site;
  int32_t gpr[32];
  uint32_t cpr0[32];
  uint32_t cpr1[32];
  uint32_t fcr1[5];
  int num_open_fd = 0;
  bool silent = false;
  simCache *l1d = nullptr;
  history_t hbuf[HWINDOW];
  state_t(sparse_mem &mem) : mem(mem), pc(0), lo(0), hi(0),
			     icnt(0), brk(0),
			     syscall(0) {
    memset(gpr, 0, sizeof(int32_t)*32);
    memset(cpr0, 0, sizeof(uint32_t)*32);
    memset(cpr1, 0, sizeof(uint32_t)*32);
    memset(fcr1, 0, sizeof(uint32_t)*5);
    memset(hbuf, 0, sizeof(hbuf));

  }
  void copy(const state_t *other) {
    pc = other->pc;
    lo = other->lo;
    hi = other->hi;
    icnt = other->icnt;
    brk = other->brk;
    syscall = other->syscall;
    call_site = other->call_site;
    memcpy(gpr, other->gpr, sizeof(int32_t)*32);
    memcpy(cpr0, other->cpr0, sizeof(uint32_t)*32);
    memcpy(cpr1, other->cpr1, sizeof(uint32_t)*32);
    memcpy(fcr1, other->fcr1, sizeof(uint32_t)*5);
  }
};

void initState(state_t *s);
void execMips(state_t *s);
void mkMonitorVectors(state_t *s);

#endif
