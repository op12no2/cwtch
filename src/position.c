
#include <ctype.h>
#include <string.h>
#include "nodes.h"
#include "makemove.h"
#include "zobrist.h"
#include "net.h"
#include "hh.h"

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
  
  for (const char *p = rights_str; *p; ++p) {
    switch (*p) {
      case 'K': pos->rights |= WHITE_RIGHTS_KING;  break;
      case 'Q': pos->rights |= WHITE_RIGHTS_QUEEN; break;
      case 'k': pos->rights |= BLACK_RIGHTS_KING;  break;
      case 'q': pos->rights |= BLACK_RIGHTS_QUEEN; break;
    }
  }
  
  // ep

  if (ep_str[0] != '-') {

    int file = ep_str[0] - 'a';
    int rank = ep_str[1] - '1';

    pos->ep = rank * 8 + file;

  }

  // hmc
  pos->hmc = hmc;

  pos->hash = rebuild_hash(pos);
  net_slow_rebuild_accs(node);

  // initialize hh with starting position
  hh_reset();
  hh_push(pos->hash);

  // play uci moves
  for (int m = 0; m < num_uci_moves; m++) {
    play_move(node, uci_moves[m]);
    hh_push(pos->hash);
  }

}
