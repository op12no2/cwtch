#include <stdio.h>
#include "timecontrol.h"
#include "search.h"
#include "go.h"
#include "hh.h"
#include "history.h"
#include "report.h"
#include "nodes.h"

void go(int silent) {

  char bm_str[6];
  TimeControl *tc = &time_control;
  int alpha = 0;
  int beta = 0;
  int delta = 0;
  int score = 0;

  hh_set_root();
  clear_piece_to_history();
  clear_nodes();

  for (int depth=1; depth <= tc->max_depth; depth++) {

    alpha = -INF;
    beta  = INF;
    delta = 10;

    if (depth >= 4) {
      alpha = score - delta;
      beta  = score + delta;
    }

    while (1) {

      score = search(0, depth, alpha, beta);

      if (tc->finished) {
        break;
      }

      if (score <= alpha) {
        alpha = score - delta;
      }
      else if (score >= beta) {
        beta = score + delta;
      }
      else {
        break;
      }

      delta += delta;

    }

    if (!silent)
      report(depth);

    if (tc->finished)
      break;

    age_piece_to_history();
      
  }

  format_move(tc->best_move, bm_str);

  if (!silent)
    printf("bestmove %s\n", bm_str);

}
