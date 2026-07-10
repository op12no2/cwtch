#ifndef SEE_H
#define SEE_H

#include "pos.h"
#include "move.h"

int see_ge(const Position *const pos, const move_t move, int threshold);
void init_line_masks(void);

#endif
