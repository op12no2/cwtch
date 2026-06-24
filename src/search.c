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
#include "pv.h"
#include "see.h"

static int lmr[MAX_PLY][MAX_MOVES];

void init_lmr(void) {
  for (int d = 1; d < MAX_PLY; d++) {
    for (int m = 1; m < MAX_MOVES; m++) {
      lmr[d][m] = 0.77 + log(d) * log(m < 64 ? m : 63) / 2.36;
    }
  }
}

int search(const int ply, int depth, int alpha, int beta) {

  Node *node = &nodes[ply];
  pv_len[ply] = 0;
  const Position *pos = &node->pos;

  //debug_verify(1, node, ply);

  if (ply >= MAX_PLY-1) {
    lazy_update_accs(node);
    return net_eval(node);
  }

  // store hash for repetition detection (ply 0 already in hh from position())
  if (ply > 0) {
    hh_store(ply, pos->hash);
    if (is_draw(ply, pos->hash, pos->hmc) || is_mat_draw(pos)) {
      return 0;
    }
  }

  const int stm = pos->stm;
  const int opp = stm ^ 1;
  const int stm_king_idx = piece_index(KING, stm);
  const int in_check = is_attacked(pos, bsf(pos->all[stm_king_idx]), opp);
  const int is_root = ply == 0;
  const int is_pv = is_root || (beta - alpha != 1);
  const move_t excluded = node->excluded_move;

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

  // mate distance pruning 
  if (alpha < -MATE + ply)
    alpha = -MATE + ply;
  if (beta > MATE - ply - 1)
    beta = MATE - ply - 1;
  if (alpha >= beta)
    return alpha;

  const TT *entry = tt_get(pos);
  if (!is_pv && !excluded && entry && entry->depth >= depth) {
    const int tt_flags = entry->flags;
    const int tt_score = get_adjusted_score(ply, entry->score);
    if (tt_flags == TT_EXACT || (tt_flags == TT_BETA && tt_score >= beta) || (tt_flags == TT_ALPHA && tt_score <= alpha)) {
      return tt_score;
    }
  }

  // capture tt fields before null move search can overwrite the entry slot
  const move_t tt_move = entry ? entry->move : 0;
  const int tt_depth = entry ? entry->depth : 0;
  const int tt_flags = entry ? entry->flags : 0;
  const int tt_score = entry ? get_adjusted_score(ply, entry->score) : 0;

  //iir
  if (!in_check && !excluded && depth >= 4 && (!tt_move || (entry && entry->depth + 4 < depth))) {
    depth--;
  }

  lazy_update_accs(node);
  const int16_t ev = net_eval(node);

  // beta pruning
  if (!is_pv && !excluded && !in_check && depth <= 8 && ev >= beta + (100 * depth)) {
    return ev;
  }

  Node *next_node = &nodes[ply + 1];
  Position *next_pos = &next_node->pos;

  // null move pruning
  if (!is_pv && !excluded && !in_check && depth > 2 && ev > beta && !is_pawn_endgame(pos)) {
  
    const int nmp_depth = depth - 4;
  
    pos_copy(pos, next_pos);
    make_null_move(next_pos);
    memcpy(next_node->accs, node->accs, sizeof(node->accs));
    next_node->accs_dirty = 0;
    next_node->cont_entry = NULL;

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

    if (move == excluded)
      continue;

    const int is_quiet = !(move & (MOVE_FLAG_CAPTURE | MOVE_FLAG_PROMOTE));

    // lmp
    if (is_quiet && !is_pv && !in_check && alpha > -MATEISH && depth <= 2 && played > (5 * depth))
      continue;

    // futility
    if (is_quiet && !is_pv && !in_check && alpha > -MATEISH && depth <= 4 && played && (ev + depth * 120) < alpha)
      continue;

    // see pruning
    if (!is_quiet && !is_pv && !in_check && alpha > -MATEISH && depth <= 2 && played && !see_ge(pos, move, 0))
      continue;

    // singular extensions
    int extension = 0;
    if (!is_root && !excluded && depth >= 8 && move == tt_move
        && tt_depth + 4 >= depth && tt_flags != TT_ALPHA
        && tt_score > -MATEISH && tt_score < MATEISH) {

      const int s_beta = tt_score - 8 * depth;
      const int s_depth = (depth - 1) / 2;

      node->excluded_move = tt_move;
      const int s_score = search(ply, s_depth, s_beta - 1, s_beta);
      node->excluded_move = 0;
      node->stage = 1;          // restore: tt-move already returned, generate noisy next
      node->tt_move = tt_move;  // defensive (verification reset it to the same value)

      if (tc->finished)
        return 0;

      if (s_score < s_beta)       // no other move reaches s_beta -> tt-move is singular
        extension = 1;
      else if (s_beta >= beta)    // verification already fails high -> multicut
        return s_beta;
      else if (tt_score >= beta)  // not singular and likely fails high -> search less
        extension = -1;
    }

    const int from = (move >> 6) & 0x3F;
    const int to = move & 0x3F;
    const int moved_piece = pos->board[from];
    const int hist = is_quiet ? piece_to_history[moved_piece][to] : 0;

    pos_copy(pos, next_pos);
    make_move(next_node, move);
    if (is_attacked(next_pos, bsf(*next_stm_king_ptr), opp))
      continue;

    next_node->accs_dirty = 1;
    next_node->cont_entry = cont_hist[moved_piece][to];

    node->played[played++] = move;

    const int new_depth = depth - 1 + extension;

    if (is_pv) {
      if (played == 1) { // pv move 1
        score = -search(ply+1, new_depth, -beta, -alpha);
      }
      else { // pv move > 1

        int d = new_depth;

        if (depth >= 3 && played >= 3 && is_quiet && !in_check) {
          d -= lmr[depth][played];
          d += (hist > 0);
          d = (d < 1) ? 1 : d;
        }

        score = -search(ply+1, d, -alpha-1, -alpha);

        if (!tc->finished && score > alpha) {
          score = -search(ply+1, new_depth, -beta, -alpha);
        }
      }
    }
    else { // not pv

      int d = new_depth;

      if (depth >= 3 && played >= 2 && is_quiet && !in_check) {
        d -= lmr[depth][played] + 1;
        d += (hist > 0);
        d = (d < 1) ? 1 : d;
      }

      score = -search(ply+1, d, -beta, -alpha);

      if (!tc->finished && score > alpha && d < new_depth) {
        score = -search(ply+1, new_depth, -beta, -alpha);
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
        if (is_pv) {
          collect_pv(ply, best_move);
        }
        if (score >= beta) {
          if (!(best_move & (MOVE_FLAG_CAPTURE | MOVE_FLAG_PROMOTE))) {
            update_killer(node, best_move);
            int bonus = 16 * depth * depth;
            if (bonus > 2048)
              bonus = 2048;
            update_piece_to_history(pos, best_move, bonus);
            update_cont_history(node, pos, best_move, bonus);
            for (int i=0; i < played-1; i++) {
              const move_t pm = node->played[i];
              if (!(pm & (MOVE_FLAG_CAPTURE | MOVE_FLAG_PROMOTE))) {
                update_piece_to_history(pos, pm, -bonus);
                update_cont_history(node, pos, pm, -bonus);
              }
            }
          }
          if (!excluded)
            tt_put(pos, TT_BETA, depth, put_adjusted_score(ply, best_score), best_move);
          return score;
        }
      }
    }
  }

  if (played == 0) {
    return in_check ? (-MATE + ply) : 0; 
  }

  if (!excluded)
    tt_put(pos, (alpha > orig_alpha) ? TT_EXACT : TT_ALPHA, depth, put_adjusted_score(ply, best_score), best_move);

  return best_score;

}
