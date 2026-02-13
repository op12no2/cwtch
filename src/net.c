#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "net.h"
#include "weights.h"

static int16_t net_h1_w[NET_I_SIZE * NET_H1_SIZE];
static int16_t net_h2_w[NET_I_SIZE * NET_H1_SIZE];  // flipped
static int16_t net_h1_b[NET_H1_SIZE];
static int32_t net_o_w [NET_H1_SIZE * 2];
static int32_t net_o_b;

static inline int32_t sqrelu(const int32_t x) {
  const int32_t y = x & ~(x >> 31);
  return y * y;
}

static int flip_index(const int index) {

  int piece  = index / (64 * NET_H1_SIZE);
  int square = (index / NET_H1_SIZE) % 64;
  int h      = index % NET_H1_SIZE;

  square ^= 56;
  piece = (piece + 6) % 12;

  return ((piece * 64) + square) * NET_H1_SIZE + h;

}

static int get_embedded_weights(int16_t **out, size_t *count_out) {

  size_t bytes = (size_t)cwtch_weights_len;

  if (bytes % sizeof(int16_t) != 0)
    return 0;

  int16_t *buf = (int16_t*)malloc(bytes);
  if (!buf)
    return 0;

  memcpy(buf, cwtch_weights, bytes);

  *out = buf;
  *count_out = bytes / sizeof(int16_t);

  return 1;

}

int init_weights(void) {

  int16_t *weights = NULL;
  size_t n = 0;

  if (!get_embedded_weights(&weights, &n)) {
    free(weights);
    fprintf(stderr, "cannot load embedded weights\n");
    return 1;
  }

  size_t offset = 0;

  for (int i=0; i < NET_I_SIZE * NET_H1_SIZE; i++) {
    net_h1_w[i]             = weights[i];  // us
    net_h2_w[flip_index(i)] = weights[i];  // them
  }

  offset += NET_I_SIZE * NET_H1_SIZE;
  for (int i=0; i < NET_H1_SIZE; i++) {
    net_h1_b[i] = weights[offset+i];
  }

  offset += NET_H1_SIZE;
  for (int i=0; i < NET_H1_SIZE * 2; i++) {
    net_o_w[i] = (int32_t)weights[offset+i];
  }

  offset += NET_H1_SIZE * 2;
  net_o_b = (int32_t)weights[offset];

  free(weights);

  return 0;

}

void net_slow_rebuild_accs(Node *node) {

  int16_t *const restrict a1 = node->accs[0];
  int16_t *const restrict a2 = node->accs[1];

  memcpy(a1, net_h1_b, sizeof net_h1_b);
  memcpy(a2, net_h1_b, sizeof net_h1_b);

  for (int sq=0; sq < 64; sq++) {

    const int piece = node->pos.board[sq];

    if (piece == EMPTY)
      continue;

    const int base = net_base(piece, sq);
    const int16_t *const restrict w1 = &net_h1_w[base];
    const int16_t *const restrict w2 = &net_h2_w[base];

    for (int h=0; h < NET_H1_SIZE; h++) {
      a1[h] += w1[h];
      a2[h] += w2[h];
    }
  }

}

int net_eval(Node *node) {

  const int stm = node->pos.stm;

  const int16_t *a1 = (stm == 0 ? node->accs[0] : node->accs[1]);
  const int16_t *a2 = (stm == 0 ? node->accs[1] : node->accs[0]);

  const int32_t *w1 = &net_o_w[0];
  const int32_t *w2 = &net_o_w[NET_H1_SIZE];

  int32_t acc = 0;

  for (int i=0; i < NET_H1_SIZE; i++) {  // autovec eval
    acc += w1[i] * sqrelu(a1[i]) + w2[i] * sqrelu(a2[i]);
  }

  acc /= NET_QA;
  acc += net_o_b;
  acc *= NET_SCALE;
  acc /= NET_QAB;

  return (int)acc;

}

void net_move(Node *node, const int piece, const int from, const int to) {

  int16_t *const restrict a1 = node->accs[0];
  int16_t *const restrict a2 = node->accs[1];

  const int b1 = net_base(piece, from);
  const int b2 = net_base(piece, to);

  const int16_t *const restrict w1_b1 = &net_h1_w[b1];
  const int16_t *const restrict w1_b2 = &net_h1_w[b2];

  const int16_t *const restrict w2_b1 = &net_h2_w[b1];
  const int16_t *const restrict w2_b2 = &net_h2_w[b2];

  for (int i=0; i < NET_H1_SIZE; i++) {
    a1[i] += w1_b2[i] - w1_b1[i];
    a2[i] += w2_b2[i] - w2_b1[i];
  }

} 

void net_capture(Node *node, const int from_piece, const int from, const int to_piece, const int to) {

  int16_t *const restrict a1 = node->accs[0];
  int16_t *const restrict a2 = node->accs[1];

  const int b1 = net_base(from_piece, from);
  const int b2 = net_base(to_piece, to);
  const int b3 = net_base(from_piece, to);

  const int16_t *const restrict w1_b1 = &net_h1_w[b1];
  const int16_t *const restrict w1_b2 = &net_h1_w[b2];
  const int16_t *const restrict w1_b3 = &net_h1_w[b3];

  const int16_t *const restrict w2_b1 = &net_h2_w[b1];
  const int16_t *const restrict w2_b2 = &net_h2_w[b2];
  const int16_t *const restrict w2_b3 = &net_h2_w[b3];

  for (int i=0; i < NET_H1_SIZE; i++) {  // autovec cap
    a1[i] += w1_b3[i] - w1_b2[i] - w1_b1[i];
    a2[i] += w2_b3[i] - w2_b2[i] - w2_b1[i];
  }

}

void net_ep_capture (Node *node, const int from_pawn, const int from, const int to, const int opp_pawn, const int opp_ep) {

  int16_t *const restrict a1 = node->accs[0];
  int16_t *const restrict a2 = node->accs[1];

  const int b1 = net_base(from_pawn, from);
  const int b2 = net_base(from_pawn, to);
  const int b3 = net_base(opp_pawn, opp_ep);

  const int16_t *const restrict w1_b1 = &net_h1_w[b1];
  const int16_t *const restrict w1_b2 = &net_h1_w[b2];
  const int16_t *const restrict w1_b3 = &net_h1_w[b3];

  const int16_t *const restrict w2_b1 = &net_h2_w[b1];
  const int16_t *const restrict w2_b2 = &net_h2_w[b2];
  const int16_t *const restrict w2_b3 = &net_h2_w[b3];

  for (int i=0; i < NET_H1_SIZE; i++) {  // autovec ep
    a1[i] += w1_b2[i] - w1_b1[i] - w1_b3[i];
    a2[i] += w2_b2[i] - w2_b1[i] - w2_b3[i];
  }

}

void net_castle (Node *node, const int king_piece, const int king_fr_sq, const int king_to_sq, const int rook_piece, const int rook_fr_sq, const int rook_to_sq) {

  int16_t *const restrict a1 = node->accs[0];
  int16_t *const restrict a2 = node->accs[1];

  const int b1 = net_base(king_piece, king_fr_sq);
  const int b2 = net_base(king_piece, king_to_sq);
  const int b3 = net_base(rook_piece, rook_fr_sq);
  const int b4 = net_base(rook_piece, rook_to_sq);

  const int16_t *const restrict w1_b1 = &net_h1_w[b1];
  const int16_t *const restrict w1_b2 = &net_h1_w[b2];
  const int16_t *const restrict w1_b3 = &net_h1_w[b3];
  const int16_t *const restrict w1_b4 = &net_h1_w[b4];

  const int16_t *const restrict w2_b1 = &net_h2_w[b1];
  const int16_t *const restrict w2_b2 = &net_h2_w[b2];
  const int16_t *const restrict w2_b3 = &net_h2_w[b3];
  const int16_t *const restrict w2_b4 = &net_h2_w[b4];

  for (int i=0; i < NET_H1_SIZE; i++) {  // autovec_castle
    a1[i] += w1_b2[i] - w1_b1[i] + w1_b4[i] - w1_b3[i];
    a2[i] += w2_b2[i] - w2_b1[i] + w2_b4[i] - w2_b3[i];
  }

}

void net_promo_push (Node *node, const int pawn_piece, const int pawn_fr, const int pawn_to, const int promote_piece) {

  int16_t *const restrict a1 = node->accs[0];
  int16_t *const restrict a2 = node->accs[1];

  const int b1 = net_base(pawn_piece, pawn_fr);
  const int b2 = net_base(promote_piece, pawn_to);

  const int16_t *const restrict w1_b1 = &net_h1_w[b1];
  const int16_t *const restrict w1_b2 = &net_h1_w[b2];

  const int16_t *const restrict w2_b1 = &net_h2_w[b1];
  const int16_t *const restrict w2_b2 = &net_h2_w[b2];

  for (int i=0; i < NET_H1_SIZE; i++) {  // autovec promo push
    a1[i] += w1_b2[i] - w1_b1[i];
    a2[i] += w2_b2[i] - w2_b1[i];
  }

}

void net_promo_capture (Node *node, const int pawn_piece, const int pawn_fr, const int pawn_to, const int capture_piece, const int promote_piece) {

  int16_t *const restrict a1 = node->accs[0];
  int16_t *const restrict a2 = node->accs[1];

  const int b1 = net_base(pawn_piece, pawn_fr);
  const int b2 = net_base(promote_piece, pawn_to);
  const int b3 = net_base(capture_piece, pawn_to);

  const int16_t *const restrict w1_b1 = &net_h1_w[b1];
  const int16_t *const restrict w1_b2 = &net_h1_w[b2];
  const int16_t *const restrict w1_b3 = &net_h1_w[b3];

  const int16_t *const restrict w2_b1 = &net_h2_w[b1];
  const int16_t *const restrict w2_b2 = &net_h2_w[b2];
  const int16_t *const restrict w2_b3 = &net_h2_w[b3];

  for (int i=0; i < NET_H1_SIZE; i++) {  // autovec promo cap
    a1[i] += w1_b2[i] - w1_b1[i] - w1_b3[i];
    a2[i] += w2_b2[i] - w2_b1[i] - w2_b3[i];
  }

}
