#include "move.h"

extern int is_chess960; // Pull in our global flag from uci.c

int format_move(const move_t move, char *const buf) {

  static const char files[] = "abcdefgh";
  static const char ranks[] = "12345678";
  static const char promo[] = "nbrq";

  const int from = (move >> 6) & 0x3F;
  int to   = move & 0x3F; // Note: 'to' may be modified below for Chess960 translation

  // ==========================================
  // CHESS960 TRANSLATION LAYER
  // If it's a castle and we are NOT in 960 mode, visually fake 
  // the destination square to standard UCI notation for the GUI.
  if ((move & MOVE_FLAG_CASTLE) && !is_chess960) {
    if (to == 7) to = 6;        // H1 -> G1 (White Kingside)
    else if (to == 0) to = 2;   // A1 -> C1 (White Queenside)
    else if (to == 63) to = 62; // H8 -> G8 (Black Kingside)
    else if (to == 56) to = 58; // A8 -> C8 (Black Queenside)
  }
  // ==========================================

  buf[0] = files[from % 8];
  buf[1] = ranks[from / 8];
  buf[2] = files[to   % 8];
  buf[3] = ranks[to   / 8];
  buf[4] = '\0';
  buf[5] = '\0';

  if (move & MOVE_FLAG_PROMOTE) {
    buf[4] = promo[((move >> 12) & 7) - 1]; // actual piece type value encoded in move hence -1
    return 5;
  }

  return 4;

}