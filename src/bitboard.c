#include <stdio.h>
#include <string.h>
#include "types.h"
#include "builtins.h"
#include "bitboard.h"
#include "zobrist.h"

#define MAGIC_MAX_SLOTS 4096

static uint64_t raw_attacks[107648];
uint64_t pawn_attacks[2][64];
uint64_t knight_attacks[64];
Attack bishop_attacks[64];
Attack rook_attacks[64];
uint64_t king_attacks[64];
uint64_t all_attacks[64];
uint64_t all_attacks_inc_edge[64];

static void get_blockers(Attack *a, uint64_t *blockers) {

  int bits[64];
  int num_bits = 0;

  for (int b=0; b < 64; b++) {
    if (a->mask & (1ULL << b)) {
      bits[num_bits++] = b;
    }
  }

  for (int i=0; i < a->count; i++) {

    uint64_t blocker = 0;

    for (int j=0; j < a->bits; j++) {
      if (i & (1 << j)) {
        blocker |= 1ULL << bits[j];
      }
    }

    blockers[i] = blocker;

  }
}

static void find_magics(Attack attacks[64], const char *label) {

  uint64_t tbl[MAGIC_MAX_SLOTS];
  uint32_t used[MAGIC_MAX_SLOTS];

  static uint32_t stamp = 1;
  const int verbose = 0;
  int total_tries = 0;
  int total_slots = 0;

  for (int sq=0; sq < 64; ++sq) {

    Attack *a = &attacks[sq];
    const int N = a->count;

    uint64_t blockers[MAGIC_MAX_SLOTS];
    get_blockers(a, blockers);

    memset(used, 0, (size_t)N * sizeof used[0]);

    int tries = 0;

    for (;;) {

      ++tries;

      if (++stamp == 0) {
        memset(used, 0, (size_t)N * sizeof used[0]);
        stamp = 1;
      }

      const uint64_t magic = rand64() & rand64() & rand64();

      if (popcount((a->mask * magic) >> (64 - a->bits)) < a->bits - 3) // 3 tuned
        continue;

      int fail = 0;

      for (int i=0; i < N; ++i) {

        const int idx = magic_index(blockers[i], magic, a->shift);
        const uint64_t att = a->attacks[i];

        if (used[idx] != stamp) {
          used[idx] = stamp;
          tbl[idx] = att;
        }
        else if (tbl[idx] != att) {
          fail = 1;
          break;
        }
      }

      if (!fail) {

        a->magic = magic;

        for (int i=0; i < N; ++i) {
          const int idx = magic_index(blockers[i], magic, a->shift);
          a->attacks[idx] = tbl[idx];
        }

        if (verbose) {
          printf("%s sq %2d tries %d %8d magic %llx\n",
                 label, sq, tries, N, (unsigned long long)a->magic);
        }

        total_tries += tries;
        total_slots += N;

        break;

      }
    }
  }

  if (verbose)
    printf("%s total_tries %d total_slots %d\n", label, total_tries, total_slots);

}

static void init_pawn_attacks(void) {

  for (int sq=0; sq < 64; sq++) {

    const uint64_t bb = 1ULL << sq;

    pawn_attacks[WHITE][sq] = ((bb >> 7) & NOT_A_FILE) | ((bb >> 9) & NOT_H_FILE);
    pawn_attacks[BLACK][sq] = ((bb << 7) & NOT_H_FILE) | ((bb << 9) & NOT_A_FILE);

  }
}

static void init_knight_attacks(void) {

  for (int sq=0; sq < 64; sq++) {

    int r = sq / 8, f = sq % 8;
    uint64_t bb = 0;

    int dr[8] = {-2, -1, 1, 2, 2, 1, -1, -2};
    int df[8] = {1, 2, 2, 1, -1, -2, -2, -1};

    for (int i=0; i < 8; i++) {
      int nr = r + dr[i], nf = f + df[i];
      if (nr >= 0 && nr < 8 && nf >= 0 && nf < 8)
        bb |= 1ULL << (nr * 8 + nf);
    }

    knight_attacks[sq] = bb;

  }
}

static void init_bishop_attacks(void) {

  int next_attack_index = 0;

  uint64_t blockers[MAGIC_MAX_SLOTS];

  for (int sq=0; sq < 64; sq++) {

    Attack *a = &bishop_attacks[sq];

    int rank = sq / 8;
    int file = sq % 8;
    
    a->mask = 0;
    
    for (int r = rank + 1, f = file + 1; r <= 6 && f <= 6; r++, f++)
      a->mask |= 1ULL << (r * 8 + f);
    
    for (int r = rank + 1, f = file - 1; r <= 6 && f >= 1; r++, f--)
      a->mask |= 1ULL << (r * 8 + f);
    
    for (int r = rank - 1, f = file + 1; r >= 1 && f <= 6; r--, f++)
      a->mask |= 1ULL << (r * 8 + f);
    
    for (int r = rank - 1, f = file - 1; r >= 1 && f >= 1; r--, f--)
      a->mask |= 1ULL << (r * 8 + f);
    
    a->bits = popcount(a->mask);
    a->shift = 64 - a->bits;
    a->count = 1 << a->bits;
    a->attacks = &raw_attacks[next_attack_index];

    get_blockers(a, blockers);

    for (int i=0; i < a->count; i++) {
      
      uint64_t blocker = blockers[i];
      uint64_t attack = 0;
      
      for (int r=rank+1, f=file+1; r <= 7 && f <= 7; r++, f++) {
        int s = r * 8 + f;
        attack |= 1ULL << s;
        if (blocker & (1ULL << s))
          break;
      }
      
      for (int r=rank+1, f=file-1; r <= 7 && f >= 0; r++, f--) {
        int s = r * 8 + f;
        attack |= 1ULL << s;
        if (blocker & (1ULL << s))
          break;
      }
      
      for (int r=rank-1, f=file+1; r >= 0 && f <= 7; r--, f++) {
        int s = r * 8 + f;
        attack |= 1ULL << s;
        if (blocker & (1ULL << s))
          break;
      }
      
      for (int r=rank-1, f=file-1; r >= 0 && f >= 0; r--, f--) {
        int s = r * 8 + f;
        attack |= 1ULL << s;
        if (blocker & (1ULL << s))
          break;
      }
      
      raw_attacks[next_attack_index++] = attack;
      
    }
  }

  find_magics(bishop_attacks, "B");

}

static void init_rook_attacks(void) {

  int next_attack_index = 5248;

  uint64_t blockers[MAGIC_MAX_SLOTS];

  for (int sq=0; sq < 64; sq++) {

    Attack *a = &rook_attacks[sq];

    int rank = sq / 8;
    int file = sq % 8;
    
    a->mask = 0;
    
    for (int f=file+1; f <= 6; f++)
      a->mask |= 1ULL << (rank * 8 + f);
    
    for (int f=file-1; f >= 1; f--)
      a->mask |= 1ULL << (rank * 8 + f);
    
    for (int r=rank+1; r <= 6; r++)
      a->mask |= 1ULL << (r * 8 + file);
    
    for (int r=rank-1; r >= 1; r--)
      a->mask |= 1ULL << (r * 8 + file);
    
    a->bits = popcount(a->mask);
    a->shift = 64 - a->bits;
    a->count = 1 << a->bits;
    a->attacks = &raw_attacks[next_attack_index];

    get_blockers(a, blockers);

    for (int i=0; i < a->count; i++) {
      
      uint64_t blocker = blockers[i];
      uint64_t attack = 0;
      
      for (int r=rank+1; r <= 7; r++) {
        int s = r * 8 + file;
        attack |= 1ULL << s;
        if (blocker & (1ULL << s)) {
          break;
        }
      }
      
      for (int r=rank-1; r >= 0; r--) {
        int s = r * 8 + file;
        attack |= 1ULL << s;
        if (blocker & (1ULL << s)) {
          break;
        }
      }
      
      for (int f=file+1; f <= 7; f++) {
        int s = rank * 8 + f;
        attack |= 1ULL << s;
        if (blocker & (1ULL << s)) {
          break;
        }
      }
      
      for (int f=file-1; f >= 0; f--) {
        int s = rank * 8 + f;
        attack |= 1ULL << s;
        if (blocker & (1ULL << s)) {
          break;
        }
      }
      
      raw_attacks[next_attack_index++] = attack;
      
    }
  }

  find_magics(rook_attacks, "R");

}

static void init_king_attacks(void) {

  for (int sq=0; sq < 64; sq++) {

    int r = sq / 8, f = sq % 8;
    uint64_t bb = 0;

    for (int dr=-1; dr <= 1; dr++) {
      for (int df=-1; df <= 1; df++) {
        if (dr == 0 && df == 0) continue;
        int nr = r + dr, nf = f + df;
        if (nr >= 0 && nr < 8 && nf >= 0 && nf < 8)
          bb |= 1ULL << (nr * 8 + nf);
      }
    }

    king_attacks[sq] = bb;

  }
}

static void init_all_attacks(void) {

  for (int sq=0; sq < 64; sq++) {

    int r = sq / 8;
    int f = sq % 8;
    uint64_t bb = 0;

    const int bdr[4] = { 1,  1, -1, -1};
    const int bdf[4] = { 1, -1,  1, -1};

    for (int dir = 0; dir < 4; dir++) {
      int nr = r + bdr[dir];
      int nf = f + bdf[dir];
      while (nr >= 0 && nr < 8 && nf >= 0 && nf < 8) {
        bb |= 1ULL << (nr * 8 + nf);
        nr += bdr[dir];
        nf += bdf[dir];
      }
    }

    const int rdr[4] = { 1, -1,  0,  0};
    const int rdf[4] = { 0,  0,  1, -1};

    for (int dir = 0; dir < 4; dir++) {
      int nr = r + rdr[dir];
      int nf = f + rdf[dir];
      while (nr >= 0 && nr < 8 && nf >= 0 && nf < 8) {
        bb |= 1ULL << (nr * 8 + nf);
        nr += rdr[dir];
        nf += rdf[dir];
      }
    }

    bb |= knight_attacks[sq];

    all_attacks_inc_edge[sq] = bb;
    all_attacks[sq] = knight_attacks[sq] | bishop_attacks[sq].mask | rook_attacks[sq].mask;

  }
}

void init_attacks(void) {

  init_pawn_attacks();
  init_knight_attacks();
  init_bishop_attacks();
  init_rook_attacks();
  init_king_attacks();
  init_all_attacks();

}
