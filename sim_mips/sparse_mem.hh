#ifndef __sparse_mem_hh__
#define __sparse_mem_hh__

#include <cstdlib>
#include <cassert>
#include <cstring>
#include <cstdint>
#include <iostream>

#include "sim_bitvec.hh"

#include <unistd.h>

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
  sparse_mem(uint64_t nbytes = 1UL<<32) {
    npages = (nbytes+pgsize-1) / pgsize;
    present_bitvec.clear_and_resize(npages);
    mem = new uint8_t*[npages];
    memset(mem, 0, sizeof(uint8_t*)*npages);
  }
  sparse_mem(const sparse_mem &other) {
    npages = other.npages;
    present_bitvec.clear_and_resize(npages);
    mem = new uint8_t*[npages];
    memset(mem, 0, sizeof(uint8_t*)*npages);
    for(size_t i = 0; i < npages; i++) {
      if(other.mem[i]) {
	present_bitvec.set_bit(i);
	int rc = posix_memalign(reinterpret_cast<void**>(&mem[i]), 4096, pgsize);
	assert(rc==0);
	memcpy(mem[i], other.mem[i], pgsize);
      }
    }
  }
  ~sparse_mem() {
    for(size_t i = 0; i < npages; i++) {
      if(mem[i]) {
	free(mem[i]);
      }
    }
    delete [] mem;
  }
  bool equal(const sparse_mem &other) const {
    if(other.npages != npages) {
      return false;
    }
    
    int64_t p0 = other.present_bitvec.find_first_set();
    int64_t p1 = present_bitvec.find_first_set();
      
    while(p0!=-1 and p1 !=-1) {
      assert(other.mem[p0]);
      assert(mem[p1]);
      //std::cerr << "p0 = " << p0 << ",p1 = " << p1 << "\n";

      if(p0==p1) {
	int d = memcmp(mem[p1], other.mem[p0], pgsize);
	if(d != 0) {
	  std::cout << "page " << p1 << " mismatch!\n";
	  for(int i = 0; i < pgsize; i++) {
	    if(other.mem[p0][i] != mem[p1][i]) {
	      std::cout << "byte " << i << " differs : "
			<< std::hex
			<< static_cast<uint32_t>(other.mem[p0][i]) << ","
		        << static_cast<uint32_t>(mem[p1][i])
			<< std::dec << "\n";
	    }
	  }
	  return false;
	}
	p0 = other.present_bitvec.find_next_set(p0);
	p1 = present_bitvec.find_next_set(p1);
      }
      else if(p0 < p1) {
	if(!other.is_zero(p0)) {
	  std::cout << "p0 present page " << p0 << " mismatch!\n";
	  return false;
	}
	p0 = other.present_bitvec.find_next_set(p0);
      }
      else {
	if(!is_zero(p1)) {
	  std::cout << "p1 present page " << p1 << " mismatch!\n";
	  return false;
	}
	p1 = present_bitvec.find_next_set(p1);
      }
    }
    return true;
  }
  uint8_t & at(uint64_t addr) {
    uint64_t paddr = addr / pgsize;
    uint64_t baddr = addr % pgsize;
    if(mem[paddr]==nullptr) {
      present_bitvec.set_bit(paddr);
      int rc = posix_memalign(reinterpret_cast<void**>(&mem[paddr]), 4096, pgsize);
      assert(rc==0);
      memset(mem[paddr],0,pgsize);
    }
    return mem[paddr][baddr];
  }
  uint8_t * operator[](uint64_t addr) {
    uint64_t paddr = addr / pgsize;
    uint64_t baddr = addr % pgsize;
    if(mem[paddr]==nullptr) {
      present_bitvec.set_bit(paddr);
      int rc = posix_memalign(reinterpret_cast<void**>(&mem[paddr]), 4096, pgsize);
      assert(rc==0);
      memset(mem[paddr],0,pgsize);
    }
    return &mem[paddr][baddr];
  }
  void prefault(uint64_t addr) {
    uint64_t paddr = addr / pgsize;
    if(mem[paddr]==nullptr) {
      present_bitvec.set_bit(paddr);
      int rc = posix_memalign(reinterpret_cast<void**>(&mem[paddr]), 4096, pgsize);
      assert(rc==0);
      memset(mem[paddr],0,pgsize);
    }
  }
  uint32_t& get32(uint64_t byte_addr) {
    uint64_t paddr = byte_addr / pgsize;
    uint64_t baddr = byte_addr % pgsize;
#if 0
    if(mem[paddr]==nullptr) {
      std::cerr << "ACCESS TO INVALID PAGE 0x"
	<< std::hex
	<< paddr*pgsize
	<< std::dec
	<< "\n";
	   exit(-1);
    }
#endif
    return *reinterpret_cast<uint32_t*>(mem[paddr]+baddr);
  }
  uint32_t get32(uint64_t byte_addr) const {
    uint64_t paddr = byte_addr / pgsize;
    uint64_t baddr = byte_addr % pgsize;
#if 0
    if(mem[paddr]==nullptr) {
      std::cerr << "ACCESS TO INVALID PAGE 0x"
	<< std::hex
	<< paddr*pgsize
	<< std::dec
	<< "\n";
	   exit(-1);
    }
#endif
    return *reinterpret_cast<uint32_t*>(mem[paddr]+baddr);
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
