
#include <ctype.h>
#include <string.h>
#include "nodes.h"
#include "makemove.h"
#include "zobrist.h"
#include "net.h"
#include "hh.h"
#include "builtins.h"

void position(Node *node, const char *board_fen, const char *stm_str, const char *rights_str, const char *ep_str, int hmc, int num_uci_moves, char **uci_moves) {

  Position *pos = &node->pos;

  static const int char_to_piece[128] = {
    ['p'] = 0, ['n'] = 1, ['b'] = 2, ['r'] = 3, ['q'] = 4, ['k'] = 5,
  };

  memset(pos, 0, sizeof(Position));

  // board
  
  for (int i=0; i < 64; i++)
    pos->board[i] = EMPTY;
  
  int sq = 56;
  
  for (const char *p = board_fen; *p; ++p) {
  
    if (*p == '/') {
      sq -= 16;
    }
  
    else if (isdigit(*p)) {
      sq += *p - '0';
    }
  
    else {
  
      int colour = !!islower(*p);
      int piece  = char_to_piece[tolower(*p)];  // 0-5
      int index  = piece_index(piece, colour);  // 0-11
  
      uint64_t bb = 1ULL << sq;
  
      pos->all[index] |= bb;
      pos->occupied |= bb;
      pos->colour[colour] |= bb;
  
      pos->board[sq] = index;
  
      sq++;
  
    }
  }
  
  // stm
  
  pos->stm = (stm_str[0] == 'w') ? WHITE : BLACK;
  
  // rights
  
  pos->rights = 0;
  
  int w_king_file = bsf(pos->all[piece_index(KING, WHITE)]) % 8;
  int b_king_file = bsf(pos->all[piece_index(KING, BLACK)]) % 8;
  
  for (const char *p = rights_str; *p; ++p) {
    if (*p == 'K') pos->rights |= WHITE_RIGHTS_KING;
    else if (*p == 'Q') pos->rights |= WHITE_RIGHTS_QUEEN;
    else if (*p == 'k') pos->rights |= BLACK_RIGHTS_KING;
    else if (*p == 'q') pos->rights |= BLACK_RIGHTS_QUEEN;
    
    // X-FEN / Shredder-FEN Chess960 Castling Rights (A-H)
    else if (*p >= 'A' && *p <= 'H') {
        int r_file = *p - 'A';
        if (r_file > w_king_file) pos->rights |= WHITE_RIGHTS_KING;
        else pos->rights |= WHITE_RIGHTS_QUEEN;
    }
    else if (*p >= 'a' && *p <= 'h') {
        int r_file = *p - 'a';
        if (r_file > b_king_file) pos->rights |= BLACK_RIGHTS_KING;
        else pos->rights |= BLACK_RIGHTS_QUEEN;
    }
  }
  
  // ep

  if (ep_str[0] != '-') {

    int file = ep_str[0] - 'a';
    int rank = ep_str[1] - '1';
    int ep_sq = rank * 8 + file;

    // only set EP if an enemy pawn can actually capture
    int cap_pawn = piece_index(PAWN, pos->stm);
    int pawn_rank_sq = ep_sq + (pos->stm == WHITE ? -8 : 8);
    uint64_t pawn_bb = 1ULL << pawn_rank_sq;
    uint64_t adj = ((pawn_bb & NOT_A_FILE) >> 1) | ((pawn_bb & NOT_H_FILE) << 1);
    if (pos->all[cap_pawn] & adj)
      pos->ep = ep_sq;

  }

  extern int is_chess960; // Pull in the global flag from uci.c
  pos->is_chess960 = is_chess960;
  
  // Initialize rook squares to default A/H files just in case
  pos->castling_rook_sq[WHITE][0] = H1; // Kingside
  pos->castling_rook_sq[WHITE][1] = A1; // Queenside
  pos->castling_rook_sq[BLACK][0] = H8;
  pos->castling_rook_sq[BLACK][1] = A8;
  
  if (is_chess960) {
      // In Chess960, we have to find where the Rooks actually spawned!
      int w_king_sq = bsf(pos->all[piece_index(KING, WHITE)]);
      int b_king_sq = bsf(pos->all[piece_index(KING, BLACK)]);
  
      // Find White Rooks
      uint64_t w_rooks = pos->all[piece_index(ROOK, WHITE)];
      if (w_rooks) {
          int r1 = bsf(w_rooks); 
          // Note: ensure you have an msb/bsr function defined in builtins.h for this!
          int r2 = msb(w_rooks); 
          
          // The rook to the right (higher file) is the kingside rook
          pos->castling_rook_sq[WHITE][0] = (r1 > w_king_sq) ? r1 : r2; 
          // The rook to the left (lower file) is the queenside rook
          pos->castling_rook_sq[WHITE][1] = (r1 < w_king_sq) ? r1 : r2;
      }
  
      // Find Black Rooks
      uint64_t b_rooks = pos->all[piece_index(ROOK, BLACK)];
      if (b_rooks) {
          int r1 = bsf(b_rooks);
          int r2 = msb(b_rooks);
          
          pos->castling_rook_sq[BLACK][0] = (r1 > b_king_sq) ? r1 : r2;
          pos->castling_rook_sq[BLACK][1] = (r1 < b_king_sq) ? r1 : r2;
      }
  }

  // hmc
  pos->hmc = hmc;

  pos->hash = rebuild_hash(pos);
  net_slow_rebuild_accs(node);
  node->accs_dirty = 0;
  node->prev_piece = EMPTY; /* Root has no previous move */
  node->prev_to = 0;

  // initialize hh with starting position
  hh_reset();
  hh_push(pos->hash);

  // play uci moves
  for (int m = 0; m < num_uci_moves; m++) {
    play_move(node, uci_moves[m]);
    hh_push(pos->hash);
  }

}
