#ifndef HISTORY_H
#define HISTORY_H

#include <string.h>
#include "types.h"
#include "pos.h"
#include "move.h"
#include "nodes.h"

#define MAX_HISTORY 32766
#define KILLER (1 << 24)   // above any piece_to + conthist rank sum (ranks are int32)

extern int16_t piece_to_history[12][64];
extern int16_t cont_hist[12][64][12][64];   // [prev piece][prev to][piece][to]

inline void clear_piece_to_history(void) {
  memset(piece_to_history, 0, sizeof(piece_to_history));
}

void update_piece_to_history(const Position *pos, const move_t move, int bonus);
void update_killer(Node *node, const move_t move);
void age_piece_to_history(void);

void clear_cont_history(void);
void age_cont_history(void);
void update_cont_history(Node *node, const Position *pos, const move_t move, int bonus);

// --- pawn correction history -------------------------------------------------
// Adjusts the static eval by the historical (search - eval) error seen for the
// current pawn structure. Keyed on-demand from the pawn bitboards, so there is
// no incremental-hash plumbing in makemove. Persists across moves within a
// game; cleared in new_game().

#define CORRHIST_BITS 14
#define CORRHIST_SIZE (1 << CORRHIST_BITS)
#define CORRHIST_MASK (CORRHIST_SIZE - 1)
#define CORRHIST_GRAIN 256                  // fixed point: correction = entry / GRAIN
#define CORRHIST_MAX (CORRHIST_GRAIN * 96)  // clamp on stored value (fits int16)
#define CORRHIST_DIFF_MAX 512               // clamp on a single update's raw error
#define CORRHIST_WEIGHT_SCALE 256
#define CORRHIST_WEIGHT_MAX 16

extern int16_t pawn_corrhist[2][CORRHIST_SIZE];

void clear_corrhist(void);
void update_corrhist(const Position *pos, const int depth, int diff);

static inline uint64_t pawn_key(const Position *pos) {
  uint64_t k = pos->all[WPAWN] * 0x9E3779B97F4A7C15ULL;
  k ^= (pos->all[BPAWN] + 0x9E3779B97F4A7C15ULL) * 0xBF58476D1CE4E5B9ULL;
  return k ^ (k >> 31);
}

static inline int corrhist_correct(const Position *pos, int eval) {
  const int cv = pawn_corrhist[pos->stm][pawn_key(pos) & CORRHIST_MASK];
  int e = eval + cv / CORRHIST_GRAIN;
  if (e > MATEISH) e = MATEISH;
  else if (e < -MATEISH) e = -MATEISH;
  return e;
}

#endif