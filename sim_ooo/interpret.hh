#ifndef __PROFILEMIPS__
#define __PROFILEMIPS__

#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <cassert>

#include "state.hh"

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



void initState(state_t *s);
void execMips(state_t *s);
void mkMonitorVectors(state_t *s);

#endif
