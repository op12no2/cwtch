#ifndef NODES_H
#define NODES_H

#include "types.h"
#include "pos.h"
#include "move.h"

#define MAX_PLY 64
#define MAX_MOVES 256

enum {
  NET_OP_MOVE,
  NET_OP_CAPTURE,
  NET_OP_EP_CAPTURE,
  NET_OP_CASTLE,
  NET_OP_PROMO_PUSH,
  NET_OP_PROMO_CAPTURE,
};

typedef struct {
  uint8_t type;
  uint8_t args[6];
} NetDeferred;

typedef struct {

  int16_t accs[2][NET_H1_SIZE];
  Position pos;
  NetDeferred net_deferred;
  uint8_t accs_dirty;
  move_t moves[MAX_MOVES];
  move_t played[MAX_MOVES];
  int num_moves;
  int16_t ranks[MAX_MOVES];
  int next_move;
  int in_check;
  move_t tt_move;
  int stage;
  move_t killer;

} Node;

extern Node nodes[MAX_PLY];

void clear_nodes(void);

#endif
