#ifndef BUILTINS_H
#define BUILTINS_H

#include <stdint.h>

static inline int popcount(const uint64_t bb) {

  return __builtin_popcountll(bb);

}

static inline int bsf(const uint64_t bb) {

  return __builtin_ctzll(bb);

}

static inline int msb(const uint64_t bb) {
  // 63 minus the number of leading zeros gives us the highest set bit
  return 63 - __builtin_clzll(bb); 
}

#endif
