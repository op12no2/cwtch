#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "types.h"
#include "bitboard.h"
#include "pos.h"
#include "makemove.h"
#include "builtins.h"

void print_board(const Position *pos) {

  const char piece_chars[12] = {
    'P', 'N', 'B', 'R', 'Q', 'K',
    'p', 'n', 'b', 'r', 'q', 'k'
  };

  for (int rank=7; rank >= 0; rank--) {

    printf("%d  ", rank + 1);

    for (int file=0; file < 8; file++) {

      int sq = rank * 8 + file;
      uint64_t bb = 1ULL << sq;
      char c = '.';

      for (int i=0; i < 12; i++) {
        if (pos->all[i] & bb) {
          c = piece_chars[i];
          break;
        }
      }

      printf("%c ", c);
    }

    printf("\n");
  }

  printf("\n   a b c d e f g h\n\n");
  printf("stm=%d\n", pos->stm);
  printf("rights=%d\n", pos->rights);
  printf("ep=%d\n", pos->ep);

}

int is_attacked(const Position *pos, const int sq, const int opp) {

  const uint64_t *all = pos->all;
  const int base = colour_index(opp);

  if (all[base+KNIGHT] & knight_attacks[sq])    
    return 1;
  if (all[base+PAWN]   & pawn_attacks[opp][sq]) 
    return 1;
  if (all[base+KING]   & king_attacks[sq])      
    return 1;

  const uint64_t occ = pos->occupied;
  const uint64_t all_q = all[base+QUEEN];

  {
    const uint64_t attackers  = all[base+ROOK] | all_q;
    const Attack *a = &rook_attacks[sq];
    const uint64_t blockers = occ & a->mask;
    const uint64_t attacks  = a->attacks[magic_index(blockers, a->magic, a->shift)];
    if (attacks & attackers) 
      return 1;
  }

  {
    const uint64_t attackers  = all[base+BISHOP] | all_q;
    const Attack *a = &bishop_attacks[sq];
    const uint64_t blockers = occ & a->mask;
    const uint64_t attacks = a->attacks[magic_index(blockers, a->magic, a->shift)];
    if (attacks & attackers) 
      return 1;
  }

  return 0;

}

int is_mat_draw(const Position *pos) {

  const uint64_t *all = pos->all;

  // any pawns, rooks, or queens = not insufficient material
  if (all[WPAWN] | all[BPAWN] | all[WROOK] | all[BROOK] | all[WQUEEN] | all[BQUEEN])
    return 0;

  uint64_t knights = all[WKNIGHT] | all[BKNIGHT];
  uint64_t bishops = all[WBISHOP] | all[BBISHOP];

  // K vs K
  if (!knights && !bishops)
    return 1;

  // K+N vs K or K+B vs K (exactly one minor piece)
  uint64_t minors = knights | bishops;
  if (!(minors & (minors - 1)))
    return 1;

  // K+B vs K+B same color (no knights, exactly 2 bishops)
  if (!knights && popcount(bishops) == 2) {
    uint64_t light = 0x55AA55AA55AA55AAULL;
    uint64_t on_light = bishops & light;
    if (!on_light || on_light == bishops)
      return 1;
  }

  return 0;

}
