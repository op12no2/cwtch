#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include "history.h"
#include "pos.h"
#include "move.h"
#include "nodes.h"

int16_t piece_to_history[12][64];
int16_t pawn_corrhist[2][CORRHIST_SIZE];

void clear_corrhist(void) {
  memset(pawn_corrhist, 0, sizeof(pawn_corrhist));
}

// EMA toward the depth-weighted search-vs-eval error for this pawn structure.
void update_corrhist(const Position *pos, const int depth, int diff) {

  if (diff > CORRHIST_DIFF_MAX)
    diff = CORRHIST_DIFF_MAX;
  else if (diff < -CORRHIST_DIFF_MAX)
    diff = -CORRHIST_DIFF_MAX;

  int16_t *const entry = &pawn_corrhist[pos->stm][pawn_key(pos) & CORRHIST_MASK];

  int weight = depth + 1;
  if (weight > CORRHIST_WEIGHT_MAX)
    weight = CORRHIST_WEIGHT_MAX;

  const int target = diff * CORRHIST_GRAIN;
  int v = (*entry * (CORRHIST_WEIGHT_SCALE - weight) + target * weight) / CORRHIST_WEIGHT_SCALE;

  if (v > CORRHIST_MAX)
    v = CORRHIST_MAX;
  else if (v < -CORRHIST_MAX)
    v = -CORRHIST_MAX;

  *entry = (int16_t)v;

}

void age_piece_to_history(void) {
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
  //int age_needed = 0;

  if (value >= MAX_HISTORY) { 
    value = MAX_HISTORY;
    //age_needed = 1;
  }  
  else if (value <= -MAX_HISTORY) { 
    value = -MAX_HISTORY;
    //age_needed = 1;
  }

  *entry = value;

  //if (age_needed)
    //age_piece_to_history();

}

void update_killer(Node *node, const move_t move) {

  node->killer = move;

}

