#include <cmath>
#include <map>
#include <set>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "globals.hh"
#include "mips_op.hh"
#include "helper.hh"
#include "disassemble.hh"
#include "sim_parameters.hh"
#include "sim_cache.hh"
#include "machine_state.hh"


std::ostream &operator<<(std::ostream &out, const mips_op &op) {
  out << std::hex << op.m->pc << std::dec
      << ":" << getAsmString(op.m->inst, op.m->pc) << ":"
      << op.m->fetch_cycle << ","
      << op.m->decode_cycle << ","
      << op.m->alloc_cycle << ","
      << op.m->ready_cycle << ","
      << op.m->dispatch_cycle << ","
      << op.m->complete_cycle;
  return out;
}



mips_meta_op::~mips_meta_op() {
  if(op) {
    delete op;
  }
}

void mips_meta_op::release() {
  if(op) {
    delete op;
    op = nullptr;
  }
}

bool mips_op::allocate(sim_state &machine_state) {
  die();
  return false;
}

void mips_op::execute(sim_state &machine_state) {
  die();
}

void mips_op::complete(sim_state &machine_state) {
  die();
}

bool mips_op::retire(sim_state &machine_state) {
  log_retire(machine_state);

  return false;
}

bool mips_op::ready(sim_state &machine_state) const {
  return false;
}

void mips_op::rollback(sim_state &machine_state) {
  log_rollback(machine_state);
  die();
}

void mips_op::log_retire(sim_state &machine_state) const {

  if((machine_state.icnt >= global::pipestart) and
     (machine_state.icnt < global::pipeend)) {

    machine_state.sim_records->append(m->alloc_id,
				      getAsmString(m->inst, m->pc),
				      m->pc,
				      static_cast<uint64_t>(m->fetch_cycle),
				      static_cast<uint64_t>(m->alloc_cycle),
				      static_cast<uint64_t>(m->complete_cycle),
				      static_cast<uint64_t>(m->retire_cycle),
				      false
				      );
  }  
  //*global::sim_log  << *this << "\n";
  //std::cout << machine_state.rob.size() << "\n";

  //std::cout << *this << " : retirement freevec = "
  //<< machine_state.gpr_freevec_retire.popcount()
  //<< "\n";
  
  //assert(machine_state.gpr_freevec_retire.popcount()==sim_state::num_gpr_regs);
  //assert(machine_state.cpr1_freevec_retire.popcount()==sim_state::num_cpr1_regs);
  
}

void mips_op::log_rollback(sim_state &machine_state) const {}

int64_t mips_op::get_latency() const {
  return 1;
}


mips_op* decode_insn(sim_op m_op) {
  uint32_t opcode = (m_op->inst)&127;
  switch(opcode)
    {
    default:
      break;
    }
  std::cout << "implement opcode " << std::hex << opcode << std::dec << "\n";
  return nullptr;
}
