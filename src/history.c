#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include "history.h"
#include "pos.h"
#include "move.h"
#include "nodes.h"

int16_t piece_to_history[12][64];
int16_t cont_hist[12][64][12][64];

void update_piece_to_history(const Position *pos, const move_t move, int bonus) {

  const int from = (move >> 6) & 0x3F;
  const int to = move & 0x3F;
  const int piece = pos->board[from];
  int16_t *entry = &piece_to_history[piece][to];

  // gravity self-bounds to +/-MAX_HISTORY (holds while |bonus| <= MAX_HISTORY)
  *entry += bonus - *entry * abs(bonus) / MAX_HISTORY;

}

void update_cont_history(Node *node, const Position *pos, const move_t move, int bonus) {

  int16_t (*const cont)[64] = node->cont_entry;

  if (!cont)
    return;

  const int from = (move >> 6) & 0x3F;
  const int to = move & 0x3F;
  const int piece = pos->board[from];
  int16_t *entry = &cont[piece][to];

  // gravity self-bounds to +/-MAX_HISTORY (holds while |bonus| <= MAX_HISTORY)
  *entry += bonus - *entry * abs(bonus) / MAX_HISTORY;

}

void update_killer(Node *node, const move_t move) {

  node->killer = move;

}

