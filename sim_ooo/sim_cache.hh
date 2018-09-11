#ifndef __SIM_CACHE_H__
#define __SIM_CACHE_H__
#include <string>
#include <sstream>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>
#include <cassert>
#include <cstdint>
#include <array>
#include <list>
#include <unordered_set>
#include <iterator>
#include <boost/pool/object_pool.hpp>
#include <boost/dynamic_bitset.hpp>
#include "sim_list.hh"

class mips_meta_op;

enum class opType {READ,WRITE};

#ifndef print_var
#define print_var(x) { std::cout << #x << " = " << x << "\n"; }
#endif

class simCache {
protected:
  template <typename T>
  class mylist {
  private:
    class entry {
    private:
      friend class mylist;
      entry *next;
      entry *prev;
      T data;
    public:
      entry(T data) : next(nullptr), prev(nullptr), data(data) {}
      const T &getData() const {
	return data;
      }
      T &getData() {
	return data;
      }
      void set_from_tail(entry *p) {
	p->next = this;
	prev = p;
      }
      void set_from_head(entry *n) {
	n->prev = this;
	next = n;
      }
      void unlink() {
	if(next) {
	  next->prev = prev;
	}
	if(prev) {
	  prev->next = next;
	}
      }
      void clear() {
	prev = next = nullptr;
      }
    };
    boost::object_pool<entry> pool;
    size_t cnt;
    entry *head, *tail;
  
    entry* alloc(T v) {
      return pool.construct(v);
    }
    void free(entry* e) {
      pool.free(e);
    }
  
  public:
    mylist() : cnt(0), head(nullptr), tail(nullptr) {}
  
    void push_back(T v) {
      entry* e = alloc(v);
      cnt++;
      if(head == nullptr && tail == nullptr) {
	head = tail = e;
      }
      else {
	e->set_from_tail(tail);
	tail = e;
      }
    }
    void push_front(T v) {
      entry* e = alloc(v);
      cnt++;
      if(head == nullptr && tail == nullptr) {
	head = tail = e;
      }
      else {
	e->set_from_head(head);
	head = e;
      }
    }
    void pop_back() {
      if(tail==nullptr)
	return;
      entry *ptr = tail;
      ptr->unlink();
      tail = ptr->prev;
      free(ptr);
      cnt--;
    }
    class const_iterator : public std::iterator<std::forward_iterator_tag, mylist> {
    protected:
      friend class mylist;
      entry *ptr;
      ssize_t dist;
      const_iterator(entry *ptr, ssize_t dist) : ptr(ptr), dist(dist) {};
    public:
      const_iterator() : ptr(nullptr), dist(-1) {}
      const bool operator==(const const_iterator &rhs) const {
	return ptr == rhs.ptr;
      }
      const bool operator!=(const const_iterator &rhs) const {
	return ptr != rhs.ptr;
      }
      const T &operator*() const {
	return ptr->getData();
      }
      const_iterator operator++(int postfix) {
	const_iterator it = *this;
	ptr=ptr->next;
	return it;
      }
      const_iterator operator++() {
	ptr=ptr->next;
	return *this;
      }
    };
  
    class iterator : public const_iterator {
    private:
      friend class mylist;
      iterator(entry *ptr, ssize_t dist) : const_iterator(ptr,dist) {}
    public:
      iterator() : const_iterator() {}
      T &operator*() {
	return iterator::ptr->getData();
      }
    };
    const_iterator begin() const {
      return const_iterator(head,0);
    }
    const_iterator end() const {
      return const_iterator(nullptr,-1);
    }
    iterator begin() {
      return iterator(head,0);
    }
    iterator end() {
      return iterator(nullptr,-1);
    }
    iterator find(T v) {
      entry *p = head;
      ssize_t cnt = 0;
      while(p) {
	if(p->getData() == v)
	  return iterator(p,cnt);
	p = p->next;
	cnt++;
      }
      return end();
    }
    void move_to_tail(iterator it) {
      entry *ptr = it.ptr;
      if(cnt<=1)
	return;
      ptr->unlink();
      if(ptr == head) {
	head = ptr->next;
      }
      if(ptr == tail) {
	tail = ptr->prev;
      }
      ptr->clear();
      ptr->set_from_tail(tail);
      tail = ptr;
    }
    void move_to_head(iterator it) {
      entry *ptr = it.ptr;
      if(cnt<=1)
	return;
      ptr->unlink();
      if(ptr == head) {
	head = ptr->next;
      }
      if(ptr == tail) {
	tail = ptr->prev;
      }
      ptr->clear();
      ptr->set_from_head(head);
      head = ptr;
    }
    void erase(iterator it) {
      if(it == end())
	return;
      it.ptr->unlink();
      if(it.ptr == head) {
	head = it.ptr->next;
      }
      if(it.ptr == tail) {
	tail = it.ptr->prev;
      }
      free(it.ptr);
      cnt--;
    }
    ssize_t distance(iterator it) {
      return it.dist;
    }
    size_t size() const {
      return cnt;
    }
    bool empty() const {
      return cnt==0;
    }
  };
  /* cache size stats */
  size_t bytes_per_line;
  size_t assoc;
  size_t num_sets;
  std::string name;
  int latency;
  simCache *next_level;
  size_t hits,misses;
  
  size_t total_cache_size;
  size_t ln2_tag_bits;
  size_t ln2_offset_bits;
  size_t ln2_num_sets;
  size_t ln2_bytes_per_line;
  
  /* total stats */
  std::array<size_t,2> rw_hits;
  std::array<size_t,2> rw_misses;

  sim_list<mips_meta_op*> inflight;
public:
  friend std::ostream &operator<<(std::ostream &out, const simCache &cache);
  simCache(size_t bytes_per_line, size_t assoc, size_t num_sets, 
	   std::string name, int latency, simCache *next_level);
  virtual ~simCache();
  
  void set_next_level(simCache *next_level);
  
  uint32_t index(uint32_t addr, uint32_t &l, uint32_t &t);
  virtual bool access(uint32_t addr, uint32_t num_bytes, opType o, uint32_t &lat)=0;
  
  uint32_t read(mips_meta_op *op, uint32_t addr, uint32_t num_bytes);
  uint32_t write(mips_meta_op *op, uint32_t addr, uint32_t num_bytes);
  void nuke_inflight();
  virtual void tick();
  const size_t &getHits() const {
    return hits;
  }
  const size_t &getMisses() const {
    return misses;
  }
  size_t capacity() const {
    return bytes_per_line*assoc*num_sets;
  }
  bool miss_handlers_available() const {
    return not(inflight.full());
  }
  std::string getStats(std::string &fName);
  void getStats();
  double computeAMAT() const;

};

std::ostream &operator<<(std::ostream &out, const simCache &cache);

class randomReplacementCache : public simCache {
public:
  randomReplacementCache(size_t bytes_per_line, size_t assoc, size_t num_sets, 
			 std::string name, int latency, simCache *next_level);
  ~randomReplacementCache();
  bool access(uint32_t addr, uint32_t num_bytes, opType o, uint32_t &lat) override;
  
private:
  /* all bits are valid */
  uint8_t *allvalid;
  /* cache valid bits */
  uint8_t **valid;
  /* cache tag bits */
  uint32_t **tag;
};

class directMappedCache : public simCache {
private:
  std::vector<uint32_t> tags;
  boost::dynamic_bitset<> valid;
public:
  directMappedCache(size_t bytes_per_line, size_t assoc, size_t num_sets,
		    std::string name, int latency, simCache *next_level) : 
    simCache(bytes_per_line, assoc, num_sets, name, latency, next_level) {
    assert(assoc == 1);
    tags.resize(num_sets);
    valid.resize(num_sets, false);
  }
  ~directMappedCache();
  bool access(uint32_t addr, uint32_t num_bytes, opType o, uint32_t &lat) override;
};

class fullAssocCache: public simCache {
 private:
  mylist<uint32_t> entries;
  std::vector<uint64_t> hitdepth;
public:
  fullAssocCache(size_t bytes_per_line, size_t assoc, size_t num_sets, 
		std::string name, int latency, simCache *next_level) :
    simCache(bytes_per_line, assoc, num_sets, name, latency, next_level) {
    hitdepth.resize(assoc);
    std::fill(hitdepth.begin(), hitdepth.end(), 0);
  }
  ~fullAssocCache();
  bool access(uint32_t addr, uint32_t num_bytes, opType o, uint32_t &lat) override;
};

class lowAssocCache : public simCache {
 public:
  lowAssocCache(size_t bytes_per_line, size_t assoc, size_t num_sets, 
		 std::string name, int latency, simCache *next_level);
  ~lowAssocCache();
  bool access(uint32_t addr, uint32_t num_bytes, opType o, uint32_t &lat) override;

 private:
  void updateLRU(uint32_t idx,uint32_t w);
  int32_t findLRU(uint32_t w);
  
  uint8_t *allvalid;

  /* cache valid bits */
   uint64_t *valid;
   
  /* tree-lru bits */
   uint64_t *lru;
  
   /* cache tag bits */
   uint32_t **tag;

};

class setAssocCache: public simCache {
 private:
  class cacheset {
  private:
    int32_t id;
    size_t assoc;
    std::array<size_t,2> &rw_hits;
    std::array<size_t,2> &rw_misses;
    size_t &hits;
    size_t &misses;
    size_t lhits,lmisses;
    mylist<uint32_t> entries;
  public:
    cacheset(int32_t id, size_t assoc,
	     size_t &hits, size_t &misses,
	     std::array<size_t, 2> &rw_hits,
	     std::array<size_t, 2> &rw_misses) :
      id(id), assoc(assoc), hits(hits), misses(misses),
      rw_hits(rw_hits), rw_misses(rw_misses),
      lhits(0), lmisses(0) {
    }
    bool access(uint32_t tag, opType o) {
      bool h = false;
      auto it = entries.find(tag);
      if(it != entries.end()) {
	rw_hits[(opType::WRITE==o) ? 1 : 0]++;
	hits++;
	lhits++;
	entries.move_to_head(it);
	h = true;
      }
      else {
	rw_misses[(opType::WRITE==o) ? 1 : 0]++;
	misses++;
	lmisses++;
    	if(entries.size() == assoc) {
	  entries.pop_back();
	}
	entries.push_front(tag);
      }
      return h;
    }
    size_t size() const {
      return entries.size();
    }
  };
  cacheset **sets;
  
public:
  setAssocCache(size_t bytes_per_line, size_t assoc, size_t num_sets, 
		std::string name, int latency, simCache *next_level);
  ~setAssocCache();
  bool access(uint32_t addr, uint32_t num_bytes, opType o, uint32_t &lat) override;
};


class fullRandAssocCache: public simCache {
 private:
  std::unordered_set<uint32_t> entries;
  std::vector<uint32_t> tags;
public:
  fullRandAssocCache(size_t bytes_per_line, size_t assoc, size_t num_sets, 
		std::string name, int latency, simCache *next_level) :
    simCache(bytes_per_line, assoc, num_sets, name, latency, next_level) {
  }
  ~fullRandAssocCache() {}
  bool access(uint32_t addr, uint32_t num_bytes, opType o, uint32_t &lat) override;
};


class realLRUCache : public simCache {
 public:
  realLRUCache(size_t bytes_per_line, size_t assoc, size_t num_sets, 
		 std::string name, int latency, simCache *next_level);
  ~realLRUCache();
  bool access(uint32_t addr, uint32_t num_bytes, opType o, uint32_t &lat) override;
private:
  /* all bits are valid */
  uint8_t *allvalid;
  /* cache valid bits */
  uint8_t **valid;
  /* tree-lru bits */
   uint64_t **lru;
  /* cache tag bits */
  uint32_t **tag;
};

/* This is too expensive to be used in practice */
class highAssocCache : public simCache {
 public:
  highAssocCache(size_t bytes_per_line, size_t assoc, size_t num_sets, 
		 std::string name, int latency, simCache *next_level);
  ~highAssocCache();
  bool access(uint32_t addr, uint32_t num_bytes, opType o, uint32_t &lat) override;

 private:
  void updateLRU(uint32_t idx,uint32_t w);
  int32_t findLRU(uint32_t w);
  
  uint8_t *allvalid;
  /* cache valid bits */
   uint8_t **valid;
   
  /* tree-lru bits */
   uint8_t **lru;
  
   /* cache tag bits */
   uint32_t **tag;

};

#endif



