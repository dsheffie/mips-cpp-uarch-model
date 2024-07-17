#include <string>
#include <cstdint>
#include <array>

#ifndef __DISASSEMBLE_HH__
#define __DISASSEMBLE_HH__

#define FMT_S 16 /* single precision */
#define FMT_D 17 /* double precision */
#define FMT_E 18 /* extended precision */
#define FMT_Q 19 /* quad precision */
#define FMT_W 20 /* 32-bit fixed */
#define FMT_L 21 /* 64-bit fixed */

#define COND_F 0
#define COND_UN 1
#define COND_EQ 2
#define COND_UEQ 3
#define COND_OLT 4
#define COND_ULT 5
#define COND_OLE 6
#define COND_ULE 7
#define COND_SF 8
#define COND_NGLE 9
#define COND_SEQ 10
#define COND_NGL 11
#define COND_LT 12
#define COND_NGE 13
#define COND_LE 14
#define COND_NGT 15

#define CP1_CR0 0
#define CP1_CR31 1
#define CP1_CR25 2
#define CP1_CR26 3
#define CP1_CR28 4

const std::string &getCondName(uint32_t c);
const std::string &getGPRName(uint32_t r);

void initCapstone();
void stopCapstone();

std::string getAsmString(uint32_t inst,uint64_t addr);
void disassemble(std::ostream &out, uint32_t inst, uint64_t addr);


#endif
