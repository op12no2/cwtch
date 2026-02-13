#ifndef TT_H
#define TT_H

#include <stdint.h>
#include "move.h"
#include "pos.h"

#define TT_EXACT 1
#define TT_ALPHA 2
#define TT_BETA 4

#define TT_DEFAULT_MB 16

typedef struct {

  uint64_t hash;
  move_t move;
  uint8_t flags;
  uint8_t depth;
  int16_t score;

} TT;

int new_tt(size_t megabytes);
void tt_clear(void); 
int is_tt_null();
void new_game(void);
TT *tt_get(const Position *pos);
void tt_put(const Position *pos, const int flags, const int depth, const int score, const move_t move);
int get_adjusted_score(const int ply, const int score);
int put_adjusted_score(const int ply, const int score);

#endif