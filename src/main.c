#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "uci.h"
#include "bitboard.h"
#include "nodes.h"
#include "net.h"
#include "zobrist.h"
#include "search.h"
#include "see.h"
#include "input.h"

#define INPUT_BUFFER_SIZE 8192

int main(int argc, char *argv[]) {

  setbuf(stdin, NULL);
  setbuf(stdout, NULL);

  init_attacks();
  init_weights();
  init_zob();
  init_lmr();
  init_line_masks();

  // If command-line arguments provided, execute them and exit
  if (argc > 1) {
    for (int i = 1; i < argc; i++) {
      if (!uci_exec(argv[i])) {
        break;
      }
    }
    return 0;
  }

  // Otherwise run interactive loop
  char input[INPUT_BUFFER_SIZE];

  input_init();

  while (input_get(input, sizeof(input))) {
    if (!uci_exec(input)) {
      break;
    }
  }

  _Exit(0);

}
