#include "pv.h"

_Thread_local move_t pv_table[MAX_PLY][MAX_PLY];
_Thread_local int pv_len[MAX_PLY];

