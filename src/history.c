#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include "history.h"
#include "pos.h"
#include "move.h"
#include "nodes.h"

int16_t piece_to_history[12][64];
int16_t cont_history[12][64][12][64];
int16_t capture_history[12][64][6];

// gravity self-bounds to +/-MAX_HISTORY for |bonus| <= MAX_HISTORY
static void apply_gravity(int16_t *entry, const int bonus) {

  *entry += bonus - *entry * abs(bonus) / MAX_HISTORY;

}

void update_piece_to_history(const Position *pos, const move_t move, int bonus) {

  const int from = (move >> 6) & 0x3F;
  const int to = move & 0x3F;
  const int piece = pos->board[from];

  apply_gravity(&piece_to_history[piece][to], bonus);

}

void update_cont_history(Node *node, const Position *pos, const move_t move, int bonus) {

  int16_t (*const cont)[64] = node->cont_entry;

  if (!cont)
    return;

  const int from = (move >> 6) & 0x3F;
  const int to = move & 0x3F;
  const int piece = pos->board[from];

  apply_gravity(&cont[piece][to], bonus);

}

void update_capture_history(const Position *pos, const move_t move, int bonus) {

  const int from = (move >> 6) & 0x3F;
  const int to = move & 0x3F;
  const int piece = pos->board[from];
  int victim = pos->board[to];
  victim = (victim == EMPTY) ? PAWN : victim % 6;  // ep/promo -> pawn

  apply_gravity(&capture_history[piece][to][victim], bonus);

}

void update_killer(Node *node, const move_t move) {

  node->killer = move;

}

