#include "nodes.h"

Node nodes[MAX_PLY];

void clear_nodes(void) {

  for (int i=0; i < MAX_PLY; i++) {

    Node *node = &nodes[i];

    node->killer = 0;

  }  

}
