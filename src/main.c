#include <stdio.h>
#include <string.h>
#include <time.h>
#include "uci.h"
#include "bitboard.h"
#include "nodes.h"
#include "net.h"
#include "zobrist.h"
#include "search.h"
#include "see.h"

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

  while (fgets(input, sizeof(input), stdin)) {

    size_t len = strlen(input);
    if (len > 0 && input[len - 1] == '\n') {
      input[len - 1] = '\0';
    }

    if (!uci_exec(input)) {
      break;
    }
  }

  return 0;

}
