#ifndef __sparse_mem_hh__
#define __sparse_mem_hh__

#include <cstdlib>
#include <cassert>
#include <cstring>
#include <cstdint>
#include <unistd.h>

class sparse_mem {
public:
  static const uint64_t pgsize = 4096;
private:
  size_t npages = 0;
  uint8_t **mem = nullptr;
  bool is_zero(uint64_t p) const {
    for(uint64_t i = 0; i < pgsize; i++) {
      if(mem[p][i])
	return false;
    }
    return true;
  }
public:
  sparse_mem(uint64_t nbytes) {
    npages = (nbytes+pgsize-1) / pgsize;
    mem = new uint8_t*[npages];
    memset(mem, 0, sizeof(uint8_t*)*npages);
  }
  sparse_mem(const sparse_mem &other) {
    npages = other.npages;
    mem = new uint8_t*[npages];
    memset(mem, 0, sizeof(uint8_t*)*npages);
    for(size_t i = 0; i < npages; i++) {
      if(other.mem[i]) {
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
    for(size_t p = 0; p < npages; p++) {
      if((mem[p]!=nullptr) and (other.mem[p]==nullptr)) {
	bool all_zero = true;
	for(size_t i = 0; i < pgsize; i++) {
	  if(mem[p][i] != 0) {
	    all_zero = false;
	    break;
	  }
	}
	if(all_zero)
	  continue;
	else
	  return false;
      }
      else if(mem[p]==nullptr and other.mem[p]!=nullptr) {
	return false;
      }
      else if(mem[p]==nullptr and other.mem[p]==nullptr) {
	continue;
      }
      for(size_t i = 0; i < pgsize; i++) {
	if(mem[p][i] != other.mem[p][i]) {
	  return false;
	}
      }
    }
    return true;
  }
  uint8_t & at(uint64_t addr) {
    uint64_t paddr = addr / pgsize;
    uint64_t baddr = addr % pgsize;
    if(mem[paddr]==nullptr) {
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
      int rc = posix_memalign(reinterpret_cast<void**>(&mem[paddr]), 4096, pgsize);
      assert(rc==0);
      memset(mem[paddr],0,pgsize);
    }
    return &mem[paddr][baddr];
  }
  void prefault(uint64_t addr) {
    uint64_t paddr = addr / pgsize;
    if(mem[paddr]==nullptr) {
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
