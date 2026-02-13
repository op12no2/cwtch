#ifndef NET_H
#define NET_H

#include "nodes.h"

int init_weights(void);
int net_eval(Node *node);
void net_slow_rebuild_accs(Node *node);
void net_move(Node *node, const int piece, const int from, const int to);
void net_capture(Node *node, const int from_piece, const int from, const int to_piece, const int to);
void net_ep_capture(Node *node, const int from_pawn, const int from, const int to, const int opp_pawn, const int opp_ep);
void net_castle(Node *node, const int king_piece, const int king_fr_sq, const int king_to_sq, const int rook_piece, const int rook_fr_sq, const int rook_to_sq);
void net_promo_push(Node *node, const int pawn_piece, const int pawn_fr, const int pawn_to, const int promote_piece);
void net_promo_capture(Node *node, const int pawn_piece, const int pawn_fr, const int pawn_to, const int capture_piece, const int promote_piece);

inline int net_base(const int piece, const int sq) {
  return (((piece << 6) | sq) * NET_H1_SIZE);
}

#endif
