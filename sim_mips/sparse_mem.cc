#include "sparse_mem.hh"
#include <cstring>

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

sparse_mem::~sparse_mem() {
  for(size_t i = 0; i < npages; i++) {
    if(mem[i]) {
      free(mem[i]);
    }
  }
  delete [] mem;
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
