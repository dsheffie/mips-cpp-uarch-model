#ifndef __mipsop_hh__
#define __mipsop_hh__

#include <memory>

struct mips_op;

struct mips_meta_op : std::enable_shared_from_this<mips_meta_op> {
  uint32_t pc = 0;
  uint32_t inst = 0;
  uint32_t fetch_npc = 0;  
  uint64_t fetch_cycle = 0;
  uint64_t decode_cycle = 0;
  uint64_t alloc_cycle = 0;
  uint64_t complete_cycle = 0;

  /* from decode */
  mips_op_type op_class = mips_op_type::unknown;
  
  /* finished execution */
  bool complete = false;
  int32_t rob_idx = -1;
  /* result will get written to prf idx */
  int32_t prf_idx = -1;
  /* previous prf writer (will return to freelist when this inst retires */
  int32_t prev_prf_idx = -1;

  std::shared_ptr<mips_op> op = nullptr;
  
  mips_meta_op(uint32_t pc, uint32_t inst,  uint32_t fetch_npc, uint32_t fetch_cycle) :
    pc(pc), inst(inst), fetch_npc(fetch_npc), fetch_cycle(fetch_cycle) {}

  ~mips_meta_op() {}
  std::shared_ptr<mips_meta_op> getptr() {
    return shared_from_this();
  }
};


struct mips_op : std::enable_shared_from_this<mips_op>{
  std::shared_ptr<mips_meta_op> m = nullptr;
  mips_op(std::shared_ptr<mips_meta_op> m) : m(m) {}
  virtual ~mips_op() {
    m = nullptr;
  }
};



#endif
