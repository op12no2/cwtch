#ifndef PERFT_H
#define PERFT_H

#include <stdint.h>

uint64_t perft(const int depth, const int ply);
void perft_tests(int max_depth);

#endif
