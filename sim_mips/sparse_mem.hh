#ifndef __sparse_mem_hh__
#define __sparse_mem_hh__



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
    for(size_t i = 0; i < npages; i++) {
      if(other.mem[i]) {
	mem[i] = new uint8_t[pgsize];
	memcpy(mem[i], other.mem[i], pgsize);
      }
    }
  }
  ~sparse_mem() {
    for(size_t i = 0; i < npages; i++) {
      if(mem[i]) {
	delete [] mem[i];
      }
    }
    delete [] mem;
  }
  uint8_t & at(uint64_t addr) {
    uint64_t paddr = addr / pgsize;
    uint64_t baddr = addr % pgsize;
    if(mem[paddr]==nullptr) {
      mem[paddr] = new uint8_t[pgsize];
      memset(mem[paddr],0,pgsize);
    }
    return mem[paddr][baddr];
  }
  uint8_t * operator[](uint64_t addr) {
    uint64_t paddr = addr / pgsize;
    uint64_t baddr = addr % pgsize;
    if(mem[paddr]==nullptr) {
      mem[paddr] = new uint8_t[pgsize];
      memset(mem[paddr],0,pgsize);
    }
    return &mem[paddr][baddr];
  }
  void prefault(uint64_t addr) {
    uint64_t paddr = addr / pgsize;
    if(mem[paddr]==nullptr) {
      mem[paddr] = new uint8_t[pgsize];
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


#endif
