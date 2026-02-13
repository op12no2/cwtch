#ifndef NODES_H
#define NODES_H

#include "types.h"
#include "pos.h"
#include "move.h"

#define MAX_PLY 64
#define MAX_MOVES 256

typedef struct {

  int16_t accs[2][NET_H1_SIZE]; 
  Position pos;
  move_t moves[MAX_MOVES];
  move_t played[MAX_MOVES];
  int num_moves;
  int16_t ranks[MAX_MOVES];
  int next_move;
  int in_check;
  move_t tt_move;
  int stage;


} Node;

extern Node nodes[MAX_PLY];

#endif
