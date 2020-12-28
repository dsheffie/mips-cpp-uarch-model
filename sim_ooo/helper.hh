#ifndef __HELPERFUNCTS__
#define __HELPERFUNCTS__
#include <string>
#include <sstream>
#include <vector>
#include <cstdint>
#include <iostream>
#include <fstream>
#include <type_traits>

#include <cstdio>
#include <cstdarg>
#include <ostream>
#include <map>

static const char KNRM[] = "\x1B[0m";
static const char KRED[] = "\x1B[31m";
static const char KGRN[] = "\x1B[32m";
static const char KYEL[] = "\x1B[33m";
static const char KBLU[] = "\x1B[34m";
static const char KMAG[] = "\x1B[35m";
static const char KCYN[] = "\x1B[36m";
static const char KWHT[] = "\x1B[37m";

/* from gdb simulator */
// static const uint32_t RSVD_INSTRUCTION  = 0x00000005;
// static const uint32_t RSVD_INSTRUCTION_MASK = 0xFC00003F;
// static const uint32_t RSVD_INSTRUCTION_ARG_SHIFT = 6;
// static const uint32_t RSVD_INSTRUCTION_ARG_MASK = 0xFFFFF;
// static const uint32_t IDT_MONITOR_BASE = 0xBFC00000;
// static const uint32_t IDT_MONITOR_SIZE = 2048;
// static const uint32_t K1SIZE = 0x80000000;

void dbt_backtrace();

#define terminate_prog() {						\
    dbt_backtrace();							\
    abort();								\
  }

#define die() {								\
    std::cerr << __PRETTY_FUNCTION__ << " @ " << __FILE__ << ":"	\
	      << __LINE__ << " called die\n";				\
    terminate_prog();							\
  }

#define unimplemented() {						\
    std::cerr << __PRETTY_FUNCTION__ << " @ " << __FILE__ << ":"	\
	      << __LINE__ << " is unimplemented, terminating\n";	\
    terminate_prog();							\
  }

#undef terminate

double timestamp();

uint32_t update_crc(uint32_t crc, uint8_t *buf, size_t len);
uint32_t crc32(uint8_t *buf, size_t len);

int32_t remapIOFlags(int32_t flags);

template <typename X, typename Y>
void mapToCSV(const std::map<X,Y> &m, const std::string &fname) {
  std::ofstream out(fname);
  for(auto & p : m) {
    out << p.first << "," << p.second << "\n";
  }
}

template <class T>
std::string toString(T x) {
  std::stringstream ss;
  ss << x;
  return ss.str();
}

template <class T>
std::string toStringHex(T x) {
  std::stringstream ss;
  ss << std::hex << x;
  return ss.str();
}

template <typename T, typename std::enable_if<std::is_integral<T>::value, T>::type* = nullptr>
bool extractBit(T x, uint32_t b) {
  return (x >> b) & 0x1;
}

template <typename T, typename std::enable_if<std::is_integral<T>::value, T>::type* = nullptr>
T setBit(T x, bool v, uint32_t b) {
  T vv = v ? static_cast<T>(1) : static_cast<T>(0);
  T t = static_cast<T>(1) << b;
  T tt = (~t) & x;
  t  &= ~(vv-1);
  return (tt | t);
}


#define BS_PRED(SZ) (std::is_integral<T>::value && (sizeof(T)==SZ))
template <typename T, typename std::enable_if<BS_PRED(1),T>::type* = nullptr>
T bswap(T x) {
  return x;
}
template <typename T, typename std::enable_if<BS_PRED(2),T>::type* = nullptr>
T bswap(T x) {
  return __builtin_bswap16(x);
}
template <typename T, typename std::enable_if<BS_PRED(4),T>::type* = nullptr>
T bswap(T x) {
  return __builtin_bswap32(x);
}
template <typename T, typename std::enable_if<BS_PRED(8),T>::type* = nullptr>
T bswap(T x) {
  return __builtin_bswap64(x);
}


#define INTEGRAL_ENABLE_IF(SZ,T) typename std::enable_if<std::is_integral<T>::value and (sizeof(T)==SZ),T>::type* = nullptr

template <bool EL, typename T, INTEGRAL_ENABLE_IF(1,T)>
T bswap(T x) {
  return x;
}

template <bool EL, typename T, INTEGRAL_ENABLE_IF(2,T)> 
T bswap(T x) {
  static_assert(__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__, "must be little endian machine");
  if(EL) 
    return x;
  else
  return  __builtin_bswap16(x);
}

template <bool EL, typename T, INTEGRAL_ENABLE_IF(4,T)>
T bswap(T x) {
  static_assert(__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__, "must be little endian machine");
  if(EL)
    return x;
  else 
    return  __builtin_bswap32(x);
}

template <bool EL, typename T, INTEGRAL_ENABLE_IF(8,T)> 
T bswap(T x) {
  static_assert(__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__, "must be little endian machine");
  if(EL)
    return x;
  else 
    return  __builtin_bswap64(x);
}

#undef INTEGRAL_ENABLE_IF

#ifndef UNREACHABLE
#define UNREACHABLE() {				\
    __builtin_unreachable();			\
  }
#endif



template <typename T, typename std::enable_if<std::is_integral<T>::value,T>::type* = nullptr>
bool isPow2(T x) {
  return (((x-1)&x) == 0);
}

template <typename T, typename std::enable_if<std::is_integral<T>::value,T>::type* = nullptr>
T mod(T x, T y) {
  if(isPow2(y)) {
    return x & (y-1);
  }
  return x % y;
}

template <typename T, typename std::enable_if<std::is_integral<T>::value,T>::type* = nullptr>
T ln2(T x) {
  T y = 1, l =0;
  while(y < x) {
    l++;
    y *= 2;
  }
  return l;
}

#define print_var(x) {					\
    std::cerr << #x << " = " << x  << std::endl;	\
  }

#define print_var_hex(x) {		      \
    std::cerr << #x << " = " << std::hex << x \
	      << std::dec << std::endl;	      \
  }

#ifdef  __FreeBSD__
/* cribbed straight out of the FreeBSD source */
inline int dprintf(int fd, const char * __restrict fmt, ...) {
  va_list ap;
  int ret;
  va_start(ap, fmt);
  ret = vdprintf(fd, fmt, ap);
  va_end(ap);
  return (ret);
}
#endif


#endif
