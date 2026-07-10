#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "net.h"
#include "builtins.h"

#define INCBIN_PREFIX cwtch_
#define INCBIN_STYLE INCBIN_STYLE_SNAKE
#include "incbin.h"
INCBIN(weights, NET_WEIGHTS_PATH);

#define NET_BUCKET_STRIDE (NET_I_SIZE * NET_H1_SIZE)
#define NET_L0_SIZE (NET_I_BUCKETS * NET_BUCKET_STRIDE)

// bullet pads quantised.bin to the next 64 byte boundary
#define NET_EXACT_BYTES ((size_t)(NET_L0_SIZE + NET_H1_SIZE + NET_O_BUCKETS * NET_H1_SIZE * 2 + NET_O_BUCKETS) * sizeof(int16_t))
#define NET_FILE_BYTES ((NET_EXACT_BYTES + 63) & ~(size_t)63)

// output bucket = (piece count - 2) / NET_O_DIV, matching bullet MaterialCount
#define NET_O_DIV ((32 + NET_O_BUCKETS - 1) / NET_O_BUCKETS)

static int16_t net_h1_w[NET_L0_SIZE];
static int16_t net_h1_b[NET_H1_SIZE];
static int32_t net_o_w [NET_O_BUCKETS * NET_H1_SIZE * 2];
static int32_t net_o_b [NET_O_BUCKETS];

// king bucket layout in white pov; mirrored nets use files a-d only
static const uint8_t net_bucket_map[64] = {
  0, 0, 1, 1, 1, 1, 0, 0,
  2, 2, 2, 2, 2, 2, 2, 2,
  3, 3, 3, 3, 3, 3, 3, 3,
  3, 3, 3, 3, 3, 3, 3, 3,
  3, 3, 3, 3, 3, 3, 3, 3,
  3, 3, 3, 3, 3, 3, 3, 3,
  3, 3, 3, 3, 3, 3, 3, 3,
  3, 3, 3, 3, 3, 3, 3, 3,
};

// finny cache - one accumulator per view, refreshed by board diff
typedef struct {
  _Alignas(64) int16_t acc[NET_H1_SIZE];
  uint64_t all[12];
} FinnyEntry;

static FinnyEntry finny[2][NET_I_BUCKETS][2];  // [perspective][bucket][mirror]

// per-perspective view of the feature space
typedef struct {
  int hm;      // horizontal mirror mask, 0 or 7
  int bucket;
  int off;     // bucket offset into l0 weights
} NetView;

static inline int net_hm(const int sq) {
  return NET_MIRRORED && (sq & 4) ? 7 : 0;  // sq & 4 == file > 3
}

static inline int net_bucket_off(const int sq, const int hm) {
  return net_bucket_map[sq ^ hm] * NET_BUCKET_STRIDE;
}

static void get_views(const Position *pos, NetView v[2]) {

  const int wksq = bsf(pos->all[WKING]);
  const int bksq = bsf(pos->all[BKING]) ^ 56;  // black king in white pov

  v[0].hm = net_hm(wksq);
  v[0].bucket = net_bucket_map[wksq ^ v[0].hm];
  v[0].off = v[0].bucket * NET_BUCKET_STRIDE;
  v[1].hm = net_hm(bksq);
  v[1].bucket = net_bucket_map[bksq ^ v[1].hm];
  v[1].off = v[1].bucket * NET_BUCKET_STRIDE;

}

static inline int net_flip_piece(const int piece) {
  return piece < 6 ? piece + 6 : piece - 6;  // swap colours
}

static inline const int16_t *net_row_w(const int piece, const int sq, const NetView *v) {
  return &net_h1_w[v->off + net_base(piece, sq ^ v->hm)];
}

// them view reads the same table with colours swapped and board flipped
static inline const int16_t *net_row_b(const int piece, const int sq, const NetView *v) {
  return &net_h1_w[v->off + net_base(net_flip_piece(piece), sq ^ 56 ^ v->hm)];
}

static inline const int16_t *net_row_p(const int p, const int piece, const int sq, const NetView *v) {
  return p == 0 ? net_row_w(piece, sq, v) : net_row_b(piece, sq, v);
}

static inline int32_t screlu(const int32_t x) {
  const int32_t y = x < 0 ? 0 : (x > NET_QA ? NET_QA : x);
  return y * y;
}

static inline int32_t sqrelu(const int32_t x) {
  const int32_t y = x & ~(x >> 31);
  return y * y;
}

static void unpack_weights(const int16_t *weights) {

  size_t offset = 0;

  for (int i=0; i < NET_L0_SIZE; i++) {
    net_h1_w[i] = weights[i];
  }

  offset += NET_L0_SIZE;
  for (int i=0; i < NET_H1_SIZE; i++) {
    net_h1_b[i] = weights[offset+i];
  }

  offset += NET_H1_SIZE;
  for (int i=0; i < NET_O_BUCKETS * NET_H1_SIZE * 2; i++) {
    net_o_w[i] = (int32_t)weights[offset+i];
  }

  offset += NET_O_BUCKETS * NET_H1_SIZE * 2;
  for (int i=0; i < NET_O_BUCKETS; i++) {
    net_o_b[i] = (int32_t)weights[offset+i];
  }

  // weights changed so reset the finny cache to empty boards
  for (int p=0; p < 2; p++) {
    for (int b=0; b < NET_I_BUCKETS; b++) {
      for (int m=0; m < 2; m++) {
        memcpy(finny[p][b][m].acc, net_h1_b, sizeof net_h1_b);
        memset(finny[p][b][m].all, 0, sizeof finny[p][b][m].all);
      }
    }
  }

}

int init_weights(void) {

  size_t bytes = (size_t)cwtch_weights_size;

  if (bytes != NET_FILE_BYTES) {
    printf("info string embedded weights %llu bytes, expected %llu\n", (unsigned long long)bytes, (unsigned long long)NET_FILE_BYTES);
    return 1;
  }

  unpack_weights((const int16_t *)cwtch_weights_data);
  return 0;

}

int load_weights_from_file(const char *path) {

  FILE *f = fopen(path, "rb");
  if (!f) {
    printf("info string cannot open %s\n", path);
    return 1;
  }

  fseek(f, 0, SEEK_END);
  long bytes = ftell(f);
  fseek(f, 0, SEEK_SET);

  if (bytes <= 0 || (size_t)bytes != NET_FILE_BYTES) {
    printf("info string %s is %llu bytes, expected %llu\n", path, (unsigned long long)bytes, (unsigned long long)NET_FILE_BYTES);
    fclose(f);
    return 1;
  }

  int16_t *buf = (int16_t *)malloc(bytes);
  if (!buf) {
    printf("info string allocation failed\n");
    fclose(f);
    return 1;
  }

  if (fread(buf, 1, bytes, f) != (size_t)bytes) {
    printf("info string read error\n");
    free(buf);
    fclose(f);
    return 1;
  }

  fclose(f);
  unpack_weights(buf);
  free(buf);

  printf("info string loaded %s\n", path);
  return 0;

}

void lazy_update_accs(Node *node) {

  if (!node->accs_dirty)
    return;

  // updaters write child = parent + delta in one pass, no memcpy
  update_accs(node, (node - 1)->accs);
  node->accs_dirty = 0;

}

static void net_move(Node *node, const int16_t (*src)[NET_H1_SIZE], const NetView v[2]) {

  const uint8_t *const a = node->net_deferred.args;

  const int16_t *const restrict s1 = src[0];
  const int16_t *const restrict s2 = src[1];
  int16_t *const restrict a1 = node->accs[0];
  int16_t *const restrict a2 = node->accs[1];

  const int16_t *const restrict w1_b1 = net_row_w(a[0], a[1], &v[0]);
  const int16_t *const restrict w1_b2 = net_row_w(a[0], a[2], &v[0]);

  const int16_t *const restrict w2_b1 = net_row_b(a[0], a[1], &v[1]);
  const int16_t *const restrict w2_b2 = net_row_b(a[0], a[2], &v[1]);

  for (int i=0; i < NET_H1_SIZE; i++) {  // autovec move
    a1[i] = s1[i] + w1_b2[i] - w1_b1[i];
    a2[i] = s2[i] + w2_b2[i] - w2_b1[i];
  }

}

static void net_capture(Node *node, const int16_t (*src)[NET_H1_SIZE], const NetView v[2]) {

  const uint8_t *const a = node->net_deferred.args;

  const int16_t *const restrict s1 = src[0];
  const int16_t *const restrict s2 = src[1];
  int16_t *const restrict a1 = node->accs[0];
  int16_t *const restrict a2 = node->accs[1];

  const int16_t *const restrict w1_b1 = net_row_w(a[0], a[1], &v[0]);
  const int16_t *const restrict w1_b2 = net_row_w(a[2], a[3], &v[0]);
  const int16_t *const restrict w1_b3 = net_row_w(a[0], a[3], &v[0]);

  const int16_t *const restrict w2_b1 = net_row_b(a[0], a[1], &v[1]);
  const int16_t *const restrict w2_b2 = net_row_b(a[2], a[3], &v[1]);
  const int16_t *const restrict w2_b3 = net_row_b(a[0], a[3], &v[1]);

  for (int i=0; i < NET_H1_SIZE; i++) {  // autovec cap
    a1[i] = s1[i] + w1_b3[i] - w1_b2[i] - w1_b1[i];
    a2[i] = s2[i] + w2_b3[i] - w2_b2[i] - w2_b1[i];
  }

}

static void net_ep_capture(Node *node, const int16_t (*src)[NET_H1_SIZE], const NetView v[2]) {

  const uint8_t *const a = node->net_deferred.args;

  const int16_t *const restrict s1 = src[0];
  const int16_t *const restrict s2 = src[1];
  int16_t *const restrict a1 = node->accs[0];
  int16_t *const restrict a2 = node->accs[1];

  const int16_t *const restrict w1_b1 = net_row_w(a[0], a[1], &v[0]);
  const int16_t *const restrict w1_b2 = net_row_w(a[0], a[2], &v[0]);
  const int16_t *const restrict w1_b3 = net_row_w(a[3], a[4], &v[0]);

  const int16_t *const restrict w2_b1 = net_row_b(a[0], a[1], &v[1]);
  const int16_t *const restrict w2_b2 = net_row_b(a[0], a[2], &v[1]);
  const int16_t *const restrict w2_b3 = net_row_b(a[3], a[4], &v[1]);

  for (int i=0; i < NET_H1_SIZE; i++) {  // autovec ep
    a1[i] = s1[i] + w1_b2[i] - w1_b1[i] - w1_b3[i];
    a2[i] = s2[i] + w2_b2[i] - w2_b1[i] - w2_b3[i];
  }

}

static void net_castle(Node *node, const int16_t (*src)[NET_H1_SIZE], const NetView v[2]) {

  const uint8_t *const a = node->net_deferred.args;

  const int16_t *const restrict s1 = src[0];
  const int16_t *const restrict s2 = src[1];
  int16_t *const restrict a1 = node->accs[0];
  int16_t *const restrict a2 = node->accs[1];

  const int16_t *const restrict w1_b1 = net_row_w(a[0], a[1], &v[0]);
  const int16_t *const restrict w1_b2 = net_row_w(a[0], a[2], &v[0]);
  const int16_t *const restrict w1_b3 = net_row_w(a[3], a[4], &v[0]);
  const int16_t *const restrict w1_b4 = net_row_w(a[3], a[5], &v[0]);

  const int16_t *const restrict w2_b1 = net_row_b(a[0], a[1], &v[1]);
  const int16_t *const restrict w2_b2 = net_row_b(a[0], a[2], &v[1]);
  const int16_t *const restrict w2_b3 = net_row_b(a[3], a[4], &v[1]);
  const int16_t *const restrict w2_b4 = net_row_b(a[3], a[5], &v[1]);

  for (int i=0; i < NET_H1_SIZE; i++) {  // autovec castle
    a1[i] = s1[i] + w1_b2[i] - w1_b1[i] + w1_b4[i] - w1_b3[i];
    a2[i] = s2[i] + w2_b2[i] - w2_b1[i] + w2_b4[i] - w2_b3[i];
  }

}

static void net_promo_push(Node *node, const int16_t (*src)[NET_H1_SIZE], const NetView v[2]) {

  const uint8_t *const a = node->net_deferred.args;

  const int16_t *const restrict s1 = src[0];
  const int16_t *const restrict s2 = src[1];
  int16_t *const restrict a1 = node->accs[0];
  int16_t *const restrict a2 = node->accs[1];

  const int16_t *const restrict w1_b1 = net_row_w(a[0], a[1], &v[0]);
  const int16_t *const restrict w1_b2 = net_row_w(a[3], a[2], &v[0]);

  const int16_t *const restrict w2_b1 = net_row_b(a[0], a[1], &v[1]);
  const int16_t *const restrict w2_b2 = net_row_b(a[3], a[2], &v[1]);

  for (int i=0; i < NET_H1_SIZE; i++) {  // autovec promo push
    a1[i] = s1[i] + w1_b2[i] - w1_b1[i];
    a2[i] = s2[i] + w2_b2[i] - w2_b1[i];
  }

}

static void net_promo_capture(Node *node, const int16_t (*src)[NET_H1_SIZE], const NetView v[2]) {

  const uint8_t *const a = node->net_deferred.args;

  const int16_t *const restrict s1 = src[0];
  const int16_t *const restrict s2 = src[1];
  int16_t *const restrict a1 = node->accs[0];
  int16_t *const restrict a2 = node->accs[1];

  const int16_t *const restrict w1_b1 = net_row_w(a[0], a[1], &v[0]);
  const int16_t *const restrict w1_b2 = net_row_w(a[4], a[2], &v[0]);
  const int16_t *const restrict w1_b3 = net_row_w(a[3], a[2], &v[0]);

  const int16_t *const restrict w2_b1 = net_row_b(a[0], a[1], &v[1]);
  const int16_t *const restrict w2_b2 = net_row_b(a[4], a[2], &v[1]);
  const int16_t *const restrict w2_b3 = net_row_b(a[3], a[2], &v[1]);

  for (int i=0; i < NET_H1_SIZE; i++) {  // autovec promo cap
    a1[i] = s1[i] + w1_b2[i] - w1_b1[i] - w1_b3[i];
    a2[i] = s2[i] + w2_b2[i] - w2_b1[i] - w2_b3[i];
  }

}

// rebuild one accumulator from the board using its view, bypassing the finny cache
static void net_rebuild_acc(Node *node, const int p, const NetView *v) {

  int16_t *const restrict acc = node->accs[p];
  const uint8_t *const board = node->pos.board;

  memcpy(acc, net_h1_b, sizeof net_h1_b);

  for (int sq=0; sq < 64; sq++) {

    const int piece = board[sq];

    if (piece == EMPTY)
      continue;

    const int16_t *const restrict w = net_row_p(p, piece, sq, v);

    for (int h=0; h < NET_H1_SIZE; h++) {
      acc[h] += w[h];
    }
  }

}

// refresh one accumulator by applying the board diff to its finny cache entry
static void net_refresh_acc(Node *node, const int p, const NetView *v) {

  FinnyEntry *const e = &finny[p][v->bucket][v->hm ? 1 : 0];
  const uint64_t *const all = node->pos.all;
  int16_t *const restrict acc = e->acc;

  for (int piece=0; piece < 12; piece++) {

    uint64_t add = all[piece] & ~e->all[piece];
    uint64_t sub = e->all[piece] & ~all[piece];

    while (add) {
      const int16_t *const restrict w = net_row_p(p, piece, bsf(add), v);
      for (int h=0; h < NET_H1_SIZE; h++) {  // autovec finny add
        acc[h] += w[h];
      }
      add &= add - 1;
    }

    while (sub) {
      const int16_t *const restrict w = net_row_p(p, piece, bsf(sub), v);
      for (int h=0; h < NET_H1_SIZE; h++) {  // autovec finny sub
        acc[h] -= w[h];
      }
      sub &= sub - 1;
    }

    e->all[piece] = all[piece];
  }

  memcpy(node->accs[p], e->acc, sizeof e->acc);

}

// apply the deferred op to one accumulator when the other was refreshed; king ops only
static void net_delta_acc(Node *node, const int16_t *const restrict src, const int p, const NetView *v) {

  const uint8_t *const a = node->net_deferred.args;
  int16_t *const restrict acc = node->accs[p];

  switch (node->net_deferred.type) {

    case NET_OP_MOVE: {
      const int16_t *const restrict w_b1 = net_row_p(p, a[0], a[1], v);
      const int16_t *const restrict w_b2 = net_row_p(p, a[0], a[2], v);
      for (int i=0; i < NET_H1_SIZE; i++) {
        acc[i] = src[i] + w_b2[i] - w_b1[i];
      }
      break;
    }

    case NET_OP_CAPTURE: {
      const int16_t *const restrict w_b1 = net_row_p(p, a[0], a[1], v);
      const int16_t *const restrict w_b2 = net_row_p(p, a[2], a[3], v);
      const int16_t *const restrict w_b3 = net_row_p(p, a[0], a[3], v);
      for (int i=0; i < NET_H1_SIZE; i++) {
        acc[i] = src[i] + w_b3[i] - w_b2[i] - w_b1[i];
      }
      break;
    }

    case NET_OP_CASTLE: {
      const int16_t *const restrict w_b1 = net_row_p(p, a[0], a[1], v);
      const int16_t *const restrict w_b2 = net_row_p(p, a[0], a[2], v);
      const int16_t *const restrict w_b3 = net_row_p(p, a[3], a[4], v);
      const int16_t *const restrict w_b4 = net_row_p(p, a[3], a[5], v);
      for (int i=0; i < NET_H1_SIZE; i++) {
        acc[i] = src[i] + w_b2[i] - w_b1[i] + w_b4[i] - w_b3[i];
      }
      break;
    }

    default:
      printf("net_delta_acc bad op %d\n", node->net_deferred.type);
      exit(1);
  }

}

void update_accs(Node *node, const int16_t (*src)[NET_H1_SIZE]) {

  NetView v[2];
  get_views(&node->pos, v);

  const uint8_t *const a = node->net_deferred.args;

  // king crossing a bucket or mirror boundary refreshes that perspective
  if (piece_type(a[0]) == KING) {

    const int p = piece_colour(a[0]);
    const int from = p == WHITE ? a[1] : a[1] ^ 56;
    const int hm = net_hm(from);

    if (hm != v[p].hm || net_bucket_off(from, hm) != v[p].off) {
      net_refresh_acc(node, p, &v[p]);
      net_delta_acc(node, src[p ^ 1], p ^ 1, &v[p ^ 1]);
      return;
    }
  }

  switch (node->net_deferred.type) {
    case NET_OP_MOVE:
      net_move(node, src, v);
      break;
    case NET_OP_CAPTURE:
      net_capture(node, src, v);
      break;
    case NET_OP_EP_CAPTURE:
      net_ep_capture(node, src, v);
      break;
    case NET_OP_CASTLE:
      net_castle(node, src, v);
      break;
    case NET_OP_PROMO_PUSH:
      net_promo_push(node, src, v);
      break;
    case NET_OP_PROMO_CAPTURE:
      net_promo_capture(node, src, v);
      break;
  }

}

void net_slow_rebuild_accs(Node *node) {

  NetView v[2];
  get_views(&node->pos, v);

  net_rebuild_acc(node, 0, &v[0]);
  net_rebuild_acc(node, 1, &v[1]);

}

int net_eval(Node *node) {

  const int stm = node->pos.stm;
  const int ob = (popcount(node->pos.occupied) - 2) / NET_O_DIV;

  const int16_t *a1 = (stm == 0 ? node->accs[0] : node->accs[1]);
  const int16_t *a2 = (stm == 0 ? node->accs[1] : node->accs[0]);

  const int32_t *w1 = &net_o_w[ob * NET_H1_SIZE * 2];
  const int32_t *w2 = w1 + NET_H1_SIZE;

  int32_t acc = 0;

  for (int i=0; i < NET_H1_SIZE; i++) {  // autovec eval
    acc += w1[i] * sqrelu(a1[i]) + w2[i] * sqrelu(a2[i]);
  }

  acc /= NET_QA;
  acc += net_o_b[ob];
  acc *= NET_SCALE;
  acc /= NET_QAB;

  return (int)acc;

}
