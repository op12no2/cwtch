#include <stdint.h>
#include "types.h"
#include "pos.h"
#include "corrhist.h"

int16_t pawn_corr[2][CORR_SIZE];

// splitmix64 finaliser
static inline uint64_t mix64(uint64_t x) {

  x ^= x >> 30;
  x *= 0xbf58476d1ce4e5b9ULL;
  x ^= x >> 27;
  x *= 0x94d049bb133111ebULL;
  x ^= x >> 31;

  return x;

}

static inline int16_t *pawn_entry(const Position *pos) {

  const uint64_t key = mix64(pos->all[WPAWN] ^ mix64(pos->all[BPAWN]));

  return &pawn_corr[pos->stm][key & (CORR_SIZE - 1)];

}

int correct_eval(const Position *pos, const int ev) {

  const int corrected = ev + *pawn_entry(pos) / CORR_GRAIN;

  if (corrected <= -MATEISH)
    return -MATEISH + 1;
  if (corrected >= MATEISH)
    return MATEISH - 1;

  return corrected;

}

void update_corrhist(const Position *pos, const int depth, const int diff) {

  int16_t *entry = pawn_entry(pos);
  const int w = depth + 1 < CORR_WEIGHT_MAX ? depth + 1 : CORR_WEIGHT_MAX;
  int v = (*entry * (CORR_WEIGHT_SCALE - w) + diff * CORR_GRAIN * w) / CORR_WEIGHT_SCALE;

  if (v < -CORR_LIMIT)
    v = -CORR_LIMIT;
  if (v > CORR_LIMIT)
    v = CORR_LIMIT;

  *entry = (int16_t)v;

}
