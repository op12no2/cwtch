#ifndef HISTORY_H
#define HISTORY_H

#include <string.h>
#include "types.h"
#include "pos.h"
#include "move.h"
#include "nodes.h"

#define MAX_HISTORY 32766
#define KILLER 32767

extern int16_t piece_to_history[12][64];
extern int16_t cont_hist[12][64][12][64];  // [prev piece][prev to][piece][to]

inline void clear_piece_to_history(void) {
  memset(piece_to_history, 0, sizeof(piece_to_history));
}

inline void clear_cont_history(void) {
  memset(cont_hist, 0, sizeof(cont_hist));
}

void update_piece_to_history(const Position *pos, const move_t move, int bonus);
void update_cont_history(Node *node, const Position *pos, const move_t move, int bonus);
void update_killer(Node *node, const move_t move);

#endif