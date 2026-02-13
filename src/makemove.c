#include <string.h>
#include "builtins.h"
#include "nodes.h"
#include "move.h"
#include "types.h"
#include "iterate.h"
#include "zobrist.h"
#include "net.h"

#define MOVE_FLAGS_PROMOCAP (MOVE_FLAG_PROMOTE | MOVE_FLAG_CAPTURE) 

// rook squares for castling indexed by king destination
static const int rook_from[64] = {
  [G1] = H1, [C1] = A1,
  [G8] = H8, [C8] = A8,
};

static const int rook_to[64] = {
  [G1] = F1, [C1] = D1,
  [G8] = F8, [C8] = D8,
};

// AND with rights to update - 15 means no change
static const uint8_t rights_mask[64] = {
  13, 15, 15, 15, 12, 15, 15, 14,  // A1=13, E1=12, H1=14
  15, 15, 15, 15, 15, 15, 15, 15,
  15, 15, 15, 15, 15, 15, 15, 15,
  15, 15, 15, 15, 15, 15, 15, 15,
  15, 15, 15, 15, 15, 15, 15, 15,
  15, 15, 15, 15, 15, 15, 15, 15,
  15, 15, 15, 15, 15, 15, 15, 15,
   7, 15, 15, 15,  3, 15, 15, 11,  // A8=7, E8=3, H8=11
};


void make_move(Node *node, const move_t move) {

  Position *pos = &node->pos;
  uint8_t *board = pos->board;
  uint64_t *all = pos->all;
  uint64_t *colour = pos->colour;
  const int stm = pos->stm;
  const int opp = stm ^ 1;
  const int flags = move & 0xFFF000;
  const int from = (move >> 6) & 0x3F;
  const int to = move & 0x3F;
  const int from_piece = board[from];
  const int to_piece = board[to];
  const uint64_t from_bb = 1ULL << from;
  const uint64_t to_bb = 1ULL << to;
  const uint8_t old_rights = pos->rights;
  uint64_t hash = pos->hash ^ zob_pieces[from_piece][from] ^ zob_ep[pos->ep] ^ zob_rights[old_rights];

  pos->ep = 0;

  if (flags & MOVE_FLAGS_EXTRA) {

    if ((flags & MOVE_FLAGS_EXTRA) == MOVE_FLAG_CAPTURE) {  // most common
      board[from] = EMPTY;
      board[to] = from_piece;
      all[from_piece] ^= from_bb ^ to_bb;
      all[to_piece] ^= to_bb;
      colour[stm] ^= from_bb ^ to_bb;
      colour[opp] ^= to_bb;
      hash ^= zob_pieces[from_piece][to] ^ zob_pieces[to_piece][to];
      net_capture(node, from_piece, from, to_piece, to);
    }
    
    else if (flags & MOVE_FLAG_EPCAPTURE) {
      const int cap_sq = to + (stm ? 8 : -8);
      const uint64_t cap_bb = 1ULL << cap_sq;
      const int cap_piece = board[cap_sq];
      board[from] = EMPTY;
      board[to] = from_piece;
      board[cap_sq] = EMPTY;
      all[from_piece] ^= from_bb ^ to_bb;
      all[cap_piece] ^= cap_bb;
      colour[stm] ^= from_bb ^ to_bb;
      colour[opp] ^= cap_bb;
      hash ^= zob_pieces[from_piece][to] ^ zob_pieces[cap_piece][cap_sq];
      net_ep_capture(node, from_piece, from, to, cap_piece, cap_sq);
    }

    else if ((flags & MOVE_FLAGS_PROMOCAP) == MOVE_FLAGS_PROMOCAP) {
      const int promo_piece = piece_index((flags >> 12) & 7, stm);
      board[from] = EMPTY;
      board[to] = promo_piece;
      all[from_piece] ^= from_bb;
      all[to_piece] ^= to_bb;
      all[promo_piece] ^= to_bb;
      colour[stm] ^= from_bb ^ to_bb;
      colour[opp] ^= to_bb;
      hash ^= zob_pieces[to_piece][to] ^ zob_pieces[promo_piece][to];
      net_promo_capture(node, from_piece, from, to, to_piece, promo_piece);
    }

    else if (flags & MOVE_FLAG_PROMOTE) {
      const int promo_piece = piece_index((flags >> 12) & 7, stm);
      board[from] = EMPTY;
      board[to] = promo_piece;
      all[from_piece] ^= from_bb;
      all[promo_piece] ^= to_bb;
      colour[stm] ^= from_bb ^ to_bb;
      hash ^= zob_pieces[promo_piece][to];
      net_promo_push(node, from_piece, from, to, promo_piece);
    }

    else if (flags & MOVE_FLAG_CASTLE) {
      const int rf = rook_from[to];
      const int rt = rook_to[to];
      const uint64_t rf_bb = 1ULL << rf;
      const uint64_t rt_bb = 1ULL << rt;
      const int rook_piece = board[rf];
      board[from] = EMPTY;
      board[to] = from_piece;
      board[rf] = EMPTY;
      board[rt] = rook_piece;
      all[from_piece] ^= from_bb ^ to_bb;
      all[rook_piece] ^= rf_bb ^ rt_bb;
      colour[stm] ^= from_bb ^ to_bb ^ rf_bb ^ rt_bb;
      hash ^= zob_pieces[from_piece][to] ^ zob_pieces[rook_piece][rf] ^ zob_pieces[rook_piece][rt];
      net_castle(node, from_piece, from, to, rook_piece, rf, rt);
    }

    else {
      // pawn2
      board[from] = EMPTY;
      board[to] = from_piece;
      all[from_piece] ^= from_bb ^ to_bb;
      colour[stm] ^= from_bb ^ to_bb;
      hash ^= zob_pieces[from_piece][to];
      pos->ep = (from + to) >> 1;
      net_move(node, from_piece, from, to);
    }

  }
  
  else {
    // quiet move
    board[from] = EMPTY;
    board[to] = from_piece;
    all[from_piece] ^= from_bb ^ to_bb;
    colour[stm] ^= from_bb ^ to_bb;
    hash ^= zob_pieces[from_piece][to];
    net_move(node, from_piece, from, to);
  }

  pos->rights &= rights_mask[from] & rights_mask[to];
  pos->occupied = colour[WHITE] | colour[BLACK];
  pos->stm = opp;
  pos->hash = hash ^ zob_rights[pos->rights] ^ zob_ep[pos->ep] ^ zob_stm[1];

  // update half-move clock
  const int is_pawn = (from_piece == WPAWN) || (from_piece == BPAWN);
  const int is_capture = flags & MOVE_FLAG_CAPTURE;
  pos->hmc = (is_pawn || is_capture) ? 0 : pos->hmc + 1;

}

void play_move(Node *node, char *uci_move) {

  char buf[6];

  Position *pos = &node->pos;
  const int stm = pos->stm;
  const int opp = stm ^ 1;
  const int stm_king_sq = piece_index(KING, stm);
  const int in_check = is_attacked(pos, bsf(pos->all[stm_king_sq]), opp);
  move_t move ;

  init_next_search_move(node, in_check, 0);

  while ((move = get_next_search_move(node))) {

    format_move(move, buf);
    if (!strcmp(uci_move, buf)) {
      make_move(node, move);
      return;
    }
  }

  return;

}

void make_null_move(Position *pos) {

  int ep        = pos->ep;
  uint64_t hash = pos->hash;

  hash ^= zob_ep[ep];
  ep   = 0;
  hash ^= zob_ep[ep];

  hash ^= zob_stm[1];

  pos->hash = hash;
  pos->stm  ^= 1;
  pos->ep   = (uint8_t)ep;
  pos->hmc  = 0;

}
