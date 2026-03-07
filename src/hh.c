#include "hh.h"

static uint64_t hashes[MAX_HH];
static int game_ply;
static int root_ply;

void hh_reset(void) {
  game_ply = 0;
  root_ply = 0;
}

void hh_push(uint64_t hash) {
  if (game_ply < MAX_HH)
    hashes[game_ply++] = hash;
}

void hh_set_root(void) {
  root_ply = game_ply - 1;
}

void hh_store(int search_ply, uint64_t hash) {
  int idx = root_ply + search_ply;
  if (idx < MAX_HH)
    hashes[idx] = hash;
}

int is_draw(int search_ply, uint64_t hash, int hmc) {

  // 50-move rule
  if (hmc >= 100)
    return 1;

  // current absolute index
  int current = root_ply + search_ply;
  if (current >= MAX_HH)
    return 0;

  // can only repeat positions since last irreversible move
  int earliest = current - hmc;
  if (earliest < 0)
    earliest = 0;

  // check backwards, same side to move only (step by 2)
  int count = 0;
  for (int i = current - 2; i >= earliest; i -= 2) {
    if (hashes[i] == hash) {
      count++;
      // 2-fold within search = draw
      if (i >= root_ply)
        return 1;
      // 3-fold total = draw
      if (count >= 2)
        return 1;
    }
  }

  return 0;
}
