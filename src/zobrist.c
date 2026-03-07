#include <stdint.h>
#include "zobrist.h"

uint64_t zob_pieces[12][64];
uint64_t zob_stm[2];
uint64_t zob_rights[16];
uint64_t zob_ep[64];

static uint64_t rand64_seed = 0xDEADBEEFCAFEBABEULL;

uint64_t rand64(void) {

  rand64_seed ^= rand64_seed >> 12;
  rand64_seed ^= rand64_seed << 25;
  rand64_seed ^= rand64_seed >> 27;

  return rand64_seed * 2685821657736338717ULL;

}

void init_zob(void) {

  for (int i=0; i < 12; i++) {
    for (int j=0; j < 64; j++) {
      zob_pieces[i][j] = rand64();
    }
  }

  zob_stm[0] = 0;
  zob_stm[1] = rand64();

  for (int i=0; i < 16; i++)
    zob_rights[i] = rand64();

  for (int i=0; i < 64; i++)
    zob_ep[i] = rand64();

  zob_ep[0]     = 0;
  zob_rights[0] = 0;

}

uint64_t rebuild_hash(const Position *pos) {

  uint64_t hash = 0;

  for (int i=0; i < 64; i++) {
    const int piece = pos->board[i];
    if (piece == EMPTY)
      continue;
    hash ^= zob_pieces[piece][i];
  }

  hash ^= zob_stm[pos->stm];
  hash ^= zob_rights[pos->rights];
  hash ^= zob_ep[pos->ep];

  return hash;

}
