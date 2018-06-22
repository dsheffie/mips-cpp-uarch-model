#ifndef __sparse_mem_hh__
#define __sparse_mem_hh__

#include <cstdlib>
#include <cassert>
#include <cstring>
#include <cstdint>
#include <iostream>
#include <unistd.h>
 #include <sys/mman.h>

#include "sim_bitvec.hh"
#include "helper.hh"


class sparse_mem {
public:
  static const uint64_t pgsize = 4096;
private:
  size_t npages = 0;
  uint8_t **mem = nullptr;
  sim_bitvec_template<uint64_t> present_bitvec;
  bool is_zero(uint64_t p) const {
    for(uint64_t i = 0; i < pgsize; i++) {
      if(mem[p][i])
	return false;
    }
    return true;
  }
public:
  sparse_mem(uint64_t nbytes = 1UL<<32);
  sparse_mem(const sparse_mem &other);
  void copy(const sparse_mem &other);
  ~sparse_mem();
  void mark_pages_as_no_write();
  bool get_pb_addr(uint32_t addr, uint32_t &paddr, uint32_t &baddr) {
    paddr = addr / pgsize;
    baddr = addr % pgsize;
    return(mem[paddr]!=nullptr);
  }
  uint32_t parity(uint32_t addr) const {
    uint32_t paddr = addr / pgsize;
    if(mem[paddr] == nullptr)
      return 0;
    uint8_t p = 0;
    for(int i = 0; i < pgsize; i++) {
      p ^= mem[paddr][i];
    }
    return static_cast<uint32_t>(p);
  }
  bool equal(const sparse_mem &other) const;
  uint8_t & at(uint32_t addr) {
    uint32_t paddr = addr / pgsize;
    uint32_t baddr = addr % pgsize;
    if(mem[paddr]==nullptr) {
      present_bitvec.set_bit(paddr);
      int rc = posix_memalign(reinterpret_cast<void**>(&mem[paddr]), 4096, pgsize);
      assert(rc==0);
      memset(mem[paddr],0,pgsize);
    }
    return mem[paddr][baddr];
  }
  uint8_t * operator[](uint32_t addr) {
    uint32_t paddr = addr / pgsize;
    uint32_t baddr = addr % pgsize;
    if(mem[paddr]==nullptr) {
      present_bitvec.set_bit(paddr);
      int rc = posix_memalign(reinterpret_cast<void**>(&mem[paddr]), 4096, pgsize);
      assert(rc==0);
      memset(mem[paddr],0,pgsize);
    }
    return &mem[paddr][baddr];
  }
  void prefault(uint32_t addr) {
    uint32_t paddr = addr / pgsize;
    if(mem[paddr]==nullptr) {
      present_bitvec.set_bit(paddr);
      int rc = posix_memalign(reinterpret_cast<void**>(&mem[paddr]), 4096, pgsize);
      assert(rc==0);
      memset(mem[paddr],0,pgsize);
    }
  }
  template <typename T>
  T get(uint32_t byte_addr) {
    uint32_t paddr = byte_addr / pgsize;
    uint32_t baddr = byte_addr % pgsize;
    if(mem[paddr]==nullptr) {
      present_bitvec.set_bit(paddr);
      int rc = posix_memalign(reinterpret_cast<void**>(&mem[paddr]), 4096, pgsize);
      assert(rc==0);
      memset(mem[paddr],0,pgsize);
    }
    return *reinterpret_cast<T*>(mem[paddr]+baddr);
  }
  template <typename T>
  T load(uint32_t byte_addr) {
    return get<T>(byte_addr);
  }      
  template<typename T>
  void set(uint32_t byte_addr, T v) {
    uint32_t paddr = byte_addr / pgsize;
    uint32_t baddr = byte_addr % pgsize;
    if(mem[paddr]==nullptr) {
      present_bitvec.set_bit(paddr);
      int rc = posix_memalign(reinterpret_cast<void**>(&mem[paddr]), 4096, pgsize);
      assert(rc==0);
      memset(mem[paddr],0,pgsize);
    }
    *reinterpret_cast<T*>(mem[paddr]+baddr) = v;
  }
  template<typename T>
  void store(uint32_t byte_addr, T v) {
    set<T>(byte_addr, v);
  }
  uint32_t get32(uint32_t byte_addr)  {
    return get<uint32_t>(byte_addr);
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

template <bool do_write>
int per_page_rdwr(sparse_mem &mem, int fd, uint32_t offset, uint32_t nbytes) {
  uint32_t last_byte = (offset+nbytes);
  int acc = 0, rc = 0;
  while(offset != last_byte) {
    uint64_t next_page = (offset & (~(sparse_mem::pgsize-1))) + sparse_mem::pgsize;
    next_page = std::min(next_page, static_cast<uint64_t>(last_byte));
    uint32_t disp = (next_page - offset);
    mem.prefault(offset);
    if(do_write)
      rc = write(fd, mem + offset, disp);
    else 
      rc = read(fd, mem + offset, disp);
    if(rc == 0)
      return 0;
    acc += rc;
    offset += disp;
  }
  return acc;
}


#endif
