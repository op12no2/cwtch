#include <stdio.h>
#include <stdlib.h>
#include "debug.h"
#include "zobrist.h"
#include "net.h"
#include "pos.h"

void debug_verify(int enable, Node *node, int ply) {

  if (!enable)
    return;

  const Position *pos = &node->pos;

  // verify incremental hash
  uint64_t rebuilt_hash = rebuild_hash(pos);
  if (pos->hash != rebuilt_hash) {
    printf("HASH MISMATCH at ply %d: incremental %llu != rebuilt %llu\n",
           ply, (unsigned long long)pos->hash, (unsigned long long)rebuilt_hash);
    print_board(pos);
    exit(1);
  }

  // verify incremental accumulators
  Node verify_node;
  verify_node.pos = *pos;
  net_slow_rebuild_accs(&verify_node);
  for (int i = 0; i < NET_H1_SIZE; i++) {
    if (node->accs[0][i] != verify_node.accs[0][i]) {
      printf("ACC[0][%d] MISMATCH at ply %d: incremental %d != rebuilt %d\n",
             i, ply, node->accs[0][i], verify_node.accs[0][i]);
      print_board(pos);
      exit(1);
    }
    if (node->accs[1][i] != verify_node.accs[1][i]) {
      printf("ACC[1][%d] MISMATCH at ply %d: incremental %d != rebuilt %d\n",
             i, ply, node->accs[1][i], verify_node.accs[1][i]);
      print_board(pos);
      exit(1);
    }
  }

}
