#ifdef __EMSCRIPTEN__

#include <stdio.h>
#include <string.h>
#include <emscripten.h>
#include "uci.h"
#include "bitboard.h"
#include "nodes.h"
#include "net.h"
#include "zobrist.h"
#include "search.h"
#include "see.h"

#define WASM_BRIDGE_BUFFER_SIZE 8192

EMSCRIPTEN_KEEPALIVE
void wasm_init(void) {
  setbuf(stdout, NULL);
  init_attacks();
  init_weights();
  init_zob();
  init_lmr();
  init_line_masks();
}

EMSCRIPTEN_KEEPALIVE
int wasm_exec(const char *input) {
  char buf[WASM_BRIDGE_BUFFER_SIZE];
  strncpy(buf, input, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';
  return uci_exec(buf);
}

#endif
