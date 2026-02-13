#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "types.h"
#include "tt.h"

static TT *tt = NULL;
static size_t tt_entries = 0;
static size_t tt_mask    = 0;

int new_tt(size_t megabytes) {

  if (megabytes < 1)
    megabytes = 1;

  if (megabytes > 1024)
    megabytes = 1024;

  if (tt) {
    free(tt);
    tt = NULL;
  }

  const size_t bytes = megabytes * 1024ULL * 1024ULL;

  tt_entries = bytes / sizeof(TT);
  tt_entries = 1ULL << (63 - __builtin_clzll(tt_entries));
  tt_mask    = tt_entries - 1;
  tt         = calloc(tt_entries, sizeof(TT));

  if (!tt) {
    fprintf(stderr, "info failed to allocate tt\n");
    return 1;
  }

  printf("info tt entries %zu (%zu MB)\n", tt_entries, (tt_entries * sizeof(TT)) / 1024 / 1024);

  return 0;

}

void tt_clear(void) {

  memset(tt, 0, tt_entries * sizeof(*tt));

}

int put_adjusted_score(const int ply, const int score) {

  if (score < -MATEISH)
    return score - ply;

  else if (score > MATEISH)
    return score + ply;

  else
   return score;

}

int get_adjusted_score(const int ply, const int score) {

  if (score < -MATEISH)
    return score + ply;

  else if (score > MATEISH)
    return score - ply;

  else
    return score;

}

void tt_put(const Position *pos, const int flags, const int depth, const int score, const move_t move) {

  const size_t idx = pos->hash & tt_mask;
  TT *entry = &tt[idx];

  if (entry->flags && entry->hash == pos->hash && entry->depth > depth)
    return;

  entry->depth = depth;
  entry->score = score;
  entry->move  = move;
  entry->hash  = pos->hash;
  entry->flags = flags;

}

TT *tt_get(const Position *pos) {

  const size_t idx = pos->hash & tt_mask;
  TT *entry = &tt[idx];

  if (!entry->flags)
    return NULL;

  if (entry->hash != pos->hash)
    return NULL;

  return entry;

}

void new_game(void) {

  if (!tt)
    new_tt(TT_DEFAULT_MB);

  tt_clear();

}

int is_tt_null() {

  return (int)(tt == NULL);

}
