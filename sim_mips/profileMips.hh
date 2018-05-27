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

#define MARGS 20


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




struct state_t {
  sparse_mem &mem;
  uint32_t pc;
  int32_t lo;
  int32_t hi;
  uint64_t icnt;
  uint8_t brk;
  int call_site;
  int32_t gpr[32];
  uint32_t cpr0[32];
  uint32_t cpr1[32];
  uint32_t fcr1[5];

  int num_open_fd = 0;
  int debug;
  state_t(sparse_mem &mem) : mem(mem), pc(0), lo(0), hi(0),
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
