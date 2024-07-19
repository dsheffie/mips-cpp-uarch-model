#ifndef __riscvhh__
#define __riscvhh__

#include <ostream>

namespace riscv {
  enum riscv_priv {
    priv_user = 0,
    priv_supervisor,
    priv_hypervisor,
    priv_machine,
  };

  struct mip_t {
    uint64_t z0 : 1;
    uint64_t ssip : 1;
    uint64_t z2 : 1;
    uint64_t msip : 1;
    uint64_t z4 : 1;
    uint64_t stip : 1;
    uint64_t z6 : 1;
    uint64_t mtip : 1;
    uint64_t z8 : 1;
    uint64_t seip : 1;
    uint64_t z10 : 1;
    uint64_t meip : 1;
    uint64_t z : 52;
  };

  struct mie_t {
    uint64_t z0 : 1;
    uint64_t ssie : 1;
    uint64_t z2 : 1;
    uint64_t msie : 1;
    uint64_t z4 : 1;
    uint64_t stie : 1;
    uint64_t z6 : 1;
    uint64_t mtie : 1;
    uint64_t z8 : 1;
    uint64_t seie : 1;
    uint64_t z10 : 1;
    uint64_t meie : 1;
    uint64_t z : 52;
  };

  struct satp_t {
    uint64_t ppn : 44;
    uint64_t asid : 16;
    uint64_t mode : 4;
  };


  struct store_rec {
    uint64_t pc;
    uint64_t addr;
    uint64_t data;
    store_rec(uint64_t pc, uint64_t addr, uint64_t data) :
      pc(pc), addr(addr), data(data) {}

  };



  struct utype_t {
    uint32_t opcode : 7;
    uint32_t rd : 5;
    uint32_t imm : 20;
  };

  struct branch_t {
    uint32_t opcode : 7;
    uint32_t imm11 : 1; //8
    uint32_t imm4_1 : 4; //12
    uint32_t sel: 3; //15
    uint32_t rs1 : 5; //20
    uint32_t rs2 : 5; //25
    uint32_t imm10_5 : 6; //31
    uint32_t imm12 : 1; //32
  };

  struct jal_t {
    uint32_t opcode : 7;
    uint32_t rd : 5;
    uint32_t imm19_12 : 8;
    uint32_t imm11 : 1;
    uint32_t imm10_1 : 10;
    uint32_t imm20 : 1;
  };

  struct jalr_t {
    uint32_t opcode : 7;
    uint32_t rd : 5; //12
    uint32_t mbz : 3; //15
    uint32_t rs1 : 5; //20
    uint32_t imm11_0 : 12; //32
  };

  struct rtype_t {
    uint32_t opcode : 7;
    uint32_t rd : 5;
    uint32_t sel: 3;
    uint32_t rs1 : 5;
    uint32_t rs2 : 5;
    uint32_t special : 7;
  };

  struct itype_t {
    uint32_t opcode : 7;
    uint32_t rd : 5;
    uint32_t sel : 3;
    uint32_t rs1 : 5;
    uint32_t imm : 12;
  };

  struct store_t {
    uint32_t opcode : 7;
    uint32_t imm4_0 : 5; //12
    uint32_t sel : 3; //15
    uint32_t rs1 : 5; //20
    uint32_t rs2 : 5; //25
    uint32_t imm11_5 : 7; //32
  };

  struct load_t {
    uint32_t opcode : 7;
    uint32_t rd : 5; //12
    uint32_t sel : 3; //15
    uint32_t rs1 : 5; //20
    uint32_t imm11_0 : 12; //32
  };

  struct amo_t {
    uint32_t opcode : 7;
    uint32_t rd : 5; //12
    uint32_t sel : 3; //15
    uint32_t rs1 : 5; //20
    uint32_t rs2 : 5; //25
    uint32_t rl : 1; //27
    uint32_t aq : 1;
    uint32_t hiop : 5;
  };

  union riscv_t {
    rtype_t r;
    itype_t i;
    utype_t u;
    jal_t j;
    jalr_t jj;
    branch_t b;
    store_t s;
    load_t l;
    amo_t a;
    uint32_t raw;
    riscv_t(uint32_t x) : raw(x) {}
  };

  struct mstatus_t {
    uint64_t j0 : 1;
    uint64_t sie : 1;
    uint64_t j2 : 1;
    uint64_t mie : 1;
    uint64_t j4 : 1;
    uint64_t spie : 1;
    uint64_t ube : 1;
    uint64_t mpie : 1;
    uint64_t spp : 1;
    uint64_t vs : 2;
    uint64_t mpp : 2;
    uint64_t fs : 2;
    uint64_t xs : 2;
    uint64_t mprv : 1;
    uint64_t sum : 1;
    uint64_t mxr : 1;
    uint64_t tvm : 1;
    uint64_t tw : 1;
    uint64_t tsr : 1;
    uint64_t junk23 : 9;
    uint64_t uxl : 2;
    uint64_t sxl : 2;
    uint64_t sbe : 1;
    uint64_t mbe : 1;
    uint64_t junk38 : 25;
    uint64_t sd : 1;
  };

  union csr_t {
    satp_t satp;
    mip_t mip;
    mie_t mie;
    mstatus_t mstatus;
    uint64_t raw;
    csr_t(uint64_t x) : raw(x) {}
  };

  static inline std::ostream &operator<<(std::ostream &out, mstatus_t mstatus) {
    out << "sie  " << mstatus.sie << "\n";
    out << "mie  " << mstatus.mie << "\n";
    out << "spie " << mstatus.spie << "\n";
    out << "ube  " << mstatus.ube << "\n";  
    out << "mpie " << mstatus.mpie << "\n";
    out << "spp  " << mstatus.spp << "\n";
    out << "mpp  " << mstatus.mpp << "\n";
    out << "fs   " << mstatus.fs << "\n";
    out << "xs   " << mstatus.xs << "\n";      
    out << "mprv " << mstatus.mprv << "\n";
    out << "sum  " << mstatus.sum << "\n";
    out << "mxr  " << mstatus.mxr << "\n";
    out << "tvm  " << mstatus.tvm << "\n";
    out << "tw   " << mstatus.tw << "\n";      
    out << "tsr  " << mstatus.tsr << "\n";
    out << "uxl  " << mstatus.uxl << "\n";
    out << "sxl  " << mstatus.sxl << "\n";
    out << "sbe  " << mstatus.sbe << "\n";
    out << "mbe  " << mstatus.mbe << "\n";
    out << "sd   " << mstatus.sd << "\n";
    return out;
  }


  struct sv39_t {
    uint64_t v : 1; //0
    uint64_t r : 1; //1
    uint64_t w : 1; //2
    uint64_t x : 1; //3
    uint64_t u : 1; //4
    uint64_t g : 1; //5
    uint64_t a : 1; //6
    uint64_t d : 1; //7
    uint64_t rsw : 2; //10
    uint64_t ppn : 44;
    uint64_t mbz : 7;
    uint64_t pbmt : 2;
    uint64_t n : 1;
  };

  union pte_t {
    sv39_t sv39;
    uint64_t r;
    pte_t(uint64_t x) : r(x) {}
  };  
};


#endif
