#include <stdio.h>
#include <pthread.h>
#include <stdint.h>
#include "timecontrol.h"
#include "search.h"
#include "go.h"
#include "hh.h"
#include "history.h"
#include "report.h"
#include "nodes.h"
#include "uci.h"
#include "pos.h"
#include "net.h"

extern int num_threads; // Bring in the thread count from uci.c

Position root_pos;

// This is the function that EVERY thread will run independently
void* search_worker(void* arg) {
  int thread_id = (int)(intptr_t)arg;
  TimeControl *tc = &time_control;
  int alpha = 0, beta = 0, delta = 0, score = 0;

  // Only the main thread (0) resets the global root history
  if (thread_id == 0) {
    hh_set_root();
  }
  
  // Every thread clears its own _Thread_local nodes stack
  clear_nodes();
  pos_copy(&root_pos, &nodes[0].pos);
  
  net_init_thread();                // 1. Initialize the thread's local Finny cache
  net_slow_rebuild_accs(&nodes[0]); // 2. Build the root position accumulator from scratch

  for (int depth = 1; depth <= tc->max_depth; depth++) {
    alpha = -INF;
    beta  = INF;
    delta = 10;

    if (depth >= 4) {
      alpha = score - delta;
      beta  = score + delta;
    }

    while (1) {
      // Begin searching!
      score = search(0, depth, alpha, beta);

      if (tc->finished) break;

      if (score <= alpha) alpha = score - delta;
      else if (score >= beta) beta = score + delta;
      else break;

      delta += delta;
    }

    // ONLY thread 0 prints to the UCI console. 
    // Helper threads stay completely silent to not crash the GUI.
    if (thread_id == 0 && !tc->finished) {
      report(depth);
    }

    if (tc->finished) break;

    // Any thread hitting the node limit can flag tc->finished
    check_tc_nodes(); 
    if (tc->finished) break;
  }
  
  return NULL;
}

void go(int silent) {
  pthread_t threads[256]; // Support up to 256 threads
  time_control.finished = 0;
  pos_copy(&nodes[0].pos, &root_pos);

  // 1. Spawn helper threads (IDs 1 through num_threads - 1)
  for (int i = 1; i < num_threads; i++) {
    pthread_create(&threads[i], NULL, search_worker, (void*)(intptr_t)i);
  }

  // 2. The main thread acts as Thread 0 and does the work too
  search_worker((void*)(intptr_t)0);

  // 3. When Thread 0 finishes (either found mate or ran out of time),
  // time_control.finished will be set to 1. We must wait for helpers to exit.
  for (int i = 1; i < num_threads; i++) {
    pthread_join(threads[i], NULL);
  }

  // 4. Report the best move
  if (!silent) {
    char bm_str[6];
    format_move(time_control.best_move, bm_str);
    printf("bestmove %s\n", bm_str);
  }
}