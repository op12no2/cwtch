#include "move.h"

int format_move(const move_t move, char *const buf) {

  static const char files[] = "abcdefgh";
  static const char ranks[] = "12345678";
  static const char promo[] = "nbrq";

  const int from = (move >> 6) & 0x3F;
  const int to   = move & 0x3F;

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
