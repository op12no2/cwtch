#ifndef TIMECONTROL_H
#define TIMECONTROL_H

#include <stdint.h>
#include "move.h"

typedef struct {

  uint64_t start_time;
  uint64_t finish_time;
  int max_depth; // iterative deepening depth, MAX_PLY if infinite.
  uint64_t max_nodes;
  uint8_t finished;
  uint64_t nodes;  // node count when searching
  move_t best_move;
  int best_score;

} TimeControl;

extern TimeControl time_control;

void init_tc(int64_t wtime, int64_t winc, int64_t btime, int64_t binc, int64_t max_nodes, int64_t move_time, int max_depth, int moves_to_go);
void check_tc(void);

#endif