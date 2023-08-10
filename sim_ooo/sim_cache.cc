#include <cstdio>
#include <algorithm>
#include "sim_cache.hh"
#include "sim_parameters.hh"
#include "helper.hh"
#include "mips_op.hh"

uint64_t get_curr_cycle();

std::ostream &operator<<(std::ostream &out, const simCache &cache) {
  double total = static_cast<double>(cache.hits+cache.misses);
  double rate = cache.misses / total;
  
  out << cache.name << ":\n";
  out << "total_cache_size = " << cache.total_cache_size << "\n";
  out << "bytes_per_line = " << cache.bytes_per_line << "\n";
  out << "assoc = " << cache.assoc << "\n";
  out << "num_sets = " << cache.num_sets<< "\n";
  out << "miss_rate = " << rate << "\n";
  out << "total_accesses = " << (cache.hits+cache.misses) << "\n";
  out << "hits = " << cache.hits << "\n";
  out << "misses = " << cache.misses << "\n";
  out << "read_hits = "<< cache.rw_hits[0] << "\n";
  out << "read_misses = "<< cache.rw_misses[0] << "\n";
  out << "write_hits = "<< cache.rw_hits[1] << "\n";
  out << "write_misses = "<< cache.rw_misses[1] << "\n";
  out << "amat = " << cache.computeAMAT() << "\n";
  if(cache.next_level)
    out << *(cache.next_level);
  
  return out;
}

simCache::simCache(size_t bytes_per_line, size_t assoc, size_t num_sets, 
		   std::string name, int latency, simCache *next_level) :
  bytes_per_line(bytes_per_line), assoc(assoc), num_sets(num_sets),
  name(name), latency(latency), next_level(next_level),
  hits(0), misses(0) {
  
  rw_hits.fill(0);
  rw_misses.fill(0);
  
  if(!(isPow2(bytes_per_line) and isPow2(num_sets) and isPow2(assoc))) {
    die();
  }
  
  total_cache_size = num_sets * bytes_per_line * assoc;
  ln2_num_sets = 0;
  ln2_bytes_per_line = 0;
  while((1<<ln2_bytes_per_line) < bytes_per_line) {
    ln2_bytes_per_line++;
  }
  while((1<<ln2_num_sets) < num_sets) {
    ln2_num_sets++;
  }
  
  ln2_offset_bits = ln2_num_sets + ln2_bytes_per_line;
  ln2_tag_bits = 8*sizeof(uint32_t) - ln2_offset_bits;
  inflight.resize(sim_param::rob_size);
  missq.resize(sim_param::rob_size);
}

simCache::~simCache() {}

void highAssocCache::flush() {
  for(size_t i =0; i < num_sets; i++) {
    allvalid[i] = 0;
    memset(valid[i],0,sizeof(uint8_t)*assoc);
  }
  if(next_level) {
    next_level->flush();
  }
}

void highAssocCache::flush_line(uint32_t addr) {

  if(next_level) {
    next_level->flush_line(addr);
  }
}


highAssocCache::highAssocCache(size_t bytes_per_line, size_t assoc, size_t num_sets, 
			       std::string name, int latency, simCache *next_level) :
  simCache(bytes_per_line, assoc, num_sets, name, latency, next_level) {
  valid = new uint8_t*[num_sets];
  lru = new uint8_t*[num_sets];
  tag = new uint32_t*[num_sets];
  allvalid = new uint8_t[num_sets];
  for(size_t i =0; i < num_sets; i++) {
    valid[i] = new uint8_t[assoc];
    tag[i] = new uint32_t[assoc];
    lru[i] = new uint8_t[(assoc/2)+2];
    allvalid[i] = 0;
    memset(valid[i],0,sizeof(uint8_t)*assoc);
    memset(tag[i],0,sizeof(uint32_t)*assoc);
    memset(lru[i],0,sizeof(uint8_t)*((assoc/2)+2));
  }
}

highAssocCache::~highAssocCache() {
  for(size_t i =0; i < num_sets; i++) {
    delete [] valid[i];
    delete [] tag[i];
    delete [] lru[i];
  }
  delete [] lru;
  delete [] tag;
  delete [] valid;
  delete [] allvalid;
}


void lowAssocCache::flush() {
  for(size_t i =0; i < num_sets; i++) {
    allvalid[i] = 0;
    valid[i] = 0;
  }
  if(next_level) {
    next_level->flush();
  }
}

void lowAssocCache::flush_line(uint32_t addr) {

  if(next_level) {
    next_level->flush_line(addr);
  }
}


lowAssocCache::lowAssocCache(size_t bytes_per_line, size_t assoc, size_t num_sets, 
			     std::string name, int latency, simCache *next_level) :
  simCache(bytes_per_line, assoc, num_sets, name, latency, next_level){
  valid = new uint64_t[num_sets];
  lru = new uint64_t[num_sets];
  tag = new uint32_t*[num_sets];
  allvalid = new uint8_t[num_sets];
  for(size_t i =0; i < num_sets; i++){
    allvalid[i] = 0;
    tag[i] = new uint32_t[assoc];
    lru[i] = 0;
    valid[i] = 0;
    memset(tag[i],0,sizeof(uint32_t)*assoc);
  }
}

lowAssocCache::~lowAssocCache() {
  for(size_t i =0; i < num_sets; i++)
    delete [] tag[i];
  
  delete [] allvalid;
  delete [] lru;
  delete [] tag;
  delete [] valid;
}

void simCache::set_next_level(simCache *next_level) {
  this->next_level = next_level;
}

uint32_t simCache::index(uint32_t addr, uint32_t &l, uint32_t &t) {
  //shift address by ln2_bytes_per_line
  uint32_t way_addr = addr >> ln2_bytes_per_line;
  //mask by the number of sets 
  l = way_addr & (num_sets-1);
  t = addr >> ln2_offset_bits;
  
  uint32_t byte_offs = addr & (bytes_per_line-1);

  //uint32_t nA = (t << ln2_offset_bits) | (l << ln2_bytes_per_line) | byteOffset;
  //printf("addr=%u, nA = %u\n", addr, nA);
  return byte_offs;
}

void highAssocCache::updateLRU(uint32_t idx, uint32_t w)
{
  int32_t last_idx = (idx+assoc);
  int32_t lru_idx = last_idx/2;
  while(lru_idx > 0) {
    lru[w][lru_idx] = (last_idx & 0x1) ? 2 : 1;
    last_idx = lru_idx;
    lru_idx = lru_idx / 2;
  }
}

int32_t highAssocCache::findLRU(uint32_t w) {
  int32_t lru_idx = 1;
  while(true) {
    if(lru_idx >= assoc) {
      int offs = (lru_idx - assoc);
      if(offs >= assoc) {
	abort();
      }
      return offs;
    }
    
    switch(lru[w][lru_idx])
      {
      case 0:
	/* invalid line, default go left */
	lru[w][lru_idx] = 1;
	lru_idx = 2*lru_idx + 0;
	break;
      case 1:
	/* last went left */
	lru[w][lru_idx] = 2;
	lru_idx = 2*lru_idx + 1;
	break;
      case 2:
	/* last went right */
	lru[w][lru_idx] = 1;
	lru_idx = 2*lru_idx + 0;
	break;
      default:
	printf("TREE LRU broken!\n");
	abort();
	break;
      }
  }
  return -1;
}

directMappedCache::~directMappedCache(){}

bool directMappedCache::access(uint32_t addr, uint32_t num_bytes, opType o, uint32_t &lat) {
  return true;
  uint32_t w,t;
  uint32_t b = index(addr, w, t);
  bool h = false, dirty_miss = false;
  lat = 1;
  if(tags[w]==t && valid[w]) {
    hits++;
    if(opType::WRITE==o) {
      dirty[w] = true;
    }
    rw_hits[(opType::WRITE==o) ? 1 : 0]++;
    h = true;
  }
  else {
    misses++;
    rw_misses[(opType::WRITE==o) ? 1 : 0]++;
    dirty_miss = dirty[w] and valid[w];
    //read miss, bring in line clean. otherwise, dirty
    dirty[w] = (o==opType::WRITE);
    valid[w] = true;    
    tags[w] = t;
  }
  
  if(not(h)) {
    lat += (dirty_miss ? (2*sim_param::mem_latency+2) : (sim_param::mem_latency+1));
  }
  
  return h;
}

void directMappedCache::flush() {
  valid.reset();
  if(next_level) {
    next_level->flush();
  }
}

void directMappedCache::flush_line(uint32_t addr) {
  if(next_level) {
    next_level->flush_line(addr);
  }
}

fullAssocCache::~fullAssocCache() {}


bool fullAssocCache::access(uint32_t addr, uint32_t num_bytes, opType o, uint32_t &lat) {
  uint32_t w,t;
  uint32_t b = index(addr, w, t);
  bool h = false;
  auto it = entries.find(t);
  if(it != entries.end()) {
    hits++;
    auto d = entries.distance(it);
    hitdepth[d]++;
    rw_hits[(opType::WRITE==o) ? 1 : 0]++;
    entries.move_to_head(it);
    h = true;
  }
  else {
    misses++;
    rw_misses[(opType::WRITE==o) ? 1 : 0]++;
    if(entries.size() == assoc) {
      entries.pop_back();
    }
    entries.push_front(t);
  }
  return h;
}

void fullAssocCache::flush() {
  entries.clear();
  std::fill(hitdepth.begin(),hitdepth.end(),0);
  if(next_level) {
    next_level->flush();
  }
}

void fullAssocCache::flush_line(uint32_t addr) {
  uint32_t w,t;
  uint32_t b = index(addr, w, t);
  auto it = entries.find(t);
  if(it != entries.end()) {
    entries.erase(it);
  }
  if(next_level) {
    next_level->flush_line(addr);
  }
}

setAssocCache::setAssocCache(size_t bytes_per_line, size_t assoc, size_t num_sets, 
			     std::string name, int latency, simCache *next_level) :
  simCache(bytes_per_line, assoc, num_sets, name, latency, next_level) {
  sets = new cacheset*[num_sets];
  for(size_t i = 0; i < num_sets; ++i) {
    sets[i] = new cacheset(i,assoc,hits,misses,rw_hits,rw_misses);
  }
}
setAssocCache::~setAssocCache() {
  for(size_t i = 0; i < num_sets; ++i) {
    delete sets[i];
  }
  delete [] sets;
}

void setAssocCache::flush() {
  for(size_t i = 0; i < num_sets; ++i) {
    sets[i]->clear();
  }
  if(next_level) {
    next_level->flush();
  }
}

void setAssocCache::flush_line(uint32_t addr) {
  uint32_t w,t;
  uint32_t b = index(addr, w, t);
#if 0
  std::cerr << "flush 0x"
	    << std::hex
	    << addr
	    << " maps to way "
	    << w
	    << " with tag "
	    << t
	    << "\n";
#endif
  sets[w]->flush_line(t);
  if(next_level) {
    next_level->flush_line(addr);
  }
}

bool setAssocCache::access(uint32_t addr, uint32_t num_bytes, opType o, uint32_t &lat) {
  uint32_t w,t;
  lat += latency;
  uint32_t b = index(addr, w, t);
  bool hit = sets[w]->access(t,o);
#if 0
  std::cerr << "access 0x"
	    << std::hex
	    << addr
	    << " maps to way "
	    << w
	    << " with tag "
	    << t
	    << ", hit = "
	    << hit
	    << "\n";
#endif
  if(not(hit)) {
    if(next_level) {
      size_t reload_addr = addr & (~(bytes_per_line-1));
      next_level->access(reload_addr, bytes_per_line, o, lat);
    }
    else {
      lat += sim_param::mem_latency;
    }
  }
  return hit;
  
}

void fullRandAssocCache::flush() {
  entries.clear();
  tags.clear();
  if(next_level) {
    next_level->flush();
  }
}

void fullRandAssocCache::flush_line(uint32_t addr) {
  if(next_level) {
    next_level->flush_line(addr);
  }
}

bool fullRandAssocCache::access(uint32_t addr, uint32_t num_bytes, opType o, uint32_t &lat) {
  uint32_t w,t;
  uint32_t b = index(addr, w, t);
  bool h  = false;
  auto it = std::find(entries.begin(), entries.end(), t);
  if(it != entries.end()) {
    hits++;
    auto d = std::distance(entries.begin(),it);
    rw_hits[(opType::WRITE==o) ? 1 : 0]++;
    h = true;
  }
  else {
    if(entries.size() != assoc) {
      entries.insert(t);
      tags.push_back(t);
    }
    else {
      size_t pos = rand() % assoc;
      auto pit = entries.find(tags[pos]);
      entries.erase(pit);
      tags[pos] = t;
      entries.insert(t);
    }
    misses++;
    rw_misses[(opType::WRITE==o) ? 1 : 0]++;
  }
  return h;
}


bool highAssocCache::access(uint32_t addr, uint32_t num_bytes, opType o, uint32_t &lat) {
  /* way and tag of current address */
  uint32_t w,t;
  uint32_t a = assoc+1;
  uint32_t b = index(addr, w, t);
  bool h = false;
  lat += latency;
  /* search cache ways */
  if((allvalid[w]))
    {
      for(size_t i = 0; i < assoc; i++)
	{
	  if(tag[w][i]==t)
	    {
	      a = i;
	      h = true;
	      break;
	    }
	}
    }
  else
    {
      for(size_t i = 0; i < assoc; i++)
	{
	  if(valid[w][i] && (tag[w][i]==t))
	    {
	      a = i;
	      h = true;
	      break;
	    }
	}
    }
  
  /* cache miss .. handle it */
  if( a == (assoc+1)) {
    if(next_level) {
      /* mask off to align */
      size_t reload_addr = addr & (~(bytes_per_line-1));
      next_level->access(reload_addr, bytes_per_line, o, lat);
    }
    
    misses++;
    rw_misses[(opType::WRITE==o) ? 1 : 0]++;
    
    if(allvalid[w])  {
      int32_t offs = findLRU(w);
      valid[w][offs] = 1;
      tag[w][offs] = t;
    } 
    else {
      size_t offs = 0;
      for(size_t i = 0; i < assoc; i++) {
	if(valid[w][i]==0) {
	  offs = i;
	  break;
	}
      }
      valid[w][offs] = 1;
      tag[w][offs] = t;
      updateLRU((uint32_t)offs,w);
      uint8_t allV = 1;
      for(size_t i = 0; i < assoc; i++) {
	allV &= valid[w][i];
      }
      allvalid[w] = allV;
    }
  }
  else {
    hits++;
    rw_hits[(opType::WRITE==o) ? 1 : 0]++;
    updateLRU(a,w);
  }
  return h;
}

void lowAssocCache::updateLRU(uint32_t idx, uint32_t w) {
  int32_t last_idx = (idx+assoc);
  int32_t lru_idx = last_idx/2;
  uint64_t t = lru[w];
  while(lru_idx > 0) {
    //printf("lru_idx = %d\n", lru_idx);
    uint64_t m = 1UL << lru_idx;
    
      if(last_idx & 0x1) {
	/* clear bit */
	t  &= ~m; 
      } else {
	/* set bit */
	t |= m;
      }
      last_idx = lru_idx;
      lru_idx = lru_idx / 2;
    }
  lru[w] = t;
}

int32_t lowAssocCache::findLRU(uint32_t w) {
  int32_t lru_idx = 1;
  uint64_t t = lru[w];
  while(true)
    {
      if(lru_idx >= assoc)
	{
	  int offs = (lru_idx - assoc);
	  if(offs >= assoc) {
	    abort();
	  }
	  lru[w] = t;
	  return offs;
	}

      uint64_t m = 1UL << lru_idx;
      if( ((t >> lru_idx) & 0x1) ) 
	{
	  t &= ~m; 
	  /* set bit .. now clear it*/
	  lru_idx = 2*lru_idx + 0;
	} 
      else 
	{
	  t |= m;
	  /* unset bit .. now set it*/
	  lru_idx = 2*lru_idx + 1;
	}
    }
  return -1;
}

bool lowAssocCache::access(uint32_t addr, uint32_t num_bytes, opType o, uint32_t &lat){
  /* way and tag of current address */
  uint32_t w,t;
  uint32_t a = assoc+1;
  uint32_t b = index(addr, w, t);
  bool h =  false;
  lat += latency;
  if((allvalid[w])) {
    for(size_t i = 0; i < assoc; i++) {
      if(tag[w][i]==t) {
	a = i;
	h = true;
	break;
      }
    }
  }
  else
    {
      uint64_t v = valid[w];
      while(v != 0) {
	uint32_t i = __builtin_ffsl(v)-1;
	if(tag[w][i] == t) {
	  a = i;
	  h = true;
	  break;
	}
	v &= ( ~(1UL << i) );
      }
    }
  
  /* cache miss .. handle it */
  if( a == (assoc+1))
    {
      if(next_level)
	{
	  /* mask off to align */
	  size_t reload_addr = addr & (~(bytes_per_line-1));
	  next_level->access(reload_addr, bytes_per_line, o, lat);
	}
      
      misses++;
      rw_misses[(opType::WRITE==o) ? 1 : 0]++;

      int32_t offs = -1;
      if((allvalid[w])) {
	offs = findLRU(w);
      }
      else {
	uint64_t nv = ~valid[w];
	offs = __builtin_ffsl(nv)-1;
	updateLRU(offs,w);
	valid[w] |= 1UL << offs;
	allvalid[w] = (__builtin_popcountl(valid[w]) == assoc);
      }
      tag[w][offs] = t;
    }
  else {
    hits++;
    rw_hits[(opType::WRITE==o) ? 1 : 0]++;
    updateLRU(a,w);
  }
  return h;
}

void simCache::tick() {
  if(next_level) {
    next_level->tick();
  }
  
  for(auto it = inflight.begin(); it != inflight.end(); /* nil */) {
    sim_op o = *it;
     if(o->aux_cycle == get_curr_cycle()) {
      size_t was = inflight.size();
      o->complete_cycle = o->aux_cycle + 2;
      it = inflight.erase(it);
    }
    else {
      it++;
    }
  }
}

void simCache::nuke_inflight() {
  if(next_level) {
    next_level->nuke_inflight();
  }
  inflight.clear();
}

uint32_t simCache::read(sim_op op, uint32_t addr, uint32_t num_bytes) {
  uint32_t lat = 0;
  bool hit = access(addr,num_bytes,opType::READ,lat);
  //RTL cache pipeline:
  //cycle 1: read cache 
  //cycle 2: check if hit,
  //         if true, return data
  //         else check if dirty
  //              if dirty, generate writeback
  //              else immediatly reload
  //
  //if(not(hit)) {
  //assert(op);
  //std::cerr << "read: " << std::hex << op->pc << std::dec << " missed\n";
  //}
  //std::cout << "lat = " << lat << "\n";
  op->aux_cycle = lat + get_curr_cycle();
  assert(not(inflight.full()));
  inflight.push(op);
  return lat;
}

uint32_t simCache::write(sim_op op, uint32_t addr, uint32_t num_bytes) {
  uint32_t lat = 0;
  bool hit = access(addr,num_bytes,opType::WRITE,lat);
  if(not(hit) and false) {
    assert(op);
    std::cerr << "write: "
	      << std::hex
	      << op->pc
	      << std::dec
	      << " missed, push out "
	      << lat
	      << "\n";
  }
  lat = 1;
  op->aux_cycle = lat + get_curr_cycle();
  assert(not(inflight.full()));
  inflight.push(op);
  return lat;
}

void simCache::getStats() {
  std::string s;
  size_t total = hits+misses;
  double rate = ((double)misses) / ((double)total);
  s = name + ":\n";
  s += "total_cache_size = " + toString(total_cache_size) + "\n";
  s += "miss_rate = " + toString(rate) + "\n";
  s += "total_access = " + toString(total)  + "\n";
  s += "hits = " + toString(hits) + "\n";
  s += "misses = " + toString(misses) + "\n";
  s += "read_hits = "+  toString(rw_hits[0]) + "\n";
  s += "read_misses = "+  toString(rw_misses[0]) + "\n";
  s += "write_hits = "+  toString(rw_hits[1]) + "\n";
  s += "write_misses = "+  toString(rw_misses[1]) + "\n";
  std::cout << s << std::endl;
  if(next_level)
    next_level->getStats();
}

std::string simCache::getStats(std::string &fName)
{
  std::string s;
  size_t total = hits+misses;
  double rate = ((double)misses) / ((double)total);
  
  fName = name + ".txt";
  s = name + ":\n";
  s += "total_cache_size = " + toString(total_cache_size) + "\n";
  s += "miss_rate = " + toString(rate) + "\n";
  s += "total_access = " + toString(total)  + "\n";
  s += "hits = " + toString(hits) + "\n";
  s += "misses = " + toString(misses) + "\n";
  
  s += "read_hits = "+  toString(rw_hits[0]) + "\n";
  s += "read_misses = "+  toString(rw_misses[0]) + "\n";
  s += "write_hits = "+  toString(rw_hits[1]) + "\n";
  s += "write_misses = "+  toString(rw_misses[1]) + "\n";
  
  return s;
}


double simCache::computeAMAT() const {
  size_t total = hits+misses;
  double rate = ((double)misses) / ((double)total);
  double nextLevelLat = sim_param::mem_latency;

  if(next_level) {
    nextLevelLat = next_level->computeAMAT();
  }
  
  double x = (nextLevelLat * rate)  + 
    ((double)latency * (1.0 - rate));
  
  /* printf("AMAT = %g\n", x); */
  
  return x;
}


void realLRUCache::flush() {
  for(size_t i = 0; i < num_sets; i++) {
    allvalid[i] = 0;
    memset(valid[i],0,sizeof(uint8_t)*assoc);
  }
  if(next_level) {
    next_level->flush();
  }
}

void realLRUCache::flush_line(uint32_t addr) {
  if(next_level) {
    next_level->flush_line(addr);
  }
}

realLRUCache::realLRUCache(size_t bytes_per_line, size_t assoc, size_t num_sets, 
			       std::string name, int latency, simCache *next_level) :
  simCache(bytes_per_line, assoc, num_sets, name, latency, next_level) {
  valid = new uint8_t*[num_sets];
  lru = new uint64_t*[num_sets];
  tag = new uint32_t*[num_sets];
  allvalid = new uint8_t[num_sets];

  for(size_t i =0; i < num_sets; i++) {
      valid[i] = new uint8_t[assoc];
      tag[i] = new uint32_t[assoc];
      lru[i] = new uint64_t[assoc];
      allvalid[i] = 0;
      memset(valid[i],0,sizeof(uint8_t)*assoc);
      memset(tag[i],0,sizeof(uint32_t)*assoc);
      memset(lru[i],0,sizeof(uint64_t)*assoc);
    }
}

realLRUCache::~realLRUCache() {
  for(size_t i =0; i < num_sets; i++) {
    delete [] valid[i];
    delete [] tag[i];
    delete [] lru[i];
  }
  delete [] lru;
  delete [] tag;
  delete [] valid;
  delete [] allvalid;
}

randomReplacementCache::randomReplacementCache(size_t bytes_per_line, 
					       size_t assoc, size_t num_sets, 
					       std::string name, int latency, 
					       simCache *next_level) :
  simCache(bytes_per_line, assoc, num_sets, name, latency, next_level) {
  valid = new uint8_t*[num_sets];
  tag = new uint32_t*[num_sets];
  allvalid = new uint8_t[num_sets];

  for(size_t i =0; i < num_sets; i++) {
    valid[i] = new uint8_t[assoc];
    tag[i] = new uint32_t[assoc];
    allvalid[i] = 0;
    memset(valid[i],0,sizeof(uint8_t)*assoc);
    memset(tag[i],0,sizeof(uint32_t)*assoc);
  }
}

randomReplacementCache::~randomReplacementCache() {
  for(size_t i =0; i < num_sets; i++) {
    delete [] valid[i];
    delete [] tag[i];
  }
  delete [] tag;
  delete [] valid;
  delete [] allvalid;
}

bool randomReplacementCache::access(uint32_t addr, uint32_t num_bytes, opType o, uint32_t &lat){
  /* way and tag of current address */
  uint32_t w,t;
  uint32_t a = assoc+1;
  uint32_t b = index(addr, w, t);
  bool h = false;
  lat += latency;
  /* search cache ways */
  for(size_t i = 0; i < assoc; i++) {
    if(valid[w][i] && (tag[w][i]==t)) {
      a = i;
      h = true;
      break;
    }
  }
  
  /* cache miss .. handle it */
  if( a == (assoc+1))
    {
      if(next_level) {
	/* mask off to align */
	size_t reload_addr = addr & (~(bytes_per_line-1));
	next_level->access(reload_addr, bytes_per_line, o, lat);
      }
      
      misses++;
      rw_misses[(o==opType::WRITE) ? 1 : 0]++;
      
      if(allvalid[w]) 
	{
	  uint32_t offs = rand() % assoc;
	  valid[w][offs] = 1;
	  tag[w][offs] = t;
	} 
      else
	{
	  size_t offs = 0;
	  for(size_t i = 0; i < assoc; i++)
	    {
	      if(valid[w][i]==0) {
		offs = i;
		break;
	      }
	    }
	  valid[w][offs] = 1;
	  tag[w][offs] = t;
	  uint8_t allV = 1;
	  for(size_t i = 0; i < assoc; i++)
	    {
	      allV &= valid[w][i];
	    }
	  allvalid[w] = allV;
	}
    }
  else {
    hits++;
    rw_hits[(opType::WRITE==o) ? 1 : 0]++;
  }
  return h;
}



bool realLRUCache::access(uint32_t addr, uint32_t num_bytes, opType o, uint32_t &lat) {
  /* way and tag of current address */
  uint32_t w,t;
  uint32_t a = assoc+1;
  uint32_t b = index(addr, w, t);
  bool h = false;
  lat += latency;
  /* search cache ways */
  for(size_t i = 0; i < assoc; i++)
    {
      if(valid[w][i] && (tag[w][i]==t))
	{
	  a = i;
	  h = true;
	  break;
	}
    }
  
  /* cache miss .. handle it */
  if( a == (assoc+1))
    {
      if(next_level)
	{
	  /* mask off to align */
	  size_t reload_addr = addr & (~(bytes_per_line-1));
	  next_level->access(reload_addr, bytes_per_line, o, lat);
	}
      
      misses++;
      rw_misses[(o==opType::WRITE) ? 1 : 0]++;
      
      if(allvalid[w]) 
	{
	  uint32_t offs = 0;
	  int64_t now = get_curr_cycle();
	  int64_t maxDelta = 0;
	  for(uint32_t i = 0; i < assoc; i++)
	    {
	      int64_t d = (now - lru[w][i]);
	      if(d > maxDelta) {
		offs = i;
		maxDelta = d;
	      }
	    }
	  valid[w][offs] = 1;
	  tag[w][offs] = t;
	} 
      else
	{
	  size_t offs = 0;
	  for(size_t i = 0; i < assoc; i++)
	    {
	      if(valid[w][i]==0) {
		offs = i;
		break;
	      }
	    }
	  valid[w][offs] = 1;
	  tag[w][offs] = t;
	  lru[w][offs] = get_curr_cycle();
	  uint8_t allV = 1;
	  for(size_t i = 0; i < assoc; i++)
	    {
	      allV &= valid[w][i];
	    }
	  allvalid[w] = allV;
	}
    }
  else
    {
      hits++;
      rw_hits[(o==opType::WRITE)?1:0]++;
      lru[w][a] = get_curr_cycle();
    }
  return h;
}
