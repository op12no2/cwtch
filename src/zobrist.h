#ifndef ZOBRIST_H
#define ZOBRIST_H

#include <stdint.h>
#include "pos.h"

uint64_t rand64(void);
void init_zob(void);
uint64_t rebuild_hash(const Position *pos);

extern uint64_t zob_pieces[12][64];
extern uint64_t zob_stm[2];
extern uint64_t zob_rights[16];
extern uint64_t zob_ep[64];

#endif
