#include "types.h"
#include "timecontrol.h"
#include "move.h"
#include "nodes.h"

TimeControl time_control;

void init_tc(int64_t wtime, int64_t winc, int64_t btime, int64_t binc, int64_t max_nodes, int64_t move_time, int max_depth, int moves_to_go) {

  TimeControl *tc = &time_control;

  if (wtime < 0) wtime = 0;
  if (winc < 0) winc = 0;
  if (btime < 0) btime = 0;
  if (binc < 0) binc = 0;
  if (max_nodes < 0) max_nodes = 0;
  if (move_time < 0) move_time = 0;
  if (moves_to_go < 0) moves_to_go = 0;
  if (max_depth < 0) max_depth = 0;

  int stm = nodes[0].pos.stm;  // assume a position command has been issued

  if (!max_nodes && !max_depth && !wtime && !winc && !btime && !binc && !move_time)
    move_time = 100;

  if (!moves_to_go)
    moves_to_go = 20;

  if (moves_to_go < 2) // stops losses on time
    moves_to_go = 2;

  if (!max_depth)
    max_depth = MAX_PLY;

  if (!move_time && stm == WHITE && wtime) {
    move_time = wtime / moves_to_go + winc / 2;
    if (!move_time)
      move_time = 1;
  }
  else if (!move_time && stm == BLACK && btime) {
    move_time = btime / moves_to_go + binc / 2;
    if (!move_time)
      move_time = 1;
  }

  tc->start_time = time_ms();

  if (move_time) {
    tc->finish_time = tc->start_time + move_time;
  }

  tc->max_nodes = max_nodes;
  tc->max_depth = max_depth;

  tc->finished = 0;
  tc->nodes = 0;
  tc->best_move = 0;
  tc->best_score = 0;

}

void check_tc(void) {

  TimeControl *tc = &time_control;

  if (tc->finished)
    return;

  if (tc->finish_time) {
    if (time_ms() >= tc->finish_time) {
      tc->finished = 1;
      return;
    }
  }

  if (tc->max_nodes) {
    if (tc->nodes >= tc->max_nodes) {
      tc->finished = 1;
      return;
    }
  }

  return;

}