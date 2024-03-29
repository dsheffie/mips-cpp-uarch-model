#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cassert>
#include <utility>
#include <cstdint>
#include <list>

#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>

#ifdef __APPLE__
#include <TargetConditionals.h>
#if TARGET_OS_MAC
#include <libelf/gelf.h>
#endif /* TARGET_OS_MAC */
#else
#include <elf.h>
#endif

#include "helper.hh"
#include "state.hh"


static const uint8_t magicArr[4] = {0x7f, 'E', 'L', 'F'};
static bool checkElf(const Elf32_Ehdr *eh32) {
  uint8_t *identArr = (uint8_t*)eh32->e_ident;
  return memcmp((void*)magicArr, identArr, 4)==0;
}
static bool check32Bit(const Elf32_Ehdr *eh32) {
  return (eh32->e_ident[EI_CLASS] == ELFCLASS32);
}
static bool checkBigEndian(const Elf32_Ehdr *eh32) {
  return (eh32->e_ident[EI_DATA] == ELFDATA2MSB);
}
static bool checkLittleEndian(const Elf32_Ehdr *eh32) {
  return (eh32->e_ident[EI_DATA] == ELFDATA2LSB);
}


void load_elf(const char* fn, state_t *ms) {
  struct stat s;
  Elf32_Ehdr *eh32 = nullptr;
  Elf32_Phdr* ph32 = nullptr;
  Elf32_Shdr* sh32 = nullptr;
  int32_t e_phnum=-1,e_shnum=-1;
  size_t pgSize = getpagesize();
  int fd,rc;
  char *buf = nullptr;
  sparse_mem &mem = ms->mem;

  fd = open(fn, O_RDONLY);
  if(fd<0) {
    std::cerr << __PRETTY_FUNCTION__ << ": open() returned " << fd << "\n";
    exit(-1);
  }
  rc = fstat(fd,&s);
  if(rc<0) {
    std::cerr << __PRETTY_FUNCTION__ << ": fstat() returned " << rc << "\n";
    exit(-1);
  }
  buf = (char*)mmap(nullptr, s.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
  eh32 = (Elf32_Ehdr *)buf;
  close(fd);
    
  if(not(checkElf(eh32)) or not(check32Bit(eh32))) {
    std::cerr << __PRETTY_FUNCTION__ << ": bogus binary\n";
    exit(-1);
  }

  /* Check for a MIPS machine */
  if(not(checkBigEndian(eh32))) {
    std::cerr << __PRETTY_FUNCTION__ << ": not big endian ELF\n";
    exit(-1);

  }
  
  if(bswap(eh32->e_machine) != 8) {
    std::cerr << __PRETTY_FUNCTION__ << ": not MIPS binary\n";
    exit(-1);
  }

  uint32_t lAddr = bswap(eh32->e_entry);

  e_phnum = bswap(eh32->e_phnum);
  ph32 = (Elf32_Phdr*)(buf + bswap(eh32->e_phoff));
  e_shnum = bswap(eh32->e_shnum);
  sh32 = (Elf32_Shdr*)(buf + bswap(eh32->e_shoff));
  ms->pc = lAddr;

  /* Find instruction segments and copy to
   * the memory buffer */
  //uint32_t code_sz = 0;
  for(int32_t i = 0; i < e_phnum; i++, ph32++) {
    int32_t p_memsz = bswap(ph32->p_memsz);
    int32_t p_offset = bswap(ph32->p_offset);
    int32_t p_filesz = bswap(ph32->p_filesz);
    int32_t p_type = bswap(ph32->p_type);
    uint32_t p_vaddr = bswap(ph32->p_vaddr);
    if(p_type == SHT_PROGBITS && p_memsz) {
      if( (p_vaddr + p_memsz) > lAddr)
	lAddr = (p_vaddr + p_memsz);

      for(int32_t cc = 0; cc < p_memsz; cc++) {
	mem.at(cc+p_vaddr) = 0;
      }
      for(int32_t cc = 0; cc < p_filesz; cc++) {
	mem.at(cc+p_vaddr) = reinterpret_cast<uint8_t*>(buf + p_offset)[cc];
      }
      //code_sz += p_filesz;
    }
  }
  munmap(buf, s.st_size);
}
