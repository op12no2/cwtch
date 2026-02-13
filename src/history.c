#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include "history.h"
#include "pos.h"
#include "move.h"

int16_t piece_to_history[12][64];

static inline void age_piece_to_history(void) {
  for (int i=0; i < 12; i++) {
    for (int j=0; j < 64; j++) {
      piece_to_history[i][j] /= 2;
    }
  }
}

void update_piece_to_history(const Position *pos, const move_t move, int bonus) {

  const int from = (move >> 6) & 0x3F;
  const int to = move & 0x3F;
  const int piece = pos->board[from];
  int16_t *entry = &piece_to_history[piece][to];

  int value = *entry + bonus - *entry * abs(bonus) / MAX_HISTORY;
  int age_needed = 0;

  if (value >= MAX_HISTORY) { 
    value = MAX_HISTORY;
    age_needed = 1;
  }  
  else if (value <= -MAX_HISTORY) { 
    value = -MAX_HISTORY;
    age_needed = 1;
  }

  *entry = value;

  if (age_needed)
    age_piece_to_history();

}
