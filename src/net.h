#ifndef NET_H
#define NET_H

#include "nodes.h"

int init_weights(void);
int net_eval(Node *node);
void net_slow_rebuild_accs(Node *node);
void update_accs(Node *node);
void lazy_update_accs(Node *node);
void net_move(Node *node);
void net_capture(Node *node);
void net_ep_capture(Node *node);
void net_castle(Node *node);
void net_promo_push(Node *node);
void net_promo_capture(Node *node);

inline int net_base(const int piece, const int sq) {
  return (((piece << 6) | sq) * NET_H1_SIZE);
}

#endif
