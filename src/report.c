#include <stdio.h>
#include "timecontrol.h"
#include "pv.h"

void report(int depth) {

  const int pvl = pv_len[0];
  int next_pv_char = 0;
  move_t *const pv = pv_table[0];
  char pv_str[MAX_PLY * 6 + 1];
  TimeControl *tc = &time_control;
    
  for (int i=pvl-1; i >= 0; i--) {
    next_pv_char += format_move(pv[i], &pv_str[next_pv_char]);
    pv_str[next_pv_char++] = ' ';
  }
    
  pv_str[next_pv_char] = '\0';
    
  uint64_t end_ms = time_ms();
  uint64_t elapsed_ms = end_ms - tc->start_time;
  uint64_t nps = (tc->nodes * 1000ULL) / (elapsed_ms ? elapsed_ms : 1);
    
  printf("info depth %d score cp %d time %lu nodes %lu nps %lu pv %s\n", depth, tc->best_score, elapsed_ms, tc->nodes, nps, pv_str);

}

