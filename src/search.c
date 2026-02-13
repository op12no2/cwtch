#include <stdio.h>
#include <string.h>
#include <math.h>
#include "types.h"
#include "builtins.h"
#include "nodes.h"
#include "pos.h"
#include "evaluate.h"
#include "search.h"
#include "makemove.h"
#include "timecontrol.h"
#include "iterate.h"
#include "move.h"
#include "qsearch.h"
#include "net.h"
#include "tt.h"
#include "hh.h"
#include "history.h"
#include "debug.h"

static int lmr[MAX_PLY][MAX_PLY];

void init_lmr(void) {
  for (int d = 1; d < MAX_PLY; d++) {
    for (int m = 1; m < MAX_PLY; m++) {
      lmr[d][m] = 0.77 + log(d) * log(m) / 2.36;
    }
  }
}

int search(const int ply, int depth, int alpha, const int beta) {

  Node *node = &nodes[ply];
  const Position *pos = &node->pos;

  //debug_verify(1, node, ply);

  if (ply >= MAX_PLY-1) {
    return net_eval(node);
  }

  // store hash for repetition detection (ply 0 already in hh from position())
  if (ply > 0) {
    hh_store(ply, pos->hash);
    if (is_draw(ply, pos->hash, pos->hmc)) {
      return 0;
    }
  }

  const int stm = pos->stm;
  const int opp = stm ^ 1;
  const int stm_king_idx = piece_index(KING, stm);
  const int in_check = is_attacked(pos, bsf(pos->all[stm_king_idx]), opp);
  const int is_root = ply == 0;
  const int is_pv = is_root || (beta - alpha != 1);

  if (depth <= 0 && in_check == 0) {
    return qsearch(ply, alpha, beta);
  }
  if (depth < 0)
    depth = 0;

  TimeControl *tc = &time_control;
  tc->nodes++;
  if ((tc->nodes & 1023) == 0) {
    check_tc();
    if (tc->finished)
      return 0;
  }

  const TT *entry = tt_get(pos);
  if (!is_pv && entry && entry->depth >= depth) {
    const int tt_flags = entry->flags;
    const int tt_score = get_adjusted_score(ply, entry->score);
    if (tt_flags == TT_EXACT || (tt_flags == TT_BETA && tt_score >= beta) || (tt_flags == TT_ALPHA && tt_score <= alpha)) {
      return tt_score;
    }
  }

  const move_t tt_move = entry ? entry->move : 0;
  // iir
  //if (is_pv && depth >= 5 && tt_move == 0) {
    //printf("*");
    //depth--;
  //}

  const int16_t ev = net_eval(node);

  // beta pruning
  if (!is_pv && !in_check && depth <= 8 &&  ev >= beta + (100 * depth)) {
    return ev;
  }

  Node *next_node = &nodes[ply + 1];
  Position *next_pos = &next_node->pos;

  // null move pruning
  if (!is_pv && !in_check && depth > 2 && ev > beta && !is_pawn_endgame(pos)) {
  
    const int nmp_depth = depth - 4;
  
    pos_copy(pos, next_pos);
    make_null_move(next_pos);
    memcpy(next_node->accs, node->accs, sizeof(node->accs));
  
    const int score = -search(ply+1, nmp_depth, -beta, -beta+1);
  
    if (score >= beta)
      return score > MATEISH ? beta : score;
  
    if (tc->finished)
      return 0;
  
  }

  move_t move = 0;
  move_t best_move = 0;
  int score = 0;
  int best_score = -INF;
  int played = 0;
  const uint64_t *next_stm_king_ptr = &next_pos->all[stm_king_idx];
  const int orig_alpha = alpha;

  init_next_search_move(node, in_check, tt_move);

  while ((move = get_next_search_move(node))) {

    const int is_quiet = !(move & (MOVE_FLAG_CAPTURE | MOVE_FLAG_PROMOTE));

    // lmp
    if (is_quiet && !is_pv && !in_check && alpha > -MATEISH && depth <= 2 && played > (5 * depth))
      continue;

    // futility
    //if (is_quiet && !is_pv && !in_check && alpha > -MATEISH && depth <= 4 && played && (ev + depth * 120) < alpha)
      //continue;
  
    pos_copy(pos, next_pos);
    memcpy(next_node->accs, node->accs, sizeof(node->accs));
    make_move(next_node, move);
    if (is_attacked(next_pos, bsf(*next_stm_king_ptr), opp))
      continue;

    node->played[played++] = move;

    if (is_pv) {
      if (played == 1) {
        score = -search(ply+1, depth-1, -beta, -alpha);
      }
      else {
        int d = depth - 1;
        const int lmr_m = played >= MAX_PLY ? MAX_PLY - 1 : played;

        // lmr for pv quiets
        if (depth >= 3 && played >= 3 && is_quiet && !in_check) {
          d -= lmr[depth][lmr_m];
          if (d < 1) d = 1;
        }

        score = -search(ply+1, d, -alpha-1, -alpha);

        // re-search at full depth if reduced search beats alpha
        if (!tc->finished && score > alpha && d < depth - 1) {
          score = -search(ply+1, depth-1, -alpha-1, -alpha);
        }

        // full window re-search
        if (!tc->finished && score > alpha) {
          score = -search(ply+1, depth-1, -beta, -alpha);
        }
      }
    }
    else {
      int d = depth - 1;
      const int lmr_m = played >= MAX_PLY ? MAX_PLY - 1 : played;

      // lmr for non-pv quiets - reduce more aggressively
      if (depth >= 3 && played >= 2 && is_quiet && !in_check) {
        d -= lmr[depth][lmr_m] + 1;
        if (d < 1) d = 1;
      }

      score = -search(ply+1, d, -beta, -alpha);

      // re-search at full depth if reduced search beats alpha
      if (!tc->finished && score > alpha && d < depth - 1) {
        score = -search(ply+1, depth-1, -beta, -alpha);
      }
    }

    if (tc->finished)
      return 0;

    if (score > best_score) {
      best_score = score;
      best_move = move;
      if (score > alpha) {
        alpha = score;
        if (is_root) {
          tc->best_move = best_move;
          tc->best_score = best_score;
        }
        if (score >= beta) {
          if (!(best_move & MOVE_FLAG_CAPTURE)) {
            const int bonus = depth * depth;
            update_piece_to_history(pos, best_move, bonus);
            for (int i=0; i < played-1; i++) {
              const move_t pm = node->played[i];
              if (!(pm & (MOVE_FLAG_CAPTURE | MOVE_FLAG_PROMOTE))) {
                update_piece_to_history(pos, pm, -bonus);
              }  
            }  
          }
          tt_put(pos, TT_BETA, depth, put_adjusted_score(ply, best_score), best_move);
          return score;
        }
      }
    }
  }

  if (played == 0) {
    return in_check ? (-MATE + ply) : 0;
  }

  tt_put(pos, (alpha > orig_alpha) ? TT_EXACT : TT_ALPHA, depth, put_adjusted_score(ply, best_score), best_move);

  return best_score;

}
