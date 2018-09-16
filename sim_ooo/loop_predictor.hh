#ifndef __loop_predictor_hh__
#define __loop_predictor_hh__

#include <cstdint>

class loop_predictor {
private:
  enum class entry_state {invalid,training,locked};
  struct entry {
    entry_state state;
    uint32_t count;
    uint32_t limit;
    uint32_t pc;
    void clear() {
      state = entry_state::invalid;
      count = limit = pc = 0;
    }
  };
  uint32_t n_entries = 1;
  entry *arr = nullptr;
public:
  loop_predictor(uint32_t n_entries);
  ~loop_predictor();
  bool valid_loop_branch(uint32_t pc) const;
  bool predict(uint32_t pc);
  void update(uint32_t pc, bool take_br);
  
};


#endif
