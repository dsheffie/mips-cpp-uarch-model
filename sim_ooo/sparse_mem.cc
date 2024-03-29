#include <cstring>
#ifdef __amd64__
#include <x86intrin.h>
#endif

#include "sparse_mem.hh"

sparse_mem::sparse_mem(uint64_t nbytes) {
  npages = (nbytes+pgsize-1) / pgsize;
  present_bitvec.clear_and_resize(npages);
  mem = new uint8_t*[npages];
  memset(mem, 0, sizeof(uint8_t*)*npages);
}

sparse_mem::sparse_mem(const sparse_mem &other) {
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

uint32_t sparse_mem::crc32() const {
  uint32_t c = ~0x0;
  //std::cout << present_bitvec.popcount()
  //<< " non-zero pages\n";
  for(size_t i = 0; i < npages; i++) {
#ifdef __amd64__
    if(present_bitvec[i]==false) {
      for(size_t n=0;n<4096;n++) {
	c = _mm_crc32_u8(c, 0);
      }
    }
    else {
      //uint8_t x = 0;
      for(size_t n=0;n<4096;n++) {
	c = _mm_crc32_u8(c, mem[i][n]);
	//x ^= mem[i][n];
      }
      //std::cout << "page " << i << " is non-zero, x = "
      //<< std::hex << static_cast<uint32_t>(x) << std::dec << "\n";
    }
#else
    static const uint32_t POLY = 0x82f63b78;
    for(size_t n=0;n<4096;n++) {
      uint8_t b = present_bitvec[i] ? mem[i][n] : 0;
      c ^= b;
      for(int k = 0; k < 8; k++) {
	c = c & 1 ? (c>>1) ^ POLY : c>>1;
      }
    }
#endif
  }
  return c ^ (~0x0);
}

void sparse_mem::copy(const sparse_mem &other) {
  for(size_t i = 0; i < npages; i++) {
    if(mem[i]) {
      free(mem[i]);
    }
  }
  delete [] mem;
  present_bitvec.clear();
  
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


sparse_mem::~sparse_mem() {
  for(size_t i = 0; i < npages; i++) {
    if(mem[i]) {
      free(mem[i]);
    }
  }
  delete [] mem;
}

static int sparse_mem_handler(void *fault_addr, int serious) {
  uint64_t mask = ~(4095UL);
  uint64_t page = reinterpret_cast<uint64_t>(fault_addr) & mask;
  std::cerr << std::hex << "fault address = " << fault_addr << std::dec << "\n";
  std::cerr << std::hex << "page = " << page << std::dec << "\n";
  int rc = mprotect(reinterpret_cast<void*>(page), 4096, PROT_WRITE | PROT_READ);
  assert(rc==0);
  return 1;
}

void sparse_mem::mark_pages_as_no_write() {
  int64_t p = present_bitvec.find_first_set();
  while(p != -1) {
    int rc = mprotect(mem[p], pgsize,  PROT_READ);
    assert(rc == 0);
    p = present_bitvec.find_next_set(p);
  }

}

bool sparse_mem::equal(const sparse_mem &other) const {
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
