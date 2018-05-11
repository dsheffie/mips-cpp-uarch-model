#ifndef __mips_encoding_hh__
#define __mips_encoding_hh__

struct rtype_t {
  uint32_t opcode : 6;
  uint32_t sa : 5;
  uint32_t rd : 5;
  uint32_t rt : 5;
  uint32_t rs : 5;
  uint32_t special : 6;
};

struct itype_t {
  uint32_t imm : 16;
  uint32_t rt : 5;
  uint32_t rs : 5;
  uint32_t opcode : 6;
};

struct coproc1x_t {
  uint32_t fmt : 3;
  uint32_t id : 3;
  uint32_t fd : 5;
  uint32_t fs : 5;
  uint32_t ft : 5;
  uint32_t fr : 5;
  uint32_t opcode : 6;
};

struct lwxc1_t {
  uint32_t id : 6;
  uint32_t fd : 5;
  uint32_t pad : 5;
  uint32_t index : 5;
  uint32_t base : 5;
  uint32_t opcode : 6;
};

union mips_t {
  rtype_t r;
  itype_t i;
  coproc1x_t c1x;
  lwxc1_t lc1x;
  uint32_t raw;
  mips_t(uint32_t x) : raw(x) {}
};

#endif
