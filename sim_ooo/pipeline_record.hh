#ifndef __pipeline_record_hh__
#define __pipeline_record_hh__

#include <cstdint>
#include <fstream>
#include <string>
#include <list>

class pipeline_record {
public:
  uint64_t uuid;
  std::string disasm;
  uint64_t pc;
  uint64_t fetch_cycle, alloc_cycle, complete_cycle, retire_cycle;
  bool faulted;
public:
  pipeline_record(uint64_t uuid,
		  const std::string &disasm,
		  uint64_t pc,
		  uint64_t fetch_cycle,
		  uint64_t alloc_cycle,
		  uint64_t complete_cycle,
		  uint64_t retire_cycle,
		  bool faulted) :
    uuid(uuid), disasm(disasm), pc(pc), fetch_cycle(fetch_cycle), alloc_cycle(alloc_cycle),
    complete_cycle(complete_cycle), retire_cycle(retire_cycle),
    faulted(faulted) {}
  pipeline_record() :
    uuid(~0UL), disasm(""), pc(0), fetch_cycle(0), alloc_cycle(0),
    complete_cycle(0), retire_cycle(0), faulted(false) {}

  friend std::ostream &operator<<(std::ostream &out, const pipeline_record &r) {
    out << r.disasm << "," << std::hex <<  r.pc << std::dec
	<< "," << r.fetch_cycle << "," << r.alloc_cycle
	<< "," << r.complete_cycle << "," << r.retire_cycle
	<< "," << r.faulted;
    return out;
  }
};


class pipeline_data {
protected:
  std::list<pipeline_record> records;
public:
  pipeline_data() {}
  const std::list<pipeline_record> &get_records() const {
    return records;
  }
  std::list<pipeline_record> &get_records() {
    return records;
  }  
};

class pipeline_logger : public pipeline_data {
private:
  std::ofstream *ofs = nullptr;
public:
  pipeline_logger(const std::string &out) {
  }
  ~pipeline_logger() {
  }
  void append(uint64_t uuid,
	      const std::string &disasm,
	      uint64_t pc,
	      uint64_t fetch_cycle,
	      uint64_t alloc_cycle,
	      uint64_t complete_cycle,
	      uint64_t retire_cycle,
	      bool faulted) {}
};


class pipeline_reader : public pipeline_data {
public:
  pipeline_reader() {}
  
  void read(const std::string &fname) {}
  
};



#endif
