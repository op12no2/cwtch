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

inline void clear_piece_to_history(void) {
  memset(piece_to_history, 0, sizeof(piece_to_history));
}

void update_piece_to_history(const Position *pos, const move_t move, int bonus);
void update_killer(Node *node, const move_t move);
void age_piece_to_history(void);

#endif