#ifndef BUILTINS_H
#define BUILTINS_H

#include <stdint.h>

static inline int popcount(const uint64_t bb) {

  return __builtin_popcountll(bb);

}

static inline int bsf(const uint64_t bb) {

  return __builtin_ctzll(bb);

}

#endif
