
#define VERSION "4"
#define BUILD "43"

/*{{{  includes*/

#define _POSIX_C_SOURCE 200809L

#include <stdalign.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <inttypes.h> // PRIu64 PRId64 PRIx64
#include <ctype.h>
#include <time.h>
#include <sys/time.h>
#include <assert.h>
#include <unistd.h>
#include <libgen.h>
#include <pthread.h>
#include <stdatomic.h>

#include "weights.h"

/*}}}*/
/*{{{  macros*/

#define max(a,b) (( (a) > (b) ) ? (a) : (b))
#define min(a,b) (( (a) < (b) ) ? (a) : (b))

#define IS_ALIGNED(p, a) ((((uintptr_t)(p)) & ((a) - 1)) == 0)
#define ASSERT_ALIGNED64(p) assert(IS_ALIGNED((p), 64))

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
  #include <stdalign.h>
  #define ALIGN64 alignas(64)
#elif defined(_MSC_VER)
  #define ALIGN64 __declspec(align(64))
#else
  #define ALIGN64 __attribute__((aligned(64)))
#endif

#if defined(_MSC_VER) && !defined(__clang__)
  #define RESTRICT __restrict
#else
  #define RESTRICT restrict
#endif

#define move_t uint16_t

/*}}}*/
/*{{{  constants*/

#define MAX_PLY 64
#define SAFE_PLY (MAX_PLY - 2)
#define MAX_MOVES 256

#define INF  30000
#define MATE 29000
#define MATE_LIMIT 28000

#define MAGIC_MAX_SLOTS 4096

#define NET_H1_SIZE 384
#define NET_I_SIZE 768
#define NET_QA 255
#define NET_QB 64
#define NET_QAB (NET_QA * NET_QB)
#define NET_SCALE 400

#define UCI_LINE_LENGTH 8192
#define UCI_TOKENS      1024

#define WHITE_RIGHTS_KING  1
#define WHITE_RIGHTS_QUEEN 2
#define BLACK_RIGHTS_KING  4
#define BLACK_RIGHTS_QUEEN 8
#define ALL_RIGHTS         15

#define EMPTY 255

#define RANK_1 0x00000000000000FFULL
#define RANK_2 0x000000000000FF00ULL
#define RANK_3 0x0000000000FF0000ULL
#define RANK_6 0x0000FF0000000000ULL
#define RANK_7 0x00FF000000000000ULL
#define RANK_8 0xFF00000000000000ULL
#define RANK_PROMO (RANK_1 | RANK_8)

#define NOT_A_FILE 0xfefefefefefefefeULL
#define NOT_H_FILE 0x7f7f7f7f7f7f7f7fULL

enum {WHITE, BLACK};
enum {PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING};
enum {WPAWN, WKNIGHT, WBISHOP, WROOK, WQUEEN, WKING, BPAWN, BKNIGHT, BBISHOP, BROOK, BQUEEN, BKING};

#define MT_NON_PAWN_PUSH (0 << 12)
#define MT_PAWN_PUSH (1 << 12)
#define MT_CASTLE (2 << 12)
#define MT_EP_PUSH (3 << 12)
#define MT_PROMO_PUSH_N (4 << 12)
#define MT_PROMO_PUSH_B (5 << 12)
#define MT_PROMO_PUSH_R (6 << 12)
#define MT_PROMO_PUSH_Q (7 << 12)
#define MT_CAPTURE (8 << 12)
#define MT_EP_CAPTURE (9 << 12)
#define MT_PAWN_CAPTURE (10 << 12)
#define MT_SPARE (11 << 12)
#define MT_PROMO_CAPTURE_N (12 << 12)
#define MT_PROMO_CAPTURE_B (13 << 12)
#define MT_PROMO_CAPTURE_R (14 << 12)
#define MT_PROMO_CAPTURE_Q (15 << 12)

enum {
  A1, B1, C1, D1, E1, F1, G1, H1,
  A2, B2, C2, D2, E2, F2, G2, H2,
  A3, B3, C3, D3, E3, F3, G3, H3,
  A4, B4, C4, D4, E4, F4, G4, H4,
  A5, B5, C5, D5, E5, F5, G5, H5,
  A6, B6, C6, D6, E6, F6, G6, H6,
  A7, B7, C7, D7, E7, F7, G7, H7,
  A8, B8, C8, D8, E8, F8, G8, H8
};

#define MAX_HISTORY 16384

#define TT_EXACT 1
#define TT_ALPHA 2
#define TT_BETA 4
#define TT_DEFAULT 16

/*}}}*/
/*{{{  structs*/

/*{{{  Position*/

typedef struct {

  uint64_t all[12];
  uint64_t colour[2];
  uint64_t occupied;

  uint8_t board[64];

  uint8_t stm;
  uint8_t rights;
  uint8_t ep;
  uint8_t hmc;

  uint64_t hash;

  uint8_t _pad[56];

} Position;

/*}}}*/
/*{{{  Node*/

typedef struct {

  Position pos;

  ALIGN64 int32_t acc1[NET_H1_SIZE];  // us acc
  ALIGN64 int32_t acc2[NET_H1_SIZE];  // them acc
  ALIGN64 move_t moves[MAX_MOVES];
  ALIGN64 int16_t ranks[MAX_MOVES];
  ALIGN64 move_t pv[MAX_PLY];

  uint8_t pv_len;
  uint8_t num_moves;
  uint8_t next_move;
  move_t tt_move;
  move_t killer1;
  move_t killer2;
  int16_t ev;
  uint8_t in_check;
  uint8_t stage;

} Node;

/*}}}*/
/*{{{  Attack*/

typedef struct {

  int bits;
  int count;
  int shift;

  uint64_t mask;
  uint64_t magic;

  uint64_t *RESTRICT attacks;

  uint8_t pad[24];

} Attack;

/*}}}*/
/*{{{  TimeControl*/

typedef struct {

  uint64_t start_time;
  uint64_t finish_time;
  int max_depth;
  uint64_t max_nodes;
  _Atomic uint8_t finished;
  uint64_t nodes;
  move_t bm;
  int bs;

} TimeControl;

/*}}}*/
/*{{{  Perft*/

typedef struct {

  const char *fen;
  const char *stm;
  const char *rights;
  const char *ep;
  int depth;
  uint64_t expected;
  const char *label;

} Perft;

/*}}}*/
/*{{{  Bench*/

typedef struct {

  const char *fen;
  const char *stm;
  const char *rights;
  const char *ep;

} Bench;

/*}}}*/
/*{{{  Mover*/

typedef void (*Mover)(Position *const pos, const move_t move);

/*}}}*/
/*{{{  Lazy*/

typedef struct {

  void (*post_func)(Position *const pos);
  void (*net_func)(Node *const node);
  int arg0;
  int arg1;
  int arg2;
  int arg3;
  int arg4;
  int arg5;

} Lazy;

/*}}}*/
/*{{{  TT*/

typedef struct {

  uint64_t hash;
  move_t move;
  uint8_t flags;
  uint8_t depth;
  int16_t score;
  int16_t ev;

} TT;

/*}}}*/
/*{{{  HashHistory*/

typedef struct {

  uint64_t hash[1024];
  int num_uci_moves;

} HashHistory;

/*}}}*/

/*}}}*/
/*{{{  data*/

static Mover move_funcs[16];

static uint64_t rand64_seed = 0xDEADBEEFCAFEBABEULL;

static ALIGN64 uint64_t raw_attacks[107648];
static ALIGN64 uint64_t pawn_attacks[2][64];
static ALIGN64 uint64_t knight_attacks[64];
static ALIGN64 uint64_t king_attacks[64];
static ALIGN64 uint64_t all_attacks[64];
static ALIGN64 uint64_t all_attacks_inc_edge[64];

static ALIGN64 Attack bishop_attacks[64];
static ALIGN64 Attack rook_attacks[64];

static ALIGN64 Node ss[MAX_PLY];

static TimeControl tc;
static Lazy lazy;

static ALIGN64 HashHistory hh;

static ALIGN64 TT *tt    = NULL;
static size_t tt_entries = 0;
static size_t tt_mask    = 0;

static const uint8_t lut_see[16]     = {0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0};
static const uint8_t lut_prune[16]   = {1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static const uint8_t lut_history[16] = {1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1};
static const uint8_t lut_killer[16]  = {1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0};
static const uint8_t lut_quiet[16]   = {1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0};
static const uint8_t lut_noisy[16]   = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1};

static ALIGN64 uint64_t zob_pieces[12 * 64];
static ALIGN64 uint64_t zob_stm;
static ALIGN64 uint64_t zob_rights[16];
static ALIGN64 uint64_t zob_ep[64];

static ALIGN64 int32_t net_h1_w[NET_I_SIZE * NET_H1_SIZE];
static ALIGN64 int32_t net_h2_w[NET_I_SIZE * NET_H1_SIZE];  // flipped
static ALIGN64 int32_t net_h1_b[NET_H1_SIZE];
static ALIGN64 int32_t net_o_w [NET_H1_SIZE * 2];
static ALIGN64 int32_t net_o_b;

static const int see_values[12] = {100, 325, 325, 500, 1000, 0, 100, 325, 325, 500, 1000, 0};
static const int orth_offset[2] = {8, -8};
static const int rook_to[64]    = {[G1] = F1, [C1] = D1, [G8] = F8, [C8] = D8};
static const int rook_from[64]  = {[G1] = H1, [C1] = A1, [G8] = H8, [C8] = A8};

static int rights_masks[64];

static ALIGN64 int16_t piece_to_history[12][64];

static ALIGN64 uint64_t rank_mask[64];
static ALIGN64 uint64_t file_mask[64];
static ALIGN64 uint64_t diag_mask[64];
static ALIGN64 uint64_t anti_mask[64];

/*{{{  perft data*/

static const Perft perft_data[] = {

  {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR",             "w", "KQkq", "-",  0, 1,         "cpw-pos1-0"},
  {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR",             "w", "KQkq", "-",  1, 20,        "cpw-pos1-1"},
  {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR",             "w", "KQkq", "-",  2, 400,       "cpw-pos1-2"},
  {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR",             "w", "KQkq", "-",  3, 8902,      "cpw-pos1-3"},
  {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR",             "w", "KQkq", "-",  4, 197281,    "cpw-pos1-4"},
  {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR",             "w", "KQkq", "-",  5, 4865609,   "cpw-pos1-5"},
  {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR",             "w", "KQkq", "-",  6, 119060324, "cpw-pos1-6"},
  {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR",             "w", "KQkq", "-",  7, 3195901860,"cpw-pos1-6"},
  {"4k3/8/8/8/8/8/R7/R3K2R",                                  "w", "Q",    "-",  3, 4729,      "castling-2"},
  {"4k3/8/8/8/8/8/R7/R3K2R",                                  "w", "K",    "-",  3, 4686,      "castling-3"},
  {"4k3/8/8/8/8/8/R7/R3K2R",                                  "w", "-",    "-",  3, 4522,      "castling-4"},
  {"r3k2r/r7/8/8/8/8/8/4K3",                                  "b", "kq",   "-",  3, 4893,      "castling-5"},
  {"r3k2r/r7/8/8/8/8/8/4K3",                                  "b", "q",    "-",  3, 4729,      "castling-6"},
  {"r3k2r/r7/8/8/8/8/8/4K3",                                  "b", "k",    "-",  3, 4686,      "castling-7"},
  {"r3k2r/r7/8/8/8/8/8/4K3",                                  "b", "-",    "-",  3, 4522,      "castling-8"},
  {"rnbqkb1r/pp1p1ppp/2p5/4P3/2B5/8/PPP1NnPP/RNBQK2R",        "w", "KQkq", "-",  1, 42,        "cpw-pos5-1"},
  {"rnbqkb1r/pp1p1ppp/2p5/4P3/2B5/8/PPP1NnPP/RNBQK2R",        "w", "KQkq", "-",  2, 1352,      "cpw-pos5-2"},
  {"rnbqkb1r/pp1p1ppp/2p5/4P3/2B5/8/PPP1NnPP/RNBQK2R",        "w", "KQkq", "-",  3, 53392,     "cpw-pos5-3"},
  {"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R", "w", "KQkq", "-",  1, 48,        "cpw-pos2-1"},
  {"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R", "w", "KQkq", "-",  2, 2039,      "cpw-pos2-2"},
  {"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R", "w", "KQkq", "-",  3, 97862,     "cpw-pos2-3"},
  {"8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8",                         "w", "-",    "-",  5, 674624,    "cpw-pos3-5"},
  {"n1n5/PPPk4/8/8/8/8/4Kppp/5N1N",                           "b", "-",    "-",  1, 24,        "prom-1"},
  {"8/5bk1/8/2Pp4/8/1K6/8/8",                                 "w", "-",    "d6", 6, 824064,    "ccc-1"},
  {"8/8/1k6/8/2pP4/8/5BK1/8",                                 "b", "-",    "d3", 6, 824064,    "ccc-2"},
  {"8/8/1k6/2b5/2pP4/8/5K2/8",                                "b", "-",    "d3", 6, 1440467,   "ccc-3"},
  {"8/5k2/8/2Pp4/2B5/1K6/8/8",                                "w", "-",    "d6", 6, 1440467,   "ccc-4"},
  {"5k2/8/8/8/8/8/8/4K2R",                                    "w", "K",    "-",  6, 661072,    "ccc-5"},
  {"4k2r/8/8/8/8/8/8/5K2",                                    "b", "k",    "-",  6, 661072,    "ccc-6"},
  {"3k4/8/8/8/8/8/8/R3K3",                                    "w", "Q",    "-",  6, 803711,    "ccc-7"},
  {"r3k3/8/8/8/8/8/8/3K4",                                    "b", "q",    "-",  6, 803711,    "ccc-8"},
  {"r3k2r/1b4bq/8/8/8/8/7B/R3K2R",                            "w", "KQkq", "-",  4, 1274206,   "ccc-9"},
  {"r3k2r/7b/8/8/8/8/1B4BQ/R3K2R",                            "b", "KQkq", "-",  4, 1274206,   "ccc-10"},
  {"r3k2r/8/3Q4/8/8/5q2/8/R3K2R",                             "b", "KQkq", "-",  4, 1720476,   "ccc-11"},
  {"r3k2r/8/5Q2/8/8/3q4/8/R3K2R",                             "w", "KQkq", "-",  4, 1720476,   "ccc-12"},
  {"2K2r2/4P3/8/8/8/8/8/3k4",                                 "w", "-",    "-",  6, 3821001,   "ccc-13"},
  {"3K4/8/8/8/8/8/4p3/2k2R2",                                 "b", "-",    "-",  6, 3821001,   "ccc-14"},
  {"8/8/1P2K3/8/2n5/1q6/8/5k2",                               "b", "-",    "-",  5, 1004658,   "ccc-15"},
  {"5K2/8/1Q6/2N5/8/1p2k3/8/8",                               "w", "-",    "-",  5, 1004658,   "ccc-16"},
  {"4k3/1P6/8/8/8/8/K7/8",                                    "w", "-",    "-",  6, 217342,    "ccc-17"},
  {"8/k7/8/8/8/8/1p6/4K3",                                    "b", "-",    "-",  6, 217342,    "ccc-18"},
  {"8/P1k5/K7/8/8/8/8/8",                                     "w", "-",    "-",  6, 92683,     "ccc-19"},
  {"8/8/8/8/8/k7/p1K5/8",                                     "b", "-",    "-",  6, 92683,     "ccc-20"},
  {"K1k5/8/P7/8/8/8/8/8",                                     "w", "-",    "-",  6, 2217,      "ccc-21"},
  {"8/8/8/8/8/p7/8/k1K5",                                     "b", "-",    "-",  6, 2217,      "ccc-22"},
  {"8/k1P5/8/1K6/8/8/8/8",                                    "w", "-",    "-",  7, 567584,    "ccc-23"},
  {"8/8/8/8/1k6/8/K1p5/8",                                    "b", "-",    "-",  7, 567584,    "ccc-24"},
  {"8/8/2k5/5q2/5n2/8/5K2/8",                                 "b", "-",    "-",  4, 23527,     "ccc-25"},
  {"8/5k2/8/5N2/5Q2/2K5/8/8",                                 "w", "-",    "-",  4, 23527,     "ccc-26"},
  {"8/p7/8/1P6/K1k3p1/6P1/7P/8",                              "w", "-",    "-",  8, 8103790,   "jvm-7"},
  {"n1n5/PPPk4/8/8/8/8/4Kppp/5N1N",                           "b", "-",    "-",  6, 71179139,  "jvm-8"},
  {"r3k2r/p6p/8/B7/1pp1p3/3b4/P6P/R3K2R",                     "w", "KQkq", "-",  6, 77054993,  "jvm-9"},
  {"8/5p2/8/2k3P1/p3K3/8/1P6/8",                              "b", "-",    "-",  8, 64451405,  "jvm-11"},
  {"r3k2r/pb3p2/5npp/n2p4/1p1PPB2/6P1/P2N1PBP/R3K2R",         "w", "KQkq", "-",  5, 29179893,  "jvm-12"},
  {"8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8",                         "w", "-",    "-",  7, 178633661, "jvm-10"},
  {"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R", "w", "KQkq", "-",  5, 193690690, "jvm-6"},
  {"8/2pkp3/8/RP3P1Q/6B1/8/2PPP3/rb1K1n1r",                   "w", "-",    "-",  6, 181153194, "ob1"},
  {"rnbqkb1r/ppppp1pp/7n/4Pp2/8/8/PPPP1PPP/RNBQKBNR",         "w", "KQkq", "f6", 6, 244063299, "jvm-5"},
  {"8/2ppp3/8/RP1k1P1Q/8/8/2PPP3/rb1K1n1r",                   "w", "-",    "-",  6, 205552081, "ob2"},
  {"8/8/3q4/4r3/1b3n2/8/3PPP2/2k1K2R",                        "w", "K",    "-",  6, 207139531, "ob3"},
  {"4r2r/RP1kP1P1/3P1P2/8/8/3ppp2/1p4p1/4K2R",                "b", "K",    "-",  6, 314516438, "ob4"},
  {"r3k2r/8/8/8/3pPp2/8/8/R3K1RR",                            "b", "KQkq", "e3", 6, 485647607, "jvm-1"},
  {"8/3K4/2p5/p2b2r1/5k2/8/8/1q6",                            "b", "-",    "-",  7, 493407574, "jvm-4"},
  {"r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1",   "w", "kq",   "-",  6, 706045033, "jvm-2"},
  {"r6r/1P4P1/2kPPP2/8/8/3ppp2/1p4p1/R3K2R",                  "w", "KQ",   "-",  6, 975944981, "ob5"}

};

static const size_t perft_data_len = sizeof(perft_data) / sizeof(perft_data[0]);

/*}}}*/
/*{{{  bench data*/

static const Bench bench_data[] = {

  {"r3k2r/2pb1ppp/2pp1q2/p7/1nP1B3/1P2P3/P2N1PPP/R2QK2R", "w", "KQkq", "a6"},
  {"4rrk1/2p1b1p1/p1p3q1/4p3/2P2n1p/1P1NR2P/PB3PP1/3R1QK1", "b", "-", "-"},
  {"r3qbrk/6p1/2b2pPp/p3pP1Q/PpPpP2P/3P1B2/2PB3K/R5R1", "w", "-", "-"},
  {"6k1/1R3p2/6p1/2Bp3p/3P2q1/P7/1P2rQ1K/5R2", "b", "-", "-"},
  {"8/8/1p2k1p1/3p3p/1p1P1P1P/1P2PK2/8/8", "w", "-", "-"},
  {"7r/2p3k1/1p1p1qp1/1P1Bp3/p1P2r1P/P7/4R3/Q4RK1", "w", "-", "-"},
  {"r1bq1rk1/pp2b1pp/n1pp1n2/3P1p2/2P1p3/2N1P2N/PP2BPPP/R1BQ1RK1", "b", "-", "-"},
  {"3r3k/2r4p/1p1b3q/p4P2/P2Pp3/1B2P3/3BQ1RP/6K1", "w", "-", "-"},
  {"2r4r/1p4k1/1Pnp4/3Qb1pq/8/4BpPp/5P2/2RR1BK1", "w", "-", "-"},
  {"4q1bk/6b1/7p/p1p4p/PNPpP2P/KN4P1/3Q4/4R3", "b", "-", "-"},
  {"2q3r1/1r2pk2/pp3pp1/2pP3p/P1Pb1BbP/1P4Q1/R3NPP1/4R1K1", "w", "-", "-"},
  {"1r2r2k/1b4q1/pp5p/2pPp1p1/P3Pn2/1P1B1Q1P/2R3P1/4BR1K", "b", "-", "-"},
  {"r3kbbr/pp1n1p1P/3ppnp1/q5N1/1P1pP3/P1N1B3/2P1QP2/R3KB1R", "b", "KQkq", "b3"},
  {"8/6pk/2b1Rp2/3r4/1R1B2PP/P5K1/8/2r5", "b", "-", "-"},
  {"1r4k1/4ppb1/2n1b1qp/pB4p1/1n1BP1P1/7P/2PNQPK1/3RN3", "w", "-", "-"},
  {"8/p2B4/PkP5/4p1pK/4Pb1p/5P2/8/8", "w", "-", "-"},
  {"3r4/ppq1ppkp/4bnp1/2pN4/2P1P3/1P4P1/PQ3PBP/R4K2", "b", "-", "-"},
  {"5rr1/4n2k/4q2P/P1P2n2/3B1p2/4pP2/2N1P3/1RR1K2Q", "w", "-", "-"},
  {"1r5k/2pq2p1/3p3p/p1pP4/4QP2/PP1R3P/6PK/8", "w", "-", "-"},
  {"q5k1/5ppp/1r3bn1/1B6/P1N2P2/BQ2P1P1/5K1P/8", "b", "-", "-"},
  {"r1b2k1r/5n2/p4q2/1ppn1Pp1/3pp1p1/NP2P3/P1PPBK2/1RQN2R1", "w", "-", "-"},
  {"r1bqk2r/pppp1ppp/5n2/4b3/4P3/P1N5/1PP2PPP/R1BQKB1R", "w", "KQkq", "-"},
  {"r1bqr1k1/pp1p1ppp/2p5/8/3N1Q2/P2BB3/1PP2PPP/R3K2n", "b", "Q", "-"},
  {"r1bq2k1/p4r1p/1pp2pp1/3p4/1P1B3Q/P2B1N2/2P3PP/4R1K1", "b", "-", "-"},
  {"r4qk1/6r1/1p4p1/2ppBbN1/1p5Q/P7/2P3PP/5RK1", "w", "-", "-"},
  {"r7/6k1/1p6/2pp1p2/7Q/8/p1P2K1P/8", "w", "-", "-"},
  {"r3k2r/ppp1pp1p/2nqb1pn/3p4/4P3/2PP4/PP1NBPPP/R2QK1NR", "w", "KQkq", "-"},
  {"3r1rk1/1pp1pn1p/p1n1q1p1/3p4/Q3P3/2P5/PP1NBPPP/4RRK1", "w", "-", "-"},
  {"5rk1/1pp1pn1p/p3Brp1/8/1n6/5N2/PP3PPP/2R2RK1", "w", "-", "-"},
  {"8/1p2pk1p/p1p1r1p1/3n4/8/5R2/PP3PPP/4R1K1", "b", "-", "-"},
  {"8/4pk2/1p1r2p1/p1p4p/Pn5P/3R4/1P3PP1/4RK2", "w", "-", "-"},
  {"8/5k2/1pnrp1p1/p1p4p/P6P/4R1PK/1P3P2/4R3", "b", "-", "-"},
  {"8/8/1p1kp1p1/p1pr1n1p/P6P/1R4P1/1P3PK1/1R6", "b", "-", "-"},
  {"8/8/1p1k2p1/p1prp2p/P2n3P/6P1/1P1R1PK1/4R3", "b", "-", "-"},
  {"8/8/1p4p1/p1p2k1p/P2npP1P/4K1P1/1P6/3R4", "w", "-", "-"},
  {"8/8/1p4p1/p1p2k1p/P2n1P1P/4K1P1/1P6/6R1", "b", "-", "-"},
  {"8/5k2/1p4p1/p1pK3p/P2n1P1P/6P1/1P6/4R3", "b", "-", "-"},
  {"8/1R6/1p1K1kp1/p6p/P1p2P1P/6P1/1Pn5/8", "w", "-", "-"},
  {"1rb1rn1k/p3q1bp/2p3p1/2p1p3/2P1P2N/PP1RQNP1/1B3P2/4R1K1", "b", "-", "-"},
  {"4rrk1/pp1n1pp1/q5p1/P1pP4/2n3P1/7P/1P3PB1/R1BQ1RK1", "w", "-", "-"},
  {"r2qr1k1/pb1nbppp/1pn1p3/2ppP3/3P4/2PB1NN1/PP3PPP/R1BQR1K1", "w", "-", "-"},
  {"2r2k2/8/4P1R1/1p6/8/P4K1N/7b/2B5", "b", "-", "-"},
  {"6k1/5pp1/8/2bKP2P/2P5/p4PNb/B7/8", "b", "-", "-"},
  {"2rqr1k1/1p3p1p/p2p2p1/P1nPb3/2B1P3/5P2/1PQ2NPP/R1R4K", "w", "-", "-"},
  {"r1b2rk1/p1q1ppbp/6p1/2Q5/8/4BP2/PPP3PP/2KR1B1R", "b", "-", "-"},
  {"6r1/5k2/p1b1r2p/1pB1p1p1/1Pp3PP/2P1R1K1/2P2P2/3R4", "w", "-", "-"},
  {"rnbqkb1r/pppppppp/5n2/8/2PP4/8/PP2PPPP/RNBQKBNR", "b", "KQkq", "c3"},
  {"2rr2k1/1p4bp/p1q1p1p1/4Pp1n/2PB4/1PN3P1/P3Q2P/2RR2K1", "w", "-", "f6"},
  {"3br1k1/p1pn3p/1p3n2/5pNq/2P1p3/1PN3PP/P2Q1PB1/4R1K1", "w", "-", "-"},
  {"2r2b2/5p2/5k2/p1r1pP2/P2pB3/1P3P2/K1P3R1/7R", "w", "-", "-"}

};

static const size_t bench_data_len = sizeof(bench_data) / sizeof(bench_data[0]);

/*}}}*/

/*}}}*/

/*{{{  utility*/

/*{{{  now_ms*/

static inline uint64_t now_ms(void) {

  struct timespec ts;

  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000u + (uint64_t)(ts.tv_nsec / 1000000u);

}

/*}}}*/
/*{{{  popcount*/

static inline int popcount(const uint64_t bb) {

  return __builtin_popcountll(bb);

}

/*}}}*/
/*{{{  encode_move*/

static inline move_t encode_move(const int from, const int to, const int flags) {

  return (from << 6) | to | flags;

}

/*}}}*/
/*{{{  bsf*/

static inline int bsf(const uint64_t bb) {

  return __builtin_ctzll(bb);

}

/*}}}*/
/*{{{  rand64*/

static uint64_t rand64(void) {

  rand64_seed ^= rand64_seed >> 12;
  rand64_seed ^= rand64_seed << 25;
  rand64_seed ^= rand64_seed >> 27;

  return rand64_seed * 2685821657736338717ULL;

}

/*}}}*/
/*{{{  piece_index*/

static inline int piece_index(const int piece, const int colour) {

  return piece + colour * 6;

}

/*}}}*/
/*{{{  colour_index*/

static inline int colour_index(const int colour) {

  return colour * 6;

}

/*}}}*/
/*{{{  print_bb*/

static void print_bb(const uint64_t bb, const char *tag) {

  printf("%s\n", tag);

  for (int rank = 7; rank >= 0; rank--) {

    printf("%d  ", rank + 1);

    for (int file = 0; file < 8; file++) {

      int sq = rank * 8 + file;
      char c = (bb & (1ULL << sq)) ? 'x' : '.';

      printf("%c ", c);
    }

    printf("\n");
  }

  printf("\n   a b c d e f g h\n\n");

}

/*}}}*/
/*{{{  format_move*/

static int format_move(const move_t move, char *const buf) {

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

  if ((move >> 14) & 1) {
    buf[4] = promo[(move >> 12) & 3];
    return 5;
  }

  return 4;

}

/*}}}*/
/*{{{  print_board*/

static void print_board(Node *const node) {

  const Position *const pos = &node->pos;

  const char piece_chars[12] = {
    'P', 'N', 'B', 'R', 'Q', 'K',
    'p', 'n', 'b', 'r', 'q', 'k'
  };

  for (int rank=7; rank >= 0; rank--) {

    printf("%d  ", rank + 1);

    for (int file=0; file < 8; file++) {

      int sq = rank * 8 + file;
      uint64_t bb = 1ULL << sq;
      char c = '.';

      for (int i=0; i < 12; i++) {
        if (pos->all[i] & bb) {
          c = piece_chars[i];
          break;
        }
      }

      printf("%c ", c);
    }

    printf("\n");
  }

  printf("   a b c d e f g h\n");
  printf("stm=%d\n", pos->stm);
  printf("rights=%d\n", pos->rights);
  printf("ep=%d\n", pos->ep);
  printf("h=%" PRIx64 "\n", pos->hash);

}

/*}}}*/
/*{{{  find_token*/

static int find_token(const char *token, int n, char **tokens) {

  for (int i=0; i < n; i++) {
    if (!strcmp(token, tokens[i]))
      return i;
  }

  return -1;

}

/*}}}*/
/*{{{  zob_index*/

static inline int zob_index(const int piece, const int sq) {

  return (piece << 6) | sq;

}

/*}}}*/
/*{{{  sqrrelu*/

static inline int32_t sqrelu(const int32_t x) {

  const int32_t y = x & ~(x >> 31);

  return y * y;

}

/*}}}*/
/*{{{  init_line_masks*/

static void init_line_masks(void) {

  for (int sq=0; sq < 64; sq++) {

    int r       = sq >> 3;
    int f       = sq & 7;
    uint64_t rm = 0;
    uint64_t fm = 0;
    uint64_t dm = 0;
    uint64_t am = 0;

    for (int k=0; k < 8; k++) {
      rm |= 1ULL << (r * 8 + k);     // same rank
      fm |= 1ULL << (k * 8 + f);     // same file
    }

    for (int rr=r+1, ff=f+1; rr<8  && ff<8;  rr++, ff++) dm |= 1ULL << (rr*8+ff);
    for (int rr=r-1, ff=f-1; rr>=0 && ff>=0; rr--, ff--) dm |= 1ULL << (rr*8+ff);
    for (int rr=r+1, ff=f-1; rr<8  && ff>=0; rr++, ff--) am |= 1ULL << (rr*8+ff);
    for (int rr=r-1, ff=f+1; rr>=0 && ff<8;  rr--, ff++) am |= 1ULL << (rr*8+ff);

    rank_mask[sq] = rm;
    file_mask[sq] = fm;
    diag_mask[sq] = dm;
    anti_mask[sq] = am;

  }

}

/*}}}*/
/*{{{  lut*/

static inline uint8_t lut(const uint8_t *const this_lut, const move_t move) {

  return this_lut[(move >> 12) & 0xF];

}

/*}}}*/

/*}}}*/
/*{{{  tc*/

/*{{{  tc_init*/

static void tc_init(int64_t wtime, int64_t winc, int64_t btime, int64_t binc, int64_t max_nodes, int64_t move_time, int max_depth, int moves_to_go) {

  if (wtime < 0)       wtime       = 0;
  if (winc < 0)        winc        = 0;
  if (btime < 0)       btime       = 0;
  if (binc < 0)        binc        = 0;
  if (max_nodes < 0)   max_nodes   = 0;
  if (move_time < 0)   move_time   = 0;
  if (moves_to_go < 0) moves_to_go = 0;
  if (max_depth < 0)   max_depth   = 0;

  tc = (TimeControl){0};

  int stm = ss[0].pos.stm;

  if (!max_nodes && !max_depth && !wtime && !winc && !btime && !binc && !move_time)
    move_time = 100;

  if (!moves_to_go)
    moves_to_go = 20;

  moves_to_go = max(2, moves_to_go); // thanks @eric

  if (!max_depth)
    max_depth = MAX_PLY;

  if (!move_time && stm == WHITE && wtime) {
    move_time = wtime / moves_to_go + winc / 2;
    if (!move_time)
      move_time = 1;
  }
  else if (!move_time && stm == BLACK && btime) {
    move_time = btime / moves_to_go + binc / 2;
    if (!move_time)
      move_time = 1;
  }

  tc.start_time = now_ms();

  if (move_time) {
    tc.finish_time = tc.start_time + move_time;
  }

  tc.max_nodes = max_nodes;
  tc.max_depth = max_depth;

  tc.bm = 0;

}

/*}}}*/
/*{{{  check_tc*/

static void check_tc(void) {

  if (tc.finished)
    return;

  if (tc.bm == 0)
    return;

  if (tc.finish_time) {
    if (now_ms() >= tc.finish_time) {
      tc.finished = 1;
      return;
    }
  }

  else if (tc.max_nodes) {
    if (tc.nodes >= tc.max_nodes) {
      tc.finished = 1;
      return;
    }
  }

}

/*}}}*/

/*}}}*/
/*{{{  tt*/

/*{{{  init_tt*/

static int init_tt(size_t megabytes) {

  if (megabytes < 1)
    megabytes = 1;

  if (megabytes > 1024)
    megabytes = 1024;

  if (tt) {
    free(tt);
    tt = NULL;
  }

  const size_t bytes = megabytes * 1024ULL * 1024ULL;

  tt_entries = bytes / sizeof(TT);
  tt_entries = 1ULL << (63 - __builtin_clzll(tt_entries));
  tt_mask    = tt_entries - 1;
  tt         = calloc(tt_entries, sizeof(TT));

  if (!tt) {
    fprintf(stderr, "info failed to allocate tt\n");
    return 1;
  }

  printf("info tt entries %zu (%zu MB)\n", tt_entries, (tt_entries * sizeof(TT)) / 1024 / 1024);

  return 0;

}

/*}}}*/
/*{{{  tt_reset*/

static void tt_reset(void) {

  memset(tt, 0, tt_entries * sizeof(*tt));

}

/*}}}*/
/*{{{  tt_put_adjusted_score*/

static int tt_put_adjusted_score(const int ply, const int score) {

  if (score < -MATE_LIMIT)
    return score - ply;

  else if (score > MATE_LIMIT)
    return score + ply;

  else
   return score;

}

/*}}}*/
/*{{{  tt_get_adjusted_score*/

static int tt_get_adjusted_score(const int ply, const int score) {

  if (score < -MATE_LIMIT)
    return score + ply;

  else if (score > MATE_LIMIT)
    return score - ply;

  else
    return score;

}

/*}}}*/
/*{{{  tt_put*/

static void tt_put(const Position *const pos, const int flags, const int depth, const int score, const move_t move) {

  const size_t idx         = pos->hash & tt_mask;
  TT *const RESTRICT entry = &tt[idx];

  if (entry->flags && entry->hash == pos->hash && entry->depth > depth)
    return;

  entry->depth = depth;
  entry->score = score;
  entry->move  = move;
  entry->hash  = pos->hash;
  entry->flags = flags;

}

/*}}}*/
/*{{{  tt_get*/

static TT *tt_get(const Position *const pos) {

  const size_t idx         = pos->hash & tt_mask;
  TT *const RESTRICT entry = &tt[idx];

  if (!entry->flags)
    return NULL;

  if (entry->hash != pos->hash)
    return NULL;

  return entry;

}

/*}}}*/

/*}}}*/
/*{{{  net*/

/*{{{  net_base*/

static int net_base(const int piece, const int sq) {

  return (((piece << 6) | sq) * NET_H1_SIZE);

}

/*}}}*/
/*{{{  init_weights*/

/*{{{  flip_index*/
//
// As defined by bullet.
// https://github.com/jw1912/bullet/blob/main/docs/1-basics.md
//

static int flip_index(const int index) {

  int piece  = index / (64 * NET_H1_SIZE);
  int square = (index / NET_H1_SIZE) % 64;
  int h      = index % NET_H1_SIZE;

  square ^= 56;
  piece = (piece + 6) % 12;

  return ((piece * 64) + square) * NET_H1_SIZE + h;

}

/*}}}*/
/*{{{  read_file*/

static uint8_t *read_file(const char *path, size_t *size_out) {

    *size_out = 0;
    FILE *f = fopen(path, "rb");

    if (!f)
      return NULL;

    if (fseek(f, 0, SEEK_END) != 0) {
      fclose(f);
      return NULL;
    }

    long sz = ftell(f);
    if (sz < 0) {
      fclose(f);
      return NULL;
    }
    rewind(f);

    uint8_t *buf = (uint8_t*)malloc((size_t)sz);
    if (!buf) {
      fclose(f);
      return NULL;
    }

    size_t got = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    if (got != (size_t)sz) {
      free(buf);
      return NULL;
    }

    *size_out = (size_t)sz;

    return buf;

}

/*}}}*/
/*{{{  get_weights*/

static int get_weights(const char *path, int16_t **out, size_t *count_out) {

  size_t bytes = 0;

  uint8_t *raw = read_file(path, &bytes);
  if (!raw)
    return 0;

  if (bytes % sizeof(int16_t) != 0) {
    free(raw);
    return 0;
  }

  int16_t *w = (int16_t*)raw;
  size_t count = bytes / sizeof(int16_t);

  *out = w;            // caller must free(*out)
  *count_out = count;  // number of int16 weights

  return 1;

}

/*}}}*/
/*{{{  get_embedded_weights*/
//
// xxd -i -n cwtch_weights ../lozza/nets/farm1/lozza-500/quantised.bin > weights.h
//

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


/*}}}*/

static int init_weights(void) {

  int16_t *weights = NULL;
  size_t n = 0;

  if (!get_embedded_weights(&weights, &n)) {
    free(weights);
    fprintf(stderr, "cannot load embedded weights\n");
    return 1;
  }

  //if (get_weights("/home/xyzzy/lozza/nets/fujia/lozza-750/quantised.bin", &weights, &n) == 0) {
    //free(weights);
    //fprintf(stderr, "cannot load weights file\n");
    //return 1;
  //}

  size_t offset = 0;

  for (int i=0; i < NET_I_SIZE * NET_H1_SIZE; i++) {
    net_h1_w[i]             = (int32_t)weights[i];  // us
    net_h2_w[flip_index(i)] = (int32_t)weights[i];  // them
  }

  offset += NET_I_SIZE * NET_H1_SIZE;
  for (int i=0; i < NET_H1_SIZE; i++) {
    net_h1_b[i] = (int32_t)weights[offset+i];
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

/*}}}*/
/*{{{  net_copy*/

static inline void net_copy(const Node *const from_node, Node *const to_node) {

  memcpy(to_node->acc1, from_node->acc1, sizeof from_node->acc1);
  memcpy(to_node->acc2, from_node->acc2, sizeof from_node->acc2);

}

/*}}}*/
/*{{{  net_slow_rebuild_accs*/

static void net_slow_rebuild_accs(Node *const node) {

  memcpy(node->acc1, net_h1_b, sizeof net_h1_b);
  memcpy(node->acc2, net_h1_b, sizeof net_h1_b);

  for (int sq=0; sq < 64; sq++) {

    const int piece = node->pos.board[sq];

    if (piece == EMPTY)
      continue;

    for (int h=0; h < NET_H1_SIZE; h++) {
      const int idx1 = (piece * 64 + sq) * NET_H1_SIZE + h;
      node->acc1[h] += net_h1_w[idx1];
      node->acc2[h] += net_h2_w[idx1];
    }
  }

}

/*}}}*/
/*{{{  net_update_accs*/

static void net_update_accs(Node *const node) {

  lazy.net_func(node);

}

/*}}}*/
/*{{{  net_eval*/

static int net_eval(Node *const node) {

  const int stm = node->pos.stm;

  const int32_t *const RESTRICT a1 = (stm == 0 ? node->acc1 : node->acc2);
  const int32_t *const RESTRICT a2 = (stm == 0 ? node->acc2 : node->acc1);

  const int32_t *const RESTRICT w1 = &net_o_w[0];
  const int32_t *const RESTRICT w2 = &net_o_w[NET_H1_SIZE];

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

/*}}}*/

/*{{{  net_move*/

static void net_move(Node *const node) {

  const int fr_piece = lazy.arg0;
  const int fr       = lazy.arg1;
  const int to       = lazy.arg2;

  int32_t *const RESTRICT a1 = node->acc1;
  int32_t *const RESTRICT a2 = node->acc2;

  const int b1 = net_base(fr_piece, fr);
  const int b2 = net_base(fr_piece, to);

  const int32_t *const RESTRICT w1_b1 = &net_h1_w[b1];
  const int32_t *const RESTRICT w1_b2 = &net_h1_w[b2];

  const int32_t *const RESTRICT w2_b1 = &net_h2_w[b1];
  const int32_t *const RESTRICT w2_b2 = &net_h2_w[b2];

  for (int i=0; i < NET_H1_SIZE; i++) {  // autovec move
    a1[i] += w1_b2[i] - w1_b1[i];
    a2[i] += w2_b2[i] - w2_b1[i];
  }

}

/*}}}*/
/*{{{  net_capture*/

static void net_capture(Node *const node) {

  const int fr_piece = lazy.arg0;
  const int fr       = lazy.arg1;
  const int to       = lazy.arg2;
  const int to_piece = lazy.arg3;

  int32_t *const RESTRICT a1 = node->acc1;
  int32_t *const RESTRICT a2 = node->acc2;

  const int b1 = net_base(fr_piece, fr);
  const int b2 = net_base(to_piece, to);
  const int b3 = net_base(fr_piece, to);

  const int32_t *const RESTRICT w1_b1 = &net_h1_w[b1];
  const int32_t *const RESTRICT w1_b2 = &net_h1_w[b2];
  const int32_t *const RESTRICT w1_b3 = &net_h1_w[b3];

  const int32_t *const RESTRICT w2_b1 = &net_h2_w[b1];
  const int32_t *const RESTRICT w2_b2 = &net_h2_w[b2];
  const int32_t *const RESTRICT w2_b3 = &net_h2_w[b3];

  for (int i=0; i < NET_H1_SIZE; i++) {  // autovec cap
    a1[i] += w1_b3[i] - w1_b2[i] - w1_b1[i];
    a2[i] += w2_b3[i] - w2_b2[i] - w2_b1[i];
  }

}

/*}}}*/
/*{{{  net_promo_push*/

static void net_promo_push (Node *const node) {

  const int pawn_piece    = lazy.arg0;
  const int pawn_fr       = lazy.arg1;
  const int pawn_to       = lazy.arg2;
  const int promote_piece = lazy.arg4;

  int32_t *const RESTRICT a1 = node->acc1;
  int32_t *const RESTRICT a2 = node->acc2;

  const int b1 = net_base(pawn_piece, pawn_fr);
  const int b2 = net_base(promote_piece, pawn_to);

  const int32_t *const RESTRICT w1_b1 = &net_h1_w[b1];
  const int32_t *const RESTRICT w1_b2 = &net_h1_w[b2];

  const int32_t *const RESTRICT w2_b1 = &net_h2_w[b1];
  const int32_t *const RESTRICT w2_b2 = &net_h2_w[b2];

  for (int i=0; i < NET_H1_SIZE; i++) {  // autovec promo push
    a1[i] += w1_b2[i] - w1_b1[i];
    a2[i] += w2_b2[i] - w2_b1[i];
  }

}

/*}}}*/
/*{{{  net_promo_capture*/

static void net_promo_capture (Node *const node) {

  const int pawn_piece    = lazy.arg0;
  const int pawn_fr       = lazy.arg1;
  const int pawn_to       = lazy.arg2;
  const int capture_piece = lazy.arg3;
  const int promote_piece = lazy.arg4;

  int32_t *const RESTRICT a1 = node->acc1;
  int32_t *const RESTRICT a2 = node->acc2;

  const int b1 = net_base(pawn_piece, pawn_fr);
  const int b2 = net_base(promote_piece, pawn_to);
  const int b3 = net_base(capture_piece, pawn_to);

  const int32_t *const RESTRICT w1_b1 = &net_h1_w[b1];
  const int32_t *const RESTRICT w1_b2 = &net_h1_w[b2];
  const int32_t *const RESTRICT w1_b3 = &net_h1_w[b3];

  const int32_t *const RESTRICT w2_b1 = &net_h2_w[b1];
  const int32_t *const RESTRICT w2_b2 = &net_h2_w[b2];
  const int32_t *const RESTRICT w2_b3 = &net_h2_w[b3];

  for (int i=0; i < NET_H1_SIZE; i++) {  // autovec promo cap
    a1[i] += w1_b2[i] - w1_b1[i] - w1_b3[i];
    a2[i] += w2_b2[i] - w2_b1[i] - w2_b3[i];
  }

}

/*}}}*/
/*{{{  net_ep_capture*/

static void net_ep_capture (Node *const node) {

  const int pawn_piece     = lazy.arg0;
  const int pawn_fr        = lazy.arg1;
  const int pawn_to        = lazy.arg2;
  const int opp_pawn_piece = lazy.arg3;
  const int opp_pawn_sq    = lazy.arg4;

  int32_t *const RESTRICT a1 = node->acc1;
  int32_t *const RESTRICT a2 = node->acc2;

  const int b1 = net_base(pawn_piece,     pawn_fr);
  const int b2 = net_base(pawn_piece,     pawn_to);
  const int b3 = net_base(opp_pawn_piece, opp_pawn_sq);

  const int32_t *const RESTRICT w1_b1 = &net_h1_w[b1];
  const int32_t *const RESTRICT w1_b2 = &net_h1_w[b2];
  const int32_t *const RESTRICT w1_b3 = &net_h1_w[b3];

  const int32_t *const RESTRICT w2_b1 = &net_h2_w[b1];
  const int32_t *const RESTRICT w2_b2 = &net_h2_w[b2];
  const int32_t *const RESTRICT w2_b3 = &net_h2_w[b3];

  for (int i=0; i < NET_H1_SIZE; i++) {  // autovec ep
    a1[i] += w1_b2[i] - w1_b1[i] - w1_b3[i];
    a2[i] += w2_b2[i] - w2_b1[i] - w2_b3[i];
  }

}


/*}}}*/
/*{{{  net_castle*/

static void net_castle (Node *const node) {

  const int king_piece = lazy.arg0;
  const int king_fr_sq = lazy.arg1;
  const int king_to_sq = lazy.arg2;
  const int rook_piece = lazy.arg3;
  const int rook_fr_sq = lazy.arg4;
  const int rook_to_sq = lazy.arg5;

  int32_t *const RESTRICT a1 = node->acc1;
  int32_t *const RESTRICT a2 = node->acc2;

  const int b1 = net_base(king_piece, king_fr_sq);
  const int b2 = net_base(king_piece, king_to_sq);
  const int b3 = net_base(rook_piece, rook_fr_sq);
  const int b4 = net_base(rook_piece, rook_to_sq);

  const int32_t *const RESTRICT w1_b1 = &net_h1_w[b1];
  const int32_t *const RESTRICT w1_b2 = &net_h1_w[b2];
  const int32_t *const RESTRICT w1_b3 = &net_h1_w[b3];
  const int32_t *const RESTRICT w1_b4 = &net_h1_w[b4];

  const int32_t *const RESTRICT w2_b1 = &net_h2_w[b1];
  const int32_t *const RESTRICT w2_b2 = &net_h2_w[b2];
  const int32_t *const RESTRICT w2_b3 = &net_h2_w[b3];
  const int32_t *const RESTRICT w2_b4 = &net_h2_w[b4];

  for (int i=0; i < NET_H1_SIZE; i++) {  // autovec_castle
    a1[i] += w1_b2[i] - w1_b1[i] + w1_b4[i] - w1_b3[i];
    a2[i] += w2_b2[i] - w2_b1[i] + w2_b4[i] - w2_b3[i];
  }

}

/*}}}*/

/*}}}*/
/*{{{  history*/

/*{{{  reset_hash_history*/

static inline void reset_hash_history(const int n) {

  hh.num_uci_moves = n;

}

/*}}}*/
/*{{{  update_hash_history*/

static inline void update_hash_history(const Position *const pos, const int ply) {

  hh.hash[min(1023, hh.num_uci_moves + ply)] = pos->hash;

}

/*}}}*/

/*{{{  reset_killers*/

static inline void reset_killers(void) {

  for (int i=0; i < MAX_PLY; i++) {
    ss[i].killer1 = 0;
    ss[i].killer2 = 0;
  }

}

/*}}}*/

/*{{{  reset_piece_to_history*/

static inline void reset_piece_to_history(void) {

  memset(piece_to_history, 0, sizeof(piece_to_history));

}

/*}}}*/
/*{{{  age_piece_to_history*/

static void age_piece_to_history() {

  for (int i=0; i < 12; i++) {
    for (int j=0; j < 64; j++) {
      piece_to_history[i][j] /= 2;
    }
  }

}

/*}}}*/
/*{{{  update_piece_to_history*/

static inline void update_piece_to_history(const Position *const pos, const move_t move, const int16_t bonus) {

  const int from       = (move >> 6) & 0x3F;
  const int to         = move & 0x3F;
  const int from_piece = pos->board[from];

  piece_to_history[from_piece][to] += bonus - piece_to_history[from_piece][to] * abs(bonus) / MAX_HISTORY;

  if (abs(piece_to_history[from_piece][to]) >= MAX_HISTORY) {
    age_piece_to_history();
  }

}

/*}}}*/

/*{{{  update_killer*/

static inline void update_killer(Node *const node, const move_t move) {

  if (move == node->tt_move || move == node->killer1)
    return;

  node->killer2 = node->killer1;
  node->killer1 = move;

}

/*}}}*/

/*{{{  rank_noisy*/

static void rank_noisy(Node *const node) {

  const uint8_t *const board = node->pos.board;
  const move_t *const moves  = node->moves;
  int16_t *const ranks       = node->ranks;
  const int n                = node->num_moves;

  for (int i=0; i < n; i++) {

    const move_t m     = moves[i];
    const int from     = (m >> 6) & 0x3F;
    const int to       =  m & 0x3F;
    const int attacker = board[from] % 6;
    int victim         = board[to];

    if (victim == EMPTY)  // ep
      victim = 0;
    else
      victim %= 6;

    ranks[i] = (victim << 3) | (5 - attacker);

  }
}

/*}}}*/
/*{{{  rank_quiet*/

static void rank_quiet(Node *const node) {

  const move_t killer1       = node->killer1;
  const move_t killer2       = node->killer2;
  const uint8_t *const board = node->pos.board;
  const move_t *const moves  = node->moves;
  int16_t *const ranks       = node->ranks;
  const int n                = node->num_moves;

  for (int i=0; i < n; i++) {

    const move_t move = moves[i];

    if (move == killer1) {
      ranks[i] = MAX_HISTORY + 2;
    }
    else if (move == killer2) {
      ranks[i] = MAX_HISTORY + 1;
    }
    else {
      const int from       = (move >> 6) & 0x3F;
      const int to         = move & 0x3F;
      const int from_piece = board[from];

      ranks[i] = piece_to_history[from_piece][to];
    }
  }
}


/*}}}*/

/*}}}*/
/*{{{  magics*/

/*{{{  magic_index*/

static inline int magic_index(const uint64_t blockers, const uint64_t magic, const int shift) {

  return (int)((blockers * magic) >> shift);

}

/*}}}*/
/*{{{  get_blockers*/

static void get_blockers(Attack *a, uint64_t *blockers) {

  int bits[64];
  int num_bits = 0;

  for (int b=0; b < 64; b++) {
    if (a->mask & (1ULL << b)) {
      bits[num_bits++] = b;
    }
  }

  for (int i=0; i < a->count; i++) {

    uint64_t blocker = 0;

    for (int j=0; j < a->bits; j++) {
      if (i & (1 << j)) {
        blocker |= 1ULL << bits[j];
      }
    }

    blockers[i] = blocker;

  }
}

/*}}}*/
/*{{{  find_magics*/

static void find_magics(Attack attacks[64], const char *label) {

  uint64_t tbl[MAGIC_MAX_SLOTS];
  uint32_t used[MAGIC_MAX_SLOTS];

  static uint32_t stamp = 1;
  const int verbose     = 0;
  int total_tries       = 0;
  int total_slots       = 0;

  for (int sq=0; sq < 64; ++sq) {

    Attack *a   = &attacks[sq];
    const int N = a->count;

    uint64_t blockers[MAGIC_MAX_SLOTS];
    get_blockers(a, blockers);

    memset(used, 0, (size_t)N * sizeof used[0]);

    int tries = 0;

    for (;;) {

      ++tries;

      if (++stamp == 0) {
        memset(used, 0, (size_t)N * sizeof used[0]);
        stamp = 1;
      }

      const uint64_t magic = rand64() & rand64() & rand64();

      if (popcount((a->mask * magic) >> (64 - a->bits)) < a->bits - 3)
        continue;

      int fail = 0;

      for (int i=0; i < N; ++i) {

        const int idx      = magic_index(blockers[i], magic, a->shift);
        const uint64_t att = a->attacks[i];

        if (used[idx] != stamp) {
          used[idx] = stamp;
          tbl[idx]  = att;
        }
        else if (tbl[idx] != att) {
          fail = 1;
          break;
        }
      }

      if (!fail) {

        a->magic = magic;

        for (int i=0; i < N; ++i) {
          const int idx   = magic_index(blockers[i], magic, a->shift);
          a->attacks[idx] = tbl[idx];
        }

        if (verbose) {
          printf("%s sq %2d tries %d %8d magic %" PRIx64 "\n",
                 label, sq, tries, N, a->magic);
        }

        total_tries += tries;
        total_slots += N;

        break;

      }
    }
  }

  if (verbose)
    printf("%s total_tries %d total_slots %d\n", label, total_tries, total_slots);

  (void)label;

}

/*}}}*/

/*{{{  init_pawn_attacks*/
//
// These are attacks *to* the sq and used in pawn gen (ep) and is_attacked.
//

static void init_pawn_attacks(void) {

  for (int sq=0; sq < 64; sq++) {

    const uint64_t bb = 1ULL << sq;

    pawn_attacks[WHITE][sq] = ((bb >> 7) & NOT_A_FILE) | ((bb >> 9) & NOT_H_FILE);
    pawn_attacks[BLACK][sq] = ((bb << 7) & NOT_H_FILE) | ((bb << 9) & NOT_A_FILE);

  }
}

/*}}}*/
/*{{{  init_knight_attacks*/

static void init_knight_attacks(void) {

  for (int sq=0; sq < 64; sq++) {

    int r = sq / 8, f = sq % 8;
    uint64_t bb = 0;

    int dr[8] = {-2, -1, 1, 2, 2, 1, -1, -2};
    int df[8] = {1, 2, 2, 1, -1, -2, -2, -1};

    for (int i=0; i < 8; i++) {
      int nr = r + dr[i], nf = f + df[i];
      if (nr >= 0 && nr < 8 && nf >= 0 && nf < 8)
        bb |= 1ULL << (nr * 8 + nf);
    }

    knight_attacks[sq] = bb;

  }
}

/*}}}*/
/*{{{  init_bishop_attacks*/

static void init_bishop_attacks(void) {

  int next_attack_index = 0;

  uint64_t blockers[MAGIC_MAX_SLOTS];

  for (int sq=0; sq < 64; sq++) {

    Attack *a = &bishop_attacks[sq];

    /*{{{  build mask*/
    
    int rank = sq / 8;
    int file = sq % 8;
    
    a->mask = 0;
    
    for (int r = rank + 1, f = file + 1; r <= 6 && f <= 6; r++, f++)
      a->mask |= 1ULL << (r * 8 + f);
    
    for (int r = rank + 1, f = file - 1; r <= 6 && f >= 1; r++, f--)
      a->mask |= 1ULL << (r * 8 + f);
    
    for (int r = rank - 1, f = file + 1; r >= 1 && f <= 6; r--, f++)
      a->mask |= 1ULL << (r * 8 + f);
    
    for (int r = rank - 1, f = file - 1; r >= 1 && f >= 1; r--, f--)
      a->mask |= 1ULL << (r * 8 + f);
    
    /*}}}*/

    a->bits    = popcount(a->mask);
    a->shift   = 64 - a->bits;
    a->count   = 1 << a->bits;
    a->attacks = &raw_attacks[next_attack_index];

    get_blockers(a, blockers);

    for (int i=0; i < a->count; i++) {
      /*{{{  build attacks[next_attack_index]*/
      
      uint64_t blocker = blockers[i];
      uint64_t attack  = 0;
      
      for (int r=rank+1, f=file+1; r <= 7 && f <= 7; r++, f++) {
        int s = r * 8 + f;
        attack |= 1ULL << s;
        if (blocker & (1ULL << s))
          break;
      }
      
      for (int r=rank+1, f=file-1; r <= 7 && f >= 0; r++, f--) {
        int s = r * 8 + f;
        attack |= 1ULL << s;
        if (blocker & (1ULL << s))
          break;
      }
      
      for (int r=rank-1, f=file+1; r >= 0 && f <= 7; r--, f++) {
        int s = r * 8 + f;
        attack |= 1ULL << s;
        if (blocker & (1ULL << s))
          break;
      }
      
      for (int r=rank-1, f=file-1; r >= 0 && f >= 0; r--, f--) {
        int s = r * 8 + f;
        attack |= 1ULL << s;
        if (blocker & (1ULL << s))
          break;
      }
      
      raw_attacks[next_attack_index++] = attack;
      
      /*}}}*/
    }
  }

  find_magics(bishop_attacks, "B");

}

/*}}}*/
/*{{{  init_rook_attacks*/

static void init_rook_attacks(void) {

  int next_attack_index = 5248;

  uint64_t blockers[MAGIC_MAX_SLOTS];

  for (int sq=0; sq < 64; sq++) {

    Attack *a = &rook_attacks[sq];

    /*{{{  build mask*/
    
    int rank = sq / 8;
    int file = sq % 8;
    
    a->mask = 0;
    
    for (int f=file+1; f <= 6; f++)
      a->mask |= 1ULL << (rank * 8 + f);
    
    for (int f=file-1; f >= 1; f--)
      a->mask |= 1ULL << (rank * 8 + f);
    
    for (int r=rank+1; r <= 6; r++)
      a->mask |= 1ULL << (r * 8 + file);
    
    for (int r=rank-1; r >= 1; r--)
      a->mask |= 1ULL << (r * 8 + file);
    
    /*}}}*/

    a->bits    = popcount(a->mask);
    a->shift   = 64 - a->bits;
    a->count   = 1 << a->bits;
    a->attacks = &raw_attacks[next_attack_index];

    get_blockers(a, blockers);

    for (int i=0; i < a->count; i++) {
      /*{{{  build attacks[next_attack_index]*/
      
      uint64_t blocker = blockers[i];
      uint64_t attack  = 0;
      
      for (int r=rank+1; r <= 7; r++) {
        int s = r * 8 + file;
        attack |= 1ULL << s;
        if (blocker & (1ULL << s)) {
          break;
        }
      }
      
      for (int r=rank-1; r >= 0; r--) {
        int s = r * 8 + file;
        attack |= 1ULL << s;
        if (blocker & (1ULL << s)) {
          break;
        }
      }
      
      for (int f=file+1; f <= 7; f++) {
        int s = rank * 8 + f;
        attack |= 1ULL << s;
        if (blocker & (1ULL << s)) {
          break;
        }
      }
      
      for (int f=file-1; f >= 0; f--) {
        int s = rank * 8 + f;
        attack |= 1ULL << s;
        if (blocker & (1ULL << s)) {
          break;
        }
      }
      
      raw_attacks[next_attack_index++] = attack;
      
      /*}}}*/
    }
  }

  find_magics(rook_attacks, "R");

}

/*}}}*/
/*{{{  init_king_attacks*/

static void init_king_attacks(void) {

  for (int sq=0; sq < 64; sq++) {

    int r       = sq / 8, f = sq % 8;
    uint64_t bb = 0;

    for (int dr=-1; dr <= 1; dr++) {
      for (int df=-1; df <= 1; df++) {
        if (dr == 0 && df == 0) continue;
        int nr = r + dr, nf = f + df;
        if (nr >= 0 && nr < 8 && nf >= 0 && nf < 8)
          bb |= 1ULL << (nr * 8 + nf);
      }
    }

    king_attacks[sq] = bb;

  }
}

/*}}}*/
/*{{{  init_all_attacks*/

static void init_all_attacks(void) {

  for (int sq=0; sq < 64; sq++) {

    int r       = sq / 8;
    int f       = sq % 8;
    uint64_t bb = 0;

    const int bdr[4] = { 1,  1, -1, -1};
    const int bdf[4] = { 1, -1,  1, -1};

    for (int dir = 0; dir < 4; dir++) {
      int nr = r + bdr[dir];
      int nf = f + bdf[dir];
      while (nr >= 0 && nr < 8 && nf >= 0 && nf < 8) {
        bb |= 1ULL << (nr * 8 + nf);
        nr += bdr[dir];
        nf += bdf[dir];
      }
    }

    const int rdr[4] = { 1, -1,  0,  0};
    const int rdf[4] = { 0,  0,  1, -1};

    for (int dir = 0; dir < 4; dir++) {
      int nr = r + rdr[dir];
      int nf = f + rdf[dir];
      while (nr >= 0 && nr < 8 && nf >= 0 && nf < 8) {
        bb |= 1ULL << (nr * 8 + nf);
        nr += rdr[dir];
        nf += rdf[dir];
      }
    }

    bb |= knight_attacks[sq];

    all_attacks_inc_edge[sq] = bb;
    all_attacks[sq]          = knight_attacks[sq] | bishop_attacks[sq].mask | rook_attacks[sq].mask;

  }
}

/*}}}*/

/*}}}*/
/*{{{  move generation*/

/*{{{  init_rights_masks*/

static void init_rights_masks(void) {

  for (int sq=0; sq < 64; ++sq)
    rights_masks[sq] = ALL_RIGHTS;

  rights_masks[A1] &= ~WHITE_RIGHTS_QUEEN;
  rights_masks[H1] &= ~WHITE_RIGHTS_KING;
  rights_masks[E1] &= ~(WHITE_RIGHTS_KING | WHITE_RIGHTS_QUEEN);

  rights_masks[A8] &= ~BLACK_RIGHTS_QUEEN;
  rights_masks[H8] &= ~BLACK_RIGHTS_KING;
  rights_masks[E8] &= ~(BLACK_RIGHTS_KING | BLACK_RIGHTS_QUEEN);

}

/*}}}*/
/*{{{  is_attacked*/

static int is_attacked(const Position *const pos, const int sq, const int opp) {

  const uint64_t *const RESTRICT all = pos->all;
  const int base                     = colour_index(opp);

  if (all[base+KNIGHT] & knight_attacks[sq])    return 1;
  if (all[base+PAWN]   & pawn_attacks[opp][sq]) return 1;
  if (all[base+KING]   & king_attacks[sq])      return 1;

  const uint64_t occ   = pos->occupied;
  const uint64_t all_q = all[base+QUEEN];

  {
    const uint64_t opp             = all[base+ROOK] | all_q;
    const Attack *const RESTRICT a = &rook_attacks[sq];
    const uint64_t blockers        = occ & a->mask;
    const uint64_t attacks         = a->attacks[magic_index(blockers, a->magic, a->shift)];
    if (attacks & opp) return 1;
  }

  {
    const uint64_t opp             = all[base+BISHOP] | all_q;
    const Attack *const RESTRICT a = &bishop_attacks[sq];
    const uint64_t blockers        = occ & a->mask;
    const uint64_t attacks         = a->attacks[magic_index(blockers, a->magic, a->shift)];
    if (attacks & opp) return 1;
  }

  return 0;

}

/*}}}*/

/*{{{  gen_pawns*/

/*{{{  gen_pawns_white_quiet*/

static void gen_pawns_white_quiet(Node *const node) {

  const Position *const pos = &node->pos;
  const uint64_t pawns      = pos->all[WPAWN];
  const uint64_t occupied   = pos->occupied;
  move_t *const RESTRICT m  = node->moves + node->num_moves;
  int n                     = 0;

  /*{{{  push 1*/
  
  const uint64_t one = (pawns << 8) & ~occupied;
  uint64_t quiet     = one & ~RANK_8;
  uint64_t promo     = one &  RANK_8;
  
  while (quiet) {
    const int to = bsf(quiet); quiet &= quiet - 1;
    m[n++]       = encode_move(to - 8, to, MT_PAWN_PUSH);
  }
  
  while (promo) {
  
    const int to   = bsf(promo); promo &= promo - 1;
    const int from = to - 8;
  
    m[n++] = encode_move(from, to, MT_PROMO_PUSH_Q);
    m[n++] = encode_move(from, to, MT_PROMO_PUSH_R);
    m[n++] = encode_move(from, to, MT_PROMO_PUSH_B);
    m[n++] = encode_move(from, to, MT_PROMO_PUSH_N);
  
  }
  
  /*}}}*/
  /*{{{  push 2*/
  
  uint64_t two = ((one & RANK_3) << 8) & ~occupied;
  
  while (two) {
    int to = bsf(two); two &= two - 1;
    m[n++] = encode_move(to - 16, to, MT_EP_PUSH);
  }
  
  /*}}}*/

  node->num_moves += n;

}

/*}}}*/
/*{{{  gen_pawns_white_noisy*/

static void gen_pawns_white_noisy(Node *const node) {

  const Position *const pos = &node->pos;
  const uint64_t pawns      = pos->all[WPAWN];
  const uint64_t enemies    = pos->colour[BLACK];
  move_t *const RESTRICT m  = node->moves + node->num_moves;
  int n                     = 0;

  /*{{{  captures*/
  
  const uint64_t left  = ((pawns << 7) & NOT_H_FILE) & enemies;
  const uint64_t right = ((pawns << 9) & NOT_A_FILE) & enemies;
  
  uint64_t cap   = left & ~RANK_8;
  uint64_t promo = left &  RANK_8;
  
  while (cap) {
    int to = bsf(cap); cap &= cap - 1;
    m[n++] = encode_move(to - 7, to, MT_PAWN_CAPTURE);
  }
  
  while (promo) {
  
    int to         = bsf(promo); promo &= promo - 1;
    const int from = to - 7;
  
    m[n++] = encode_move(from, to, MT_PROMO_CAPTURE_Q);
    m[n++] = encode_move(from, to, MT_PROMO_CAPTURE_R);
    m[n++] = encode_move(from, to, MT_PROMO_CAPTURE_B);
    m[n++] = encode_move(from, to, MT_PROMO_CAPTURE_N);
  
  }
  
  cap   = right & ~RANK_8;
  promo = right &  RANK_8;
  
  while (cap) {
    const int to = bsf(cap); cap &= cap - 1;
    m[n++]       = encode_move(to - 9, to, MT_PAWN_CAPTURE);
  }
  
  while (promo) {
  
    const int to   = bsf(promo); promo &= promo - 1;
    const int from = to - 9;
  
    m[n++] = encode_move(from, to, MT_PROMO_CAPTURE_Q);
    m[n++] = encode_move(from, to, MT_PROMO_CAPTURE_R);
    m[n++] = encode_move(from, to, MT_PROMO_CAPTURE_B);
    m[n++] = encode_move(from, to, MT_PROMO_CAPTURE_N);
  
  }
  
  /*}}}*/
  /*{{{  ep*/
  
  if (pos->ep) {
  
    uint64_t ep_from = pawn_attacks[WHITE][pos->ep] & pawns;
  
    while (ep_from) {
      const int from = bsf(ep_from); ep_from &= ep_from - 1;
      m[n++]         = encode_move(from, pos->ep, MT_EP_CAPTURE);
    }
  
  }
  
  /*}}}*/

  node->num_moves += n;

}

/*}}}*/

/*{{{  gen_pawns_black_quiet*/

static void gen_pawns_black_quiet(Node *const node) {

  const Position *const pos = &node->pos;
  const uint64_t pawns      = pos->all[BPAWN];
  const uint64_t occupied   = pos->occupied;
  move_t *const RESTRICT m  = node->moves + node->num_moves;
  int n                     = 0;

  /*{{{  push 1*/
  
  const uint64_t one = (pawns >> 8) & ~occupied;
  uint64_t quiet     = one & ~RANK_1;
  uint64_t promo     = one &  RANK_1;
  
  while (quiet) {
    const int to = bsf(quiet); quiet &= quiet - 1;
    m[n++]       = encode_move(to + 8, to, MT_PAWN_PUSH);
  }
  
  while (promo) {
  
    const int to   = bsf(promo); promo &= promo - 1;
    const int from = to + 8;
  
    m[n++] = encode_move(from, to, MT_PROMO_PUSH_Q);
    m[n++] = encode_move(from, to, MT_PROMO_PUSH_R);
    m[n++] = encode_move(from, to, MT_PROMO_PUSH_B);
    m[n++] = encode_move(from, to, MT_PROMO_PUSH_N);
  
  }
  
  /*}}}*/
  /*{{{  push 2*/
  
  uint64_t two = ((one & RANK_6) >> 8) & ~occupied;
  
  while (two) {
    int to = bsf(two); two &= two - 1;
    m[n++] = encode_move(to + 16, to, MT_EP_PUSH);
  }
  
  /*}}}*/

  node->num_moves += n;

}

/*}}}*/
/*{{{  gen_pawns_black_noisy*/

static void gen_pawns_black_noisy(Node *const node) {

  const Position *const pos = &node->pos;
  const uint64_t pawns      = pos->all[BPAWN];
  const uint64_t enemies    = pos->colour[WHITE];
  move_t *const RESTRICT m  = node->moves + node->num_moves;
  int n                     = 0;

  /*{{{  captures*/
  
  const uint64_t left  = ((pawns >> 9) & NOT_H_FILE) & enemies;
  const uint64_t right = ((pawns >> 7) & NOT_A_FILE) & enemies;
  
  uint64_t cap   = left & ~RANK_1;
  uint64_t promo = left &  RANK_1;
  
  while (cap) {
    const int to = bsf(cap); cap &= cap - 1;
    m[n++]       = encode_move(to + 9, to, MT_PAWN_CAPTURE);
  }
  
  while (promo) {
  
    const int to   = bsf(promo); promo &= promo - 1;
    const int from = to + 9;
  
    m[n++] = encode_move(from, to, MT_PROMO_CAPTURE_Q);
    m[n++] = encode_move(from, to, MT_PROMO_CAPTURE_R);
    m[n++] = encode_move(from, to, MT_PROMO_CAPTURE_B);
    m[n++] = encode_move(from, to, MT_PROMO_CAPTURE_N);
  
  }
  
  cap   = right & ~RANK_1;
  promo = right &  RANK_1;
  
  while (cap) {
    const int to = bsf(cap); cap &= cap - 1;
    m[n++]       = encode_move(to + 7, to, MT_PAWN_CAPTURE);
  }
  
  while (promo) {
  
    const int to   = bsf(promo); promo &= promo - 1;
    const int from = to + 7;
  
    m[n++] = encode_move(from, to, MT_PROMO_CAPTURE_Q);
    m[n++] = encode_move(from, to, MT_PROMO_CAPTURE_R);
    m[n++] = encode_move(from, to, MT_PROMO_CAPTURE_B);
    m[n++] = encode_move(from, to, MT_PROMO_CAPTURE_N);
  
  }
  
  /*}}}*/
  /*{{{  ep*/
  
  if (pos->ep) {
  
    uint64_t ep_from = pawn_attacks[BLACK][pos->ep] & pawns;
  
    while (ep_from) {
      const int from = bsf(ep_from); ep_from &= ep_from - 1;
      m[n++]         = encode_move(from, pos->ep, MT_EP_CAPTURE);
    }
  
  }
  
  /*}}}*/

  node->num_moves += n;

}

/*}}}*/

static inline void gen_pawns_quiet(Node *const node) {
  if (node->pos.stm == WHITE)
    gen_pawns_white_quiet(node);
  else
    gen_pawns_black_quiet(node);
}

static inline void gen_pawns_noisy(Node *const node) {
  if (node->pos.stm == WHITE)
    gen_pawns_white_noisy(node);
  else
    gen_pawns_black_noisy(node);
}

/*}}}*/
/*{{{  gen_jumpers*/

static void gen_jumpers(Node *const node, const uint64_t *const attack_table, const int piece, const uint64_t targets, const int flag) {

  const Position *const pos = &node->pos;
  const int stm             = pos->stm;
  move_t *const RESTRICT m  = node->moves + node->num_moves;
  int n                     = 0;
  uint64_t bb               = pos->all[piece_index(piece, stm)];

  while (bb) {

    const int from   = bsf(bb); bb &= bb - 1;
    uint64_t attacks = attack_table[from] & targets;

    while (attacks) {
      const int to = bsf(attacks); attacks &= attacks - 1;
      m[n++]       = encode_move(from, to, flag);
    }

  }

  node->num_moves += n;

}

/*}}}*/
/*{{{  gen_sliders*/

static void gen_sliders(Node *const node, const Attack *const RESTRICT attack_table, const int piece, const uint64_t targets, const int flag) {

  const Position *const pos = &node->pos;
  const int stm             = pos->stm;
  const uint64_t occ        = pos->occupied;
  move_t *const RESTRICT m  = node->moves + node->num_moves;
  int n                     = 0;
  uint64_t bb               = pos->all[piece_index(piece, stm)];

  while (bb) {

    const int from                 = bsf(bb); bb &= bb - 1;
    const Attack *const RESTRICT a = &attack_table[from];
    const uint64_t blockers        = occ & a->mask;
    const int index                = magic_index(blockers, a->magic, a->shift);
    uint64_t attacks               = a->attacks[index] & targets;

    while (attacks) {
      const int to = bsf(attacks); attacks &= attacks - 1;
      m[n++]       = encode_move(from, to, flag);
    }

  }

  node->num_moves += n;

}

/*}}}*/
/*{{{  gen_castling*/

static void gen_castling(Node *const node) {

  const Position *const pos = &node->pos;
  const int stm             = pos->stm;
  const int opp             = stm ^ 1;
  const uint8_t rights      = pos->rights;
  const uint64_t occupied   = pos->occupied;
  move_t *const RESTRICT m  = node->moves + node->num_moves;
  int n                     = 0;

  if (stm == WHITE) {

    if ((rights & WHITE_RIGHTS_KING) &&
         !(occupied & 0x0000000000000060ULL) &&
         !is_attacked(pos, F1, opp) &&
         !is_attacked(pos, G1, opp)) {
      m[n++] = encode_move(E1, G1, MT_CASTLE);
    }

    if ((rights & WHITE_RIGHTS_QUEEN) &&
         !(occupied & 0x000000000000000EULL) &&
         !is_attacked(pos, D1, opp) &&
         !is_attacked(pos, C1, opp)) {
      m[n++] = encode_move(E1, C1, MT_CASTLE);
    }

  }

  else {

    if ((rights & BLACK_RIGHTS_KING) &&
         !(occupied & 0x6000000000000000ULL) &&
         !is_attacked(pos, F8, opp) &&
         !is_attacked(pos, G8, opp)) {
      m[n++] = encode_move(E8, G8, MT_CASTLE);
    }

    if ((rights & BLACK_RIGHTS_QUEEN) &&
         !(occupied & 0x0E00000000000000ULL) &&
         !is_attacked(pos, D8, opp) &&
         !is_attacked(pos, C8, opp)) {
      m[n++] = encode_move(E8, C8, MT_CASTLE);
    }

  }

  node->num_moves += n;

}

/*}}}*/

/*{{{  gen_noisy*/

static void gen_noisy(Node *const node) {

  const Position *const pos    = &node->pos;
  const int stm                = pos->stm;
  const int opp                = stm ^ 1;
  const uint64_t opp_king_bb   = pos->all[piece_index(KING, opp)];
  const uint64_t opp_king_near = king_attacks[bsf(opp_king_bb)];
  const uint64_t enemies       = pos->colour[opp] & ~opp_king_bb;
  uint64_t targets             = enemies;

  if (node->in_check) {
    const int our_king_sq = bsf(pos->all[piece_index(KING, stm)]);
    targets               &= all_attacks_inc_edge[our_king_sq];
  }

  gen_pawns_noisy(node);
  gen_jumpers(node, knight_attacks, KNIGHT, targets,                  MT_CAPTURE);
  gen_sliders(node, bishop_attacks, BISHOP, targets,                  MT_CAPTURE);
  gen_sliders(node, rook_attacks,   ROOK,   targets,                  MT_CAPTURE);
  gen_sliders(node, bishop_attacks, QUEEN,  targets,                  MT_CAPTURE);
  gen_sliders(node, rook_attacks,   QUEEN,  targets,                  MT_CAPTURE);
  gen_jumpers(node, king_attacks,   KING,   enemies & ~opp_king_near, MT_CAPTURE);

}

/*}}}*/
/*{{{  gen_quiet*/

static void gen_quiet(Node *const node) {

  const Position *const pos    = &node->pos;
  const int stm                = pos->stm;
  const int opp                = stm ^ 1;
  const uint64_t occ           = pos->occupied;
  const uint64_t opp_king_bb   = pos->all[piece_index(KING, opp)];
  const uint64_t opp_king_near = king_attacks[bsf(opp_king_bb)];
  uint64_t targets             = ~occ;

  if (node->in_check) {
    const int our_king_sq = bsf(pos->all[piece_index(KING, stm)]);
    targets               &= all_attacks[our_king_sq];
  }

  gen_pawns_quiet(node);
  gen_jumpers(node, knight_attacks, KNIGHT, targets,               MT_NON_PAWN_PUSH);
  gen_sliders(node, bishop_attacks, BISHOP, targets,               MT_NON_PAWN_PUSH);
  gen_sliders(node, rook_attacks,   ROOK,   targets,               MT_NON_PAWN_PUSH);
  gen_sliders(node, bishop_attacks, QUEEN,  targets,               MT_NON_PAWN_PUSH);
  gen_sliders(node, rook_attacks,   QUEEN,  targets,               MT_NON_PAWN_PUSH);
  gen_jumpers(node, king_attacks,   KING,   ~occ & ~opp_king_near, MT_NON_PAWN_PUSH);

  if (node->pos.rights && !node->in_check)
    gen_castling(node);

}

/*}}}*/

/*}}}*/
/*{{{  move iterators*/

/*{{{  remove_tt_move*/

static void remove_tt_move(Node *const node) {

  const int n                  = node->num_moves;
  move_t *const RESTRICT moves = node->moves;
  move_t tt_move               = node->tt_move;

  for (int i=0; i < n; i++) {

    if (moves[i] == tt_move) {
      moves[i] = moves[n-1];
      node->num_moves--;
      break;
    }

  }
}

/*}}}*/
/*{{{  get_next_sorted_move*/

static move_t get_next_sorted_move(Node *const node) {

  move_t max_m         = 0;
  move_t *const moves  = node->moves;
  int16_t *const ranks = node->ranks;
  const int next       = node->next_move;
  const int num        = node->num_moves;
  int16_t max_r        = INT16_MIN;  // must be < -MAX_HISTORY
  int max_i            = 0;

  for (int i=next; i < num; i++) {
    if (ranks[i] > max_r) {
      max_r = ranks[i];
      max_i = i;
    }
  }

  max_m = moves[max_i];

  moves[max_i] = moves[next];
  ranks[max_i] = ranks[next];
  moves[next]  = max_m;  // for history penalties

  node->next_move++;

  return max_m;

}

/*}}}*/

/*{{{  init_next_search_move*/

static void init_next_search_move(Node *const node, const uint8_t in_check, const move_t tt_move) {

  node->stage    = 0;
  node->in_check = in_check;
  node->tt_move  = tt_move;

}

/*}}}*/
/*{{{  get_next_search_move*/

static move_t get_next_search_move(Node *const node) {

  switch (node->stage) {

    case 0: {
      /*{{{  tt*/
      
      node->stage++;
      node->num_moves = 0;
      node->next_move = 0;
      
      if (node->tt_move)
        return node->tt_move;
      
      /*}}}*/
    }

    /* fall through */

    case 1: {
      /*{{{  gen noisy*/
      
      node->stage++;
      node->num_moves = 0;
      node->next_move = 0;
      
      gen_noisy(node);
      
      if (node->tt_move && lut(lut_noisy, node->tt_move))
        remove_tt_move(node);
      
      rank_noisy(node);
      
      /*}}}*/
    }

    /* fall through */

    case 2: {
      /*{{{  next noisy / gen quiet*/
      
      if (node->next_move < node->num_moves) {
      
        return get_next_sorted_move(node);
      
      }
      
      node->stage++;
      node->num_moves = 0;
      node->next_move = 0;
      
      gen_quiet(node);
      
      if (node->tt_move && lut(lut_quiet, node->tt_move))
        remove_tt_move(node);
      
      rank_quiet(node);
      
      /*}}}*/
    }

    /* fall through */

    case 3: {
      /*{{{  next quiet*/
      
      if (node->next_move < node->num_moves) {
      
        return get_next_sorted_move(node);
      
      }
      
      return 0;
      
      /*}}}*/
    }

    default:
      assert(0 && "get_next_search_move: stage problem");
      return 0;

  }
}

/*}}}*/

/*{{{  init_next_qsearch_move*/

static void init_next_qsearch_move(Node *const node, const move_t tt_move) {

  node->stage    = 0;
  node->in_check = 0;  //unused
  node->tt_move  = tt_move;

}

/*}}}*/
/*{{{  get_next_qsearch_move*/

static move_t get_next_qsearch_move(Node *const node) {

  switch (node->stage) {

    case 0: {
      /*{{{  tt*/
      
      node->stage++;
      node->num_moves = 0;
      node->next_move = 0;
      
      if (node->tt_move)
        return node->tt_move;
      
      /*}}}*/
    }

    /* fall through */

    case 1: {
      /*{{{  gen noisy*/
      
      node->stage++;
      node->num_moves = 0;
      node->next_move = 0;
      
      gen_noisy(node);
      
      if (node->tt_move)
        remove_tt_move(node);
      
      rank_noisy(node);
      
      /*}}}*/
    }

    /* fall through */

    case 2: {
      /*{{{  next noisy*/
      
      if (node->next_move < node->num_moves) {
      
        return get_next_sorted_move(node);
      
      }
      
      return 0;
      
      /*}}}*/
    }

    default:
      assert(0 && "get_next_search_move: stage problem");
      return 0;

  }
}

/*}}}*/

/*}}}*/
/*{{{  move makers*/
//
// pre_ and post_ are in relation to is_attacked() in search() for example.
// The pre_ functions (called via make_move) update enough for is_attacked
// (the bitboards) and leave a trail in lazy.* for the the net_ and post_
// functions to do the rest of the work.
//

/*{{{  post_move*/

static void post_move(Position *const pos) {

  int rights    = pos->rights;
  int ep        = pos->ep;
  uint64_t hash = pos->hash;

  const int from_piece = lazy.arg0;
  const int from       = lazy.arg1;
  const int to         = lazy.arg2;
  const int zob_base   = from_piece << 6;

  pos->board[to]   = from_piece;
  pos->board[from] = EMPTY;

  hash ^= zob_pieces[zob_base | from] ^ zob_pieces[zob_base | to];

  hash ^= zob_ep[ep];
  ep   = 0;
  hash ^= zob_ep[ep];

  hash   ^= zob_rights[rights];
  rights &= rights_masks[from] & rights_masks[to];
  hash   ^= zob_rights[rights];

  hash ^= zob_stm;

  pos->hash   = hash;
  pos->stm    ^= 1;
  pos->rights = (uint8_t)rights;
  pos->ep     = (uint8_t)ep;
  pos->hmc    = min(UINT8_MAX, pos->hmc + 1);

}

/*}}}*/
/*{{{  pre_move*/

static void pre_move(Position *const pos, const move_t move) {

  const int stm        = pos->stm;
  const int from       = (move >> 6) & 0x3F;
  const int to         = move & 0x3F;
  const int from_piece = pos->board[from];
  const uint64_t mask  = (1ULL << from) ^ (1ULL << to);

  pos->all[from_piece] ^= mask;
  pos->colour[stm]     ^= mask;
  pos->occupied        ^= mask;

  lazy.post_func = post_move;
  lazy.net_func  = net_move;
  lazy.arg0      = from_piece;
  lazy.arg1      = from;
  lazy.arg2      = to;

}

/*}}}*/

/*{{{  post_capture*/

static void post_capture(Position *const pos) {

  int rights    = pos->rights;
  int ep        = pos->ep;
  uint64_t hash = pos->hash;

  const int from       = lazy.arg1;
  const int to         = lazy.arg2;
  const int from_piece = lazy.arg0;
  const int to_piece   = lazy.arg3;
  const int zob_base   = from_piece << 6;

  pos->board[from] = EMPTY;
  pos->board[to]   = from_piece;

  hash ^= zob_pieces[zob_base | from] ^ zob_pieces[zob_base | to] ^ zob_pieces[(to_piece << 6) | to];

  hash ^= zob_ep[ep];
  ep   = 0;
  hash ^= zob_ep[ep];

  hash   ^= zob_rights[rights];
  rights &= rights_masks[from] & rights_masks[to];
  hash   ^= zob_rights[rights];

  hash ^= zob_stm;

  pos->hash   = hash;
  pos->stm    ^= 1;
  pos->rights = (uint8_t)rights;
  pos->ep     = (uint8_t)ep;
  pos->hmc    = 0;

}

/*}}}*/
/*{{{  pre_capture*/

static void pre_capture(Position *const pos, const move_t move) {

  uint64_t *const RESTRICT colour = pos->colour;
  uint64_t *const RESTRICT all    = pos->all;
  uint8_t  *const RESTRICT board  = pos->board;

  const int stm          = pos->stm;
  const int opp          = stm ^ 1;
  const int from         = (move >> 6) & 0x3F;
  const int to           = move & 0x3F;
  const uint64_t from_bb = 1ULL << from;
  const uint64_t to_bb   = 1ULL << to;
  const int from_piece   = board[from];
  const int to_piece     = board[to];
  const uint64_t mask    = from_bb ^ to_bb;

  all[from_piece] ^= mask;
  colour[stm]     ^= mask;
  all[to_piece]   ^= to_bb;
  colour[opp]     ^= to_bb;
  pos->occupied   ^= from_bb;

  lazy.post_func = post_capture;
  lazy.net_func  = net_capture;
  lazy.arg0      = from_piece;
  lazy.arg1      = from;
  lazy.arg2      = to;
  lazy.arg3      = to_piece;

}

/*}}}*/

/*{{{  post_ep_push*/

static void post_ep_push(Position *const pos) {

  int ep        = pos->ep;
  uint64_t hash = pos->hash;

  const int from_piece = lazy.arg0;
  const int from       = lazy.arg1;
  const int to         = lazy.arg2;
  const int zob_base   = from_piece << 6;

  pos->board[to]   = from_piece;
  pos->board[from] = EMPTY;

  hash ^= zob_pieces[zob_base | from] ^ zob_pieces[zob_base | to];

  hash ^= zob_ep[ep];
  ep   = from + orth_offset[pos->stm];
  hash ^= zob_ep[ep];

  hash ^= zob_stm;

  pos->hash = hash;
  pos->stm  ^= 1;
  pos->ep   = (uint8_t)ep;
  pos->hmc  = 0;

}

/*}}}*/
/*{{{  pre_ep_push*/

static void pre_ep_push(Position *const pos, const move_t move) {

  const int stm        = pos->stm;
  const int from       = (move >> 6) & 0x3F;
  const int to         = move & 0x3F;
  const int from_piece = piece_index(PAWN, stm);
  const uint64_t mask  = (1ULL << from) ^ (1ULL << to);

  pos->all[from_piece] ^= mask;
  pos->colour[stm]     ^= mask;
  pos->occupied        ^= mask;

  lazy.post_func = post_ep_push;
  lazy.net_func  = net_move;
  lazy.arg0      = from_piece;
  lazy.arg1      = from;
  lazy.arg2      = to;

}

/*}}}*/

/*{{{  post_ep_capture*/

static void post_ep_capture(Position *const pos) {

  uint8_t *const RESTRICT board = pos->board;

  const int from_piece = lazy.arg0;
  const int from       = lazy.arg1;
  const int to         = lazy.arg2;
  const int opp_pawn   = lazy.arg3;
  const int pawn_sq    = lazy.arg4;
  const int zob_base   = from_piece << 6;

  int ep        = pos->ep;
  uint64_t hash = pos->hash;

  board[from]    = EMPTY;
  board[to]      = from_piece;
  board[pawn_sq] = EMPTY;

  hash ^= zob_pieces[zob_base | from] ^ zob_pieces[zob_base | to] ^ zob_pieces[(opp_pawn << 6) | pawn_sq];

  hash ^= zob_ep[ep];
  ep   = 0;
  hash ^= zob_ep[ep];

  hash ^= zob_stm;

  pos->hash = hash;
  pos->stm  ^= 1;
  pos->ep   = (uint8_t)ep;
  pos->hmc  = 0;

}

/*}}}*/
/*{{{  pre_ep_capture*/

static void pre_ep_capture(Position *const pos, const move_t move) {

  uint64_t *const RESTRICT all    = pos->all;
  uint64_t *const RESTRICT colour = pos->colour;

  const int stm          = pos->stm;
  const int opp          = stm ^ 1;
  const int from         = (move >> 6) & 0x3F;
  const int to           = move & 0x3F;
  const uint64_t from_bb = 1ULL << from;
  const uint64_t to_bb   = 1ULL << to;
  const int from_piece   = piece_index(PAWN, stm);
  const uint64_t mask    = from_bb ^ to_bb;
  const int opp_pawn     = piece_index(PAWN, opp);
  const int pawn_sq      = to + orth_offset[opp];
  const uint64_t pawn_bb = 1ull << pawn_sq;

  all[from_piece] ^= mask;
  colour[stm]     ^= mask;
  all[opp_pawn]   ^= pawn_bb;
  colour[opp]     ^= pawn_bb;
  pos->occupied   ^= mask ^ pawn_bb;

  lazy.post_func = post_ep_capture;
  lazy.net_func  = net_ep_capture;
  lazy.arg0      = from_piece;
  lazy.arg1      = from;
  lazy.arg2      = to;
  lazy.arg3      = opp_pawn;
  lazy.arg4      = pawn_sq;

}

/*}}}*/

/*{{{  post_castle*/

static void post_castle(Position *const pos) {

  uint8_t *const board = pos->board;

  int rights    = pos->rights;
  int ep        = pos->ep;
  uint64_t hash = pos->hash;

  const int from_piece   = lazy.arg0;
  const int from         = lazy.arg1;
  const int to           = lazy.arg2;
  const int rook         = lazy.arg3;
  const int rook_from_sq = lazy.arg4;
  const int rook_to_sq   = lazy.arg5;
  const int zob_base_k   = from_piece << 6;
  const int zob_base_r   = rook       << 6;

  board[from]         = EMPTY;
  board[to]           = from_piece;
  board[rook_from_sq] = EMPTY;
  board[rook_to_sq]   = rook;

  hash ^= zob_pieces[zob_base_k | from] ^ zob_pieces[zob_base_k | to] ^ zob_pieces[zob_base_r | rook_from_sq] ^ zob_pieces[zob_base_r | rook_to_sq];

  hash ^= zob_ep[ep];
  ep   = 0;
  hash ^= zob_ep[ep];

  hash   ^= zob_rights[rights];
  rights &= rights_masks[from] & rights_masks[to];
  hash   ^= zob_rights[rights];

  hash ^= zob_stm;

  pos->hash   = hash;
  pos->stm    ^= 1;
  pos->rights = (uint8_t)rights;
  pos->ep     = (uint8_t)ep;
  pos->hmc    = min(UINT8_MAX, pos->hmc + 1);

}

/*}}}*/
/*{{{  pre_castle*/

static void pre_castle(Position *const pos, const move_t move) {

  uint64_t *const all    = pos->all;
  uint64_t *const colour = pos->colour;

  const int from              = (move >> 6) & 0x3F;
  const int to                = move & 0x3F;
  const uint64_t from_bb      = 1ULL << from;
  const uint64_t to_bb        = 1ULL << to;
  const int stm               = pos->stm;
  const int from_piece        = piece_index(KING, stm);
  const int rook              = piece_index(ROOK, stm);
  const uint64_t rook_from_bb = 1ULL << rook_from[to];
  const uint64_t rook_to_bb   = 1ULL << rook_to[to];
  const int rook_from_sq      = rook_from[to];
  const int rook_to_sq        = rook_to[to];
  const uint64_t mask_k       = from_bb ^ to_bb;
  const uint64_t mask_r       = rook_from_bb ^ rook_to_bb;

  all[from_piece] ^= mask_k;
  all[rook]       ^= mask_r;
  colour[stm]     ^= mask_k ^ mask_r;
  pos->occupied   ^= mask_k ^ mask_r;

  lazy.post_func = post_castle;
  lazy.net_func  = net_castle;
  lazy.arg0      = from_piece;
  lazy.arg1      = from;
  lazy.arg2      = to;
  lazy.arg3      = rook;
  lazy.arg4      = rook_from_sq;
  lazy.arg5      = rook_to_sq;

}

/*}}}*/

/*{{{  post_promo_push*/

static void post_promo_push(Position *const pos) {

  int ep        = pos->ep;
  uint64_t hash = pos->hash;

  const int from_piece = lazy.arg0;
  const int from       = lazy.arg1;
  const int to         = lazy.arg2;
  const int pro        = lazy.arg4;

  pos->board[from] = EMPTY;
  pos->board[to]   = pro;

  hash ^= zob_pieces[(from_piece << 6) | from] ^ zob_pieces[(pro << 6) | to];

  hash ^= zob_ep[ep];
  ep   = 0;
  hash ^= zob_ep[ep];

  hash ^= zob_stm;

  pos->hash = hash;
  pos->stm  ^= 1;
  pos->ep   = (uint8_t)ep;
  pos->hmc  = 0;

}

/*}}}*/
/*{{{  pre_promo_push*/

static void pre_promo_push(Position *const pos, const move_t move) {

  uint64_t *const all    = pos->all;
  uint64_t *const colour = pos->colour;

  const int from         = (move >> 6) & 0x3F;
  const int to           = move & 0x3F;
  const uint64_t from_bb = 1ULL << from;
  const uint64_t to_bb   = 1ULL << to;
  const int stm          = pos->stm;
  const int from_piece   = piece_index(PAWN, stm);
  const int pro          = piece_index(((move >> 12) & 3) + 1, stm);
  const uint64_t mask    = from_bb ^ to_bb;

  all[from_piece] ^= from_bb;
  all[pro]        ^= to_bb;
  colour[stm]     ^= mask;
  pos->occupied   ^= mask;

  lazy.post_func = post_promo_push;
  lazy.net_func  = net_promo_push;
  lazy.arg0      = from_piece;
  lazy.arg1      = from;
  lazy.arg2      = to;
  lazy.arg3      = EMPTY;
  lazy.arg4      = pro;

}

/*}}}*/

/*{{{  post_promo_capture*/

static void post_promo_capture(Position *const pos) {

  int rights    = pos->rights;
  int ep        = pos->ep;
  uint64_t hash = pos->hash;

  const int from_piece = lazy.arg0;
  const int from       = lazy.arg1;
  const int to         = lazy.arg2;
  const int to_piece   = lazy.arg3;
  const int pro        = lazy.arg4;

  pos->board[from] = EMPTY;
  pos->board[to]   = pro;

  hash ^= zob_pieces[from_piece << 6 | from] ^ zob_pieces[to_piece << 6 | to] ^ zob_pieces[pro << 6 | to];

  hash ^= zob_ep[ep];
  ep   = 0;
  hash ^= zob_ep[ep];

  hash   ^= zob_rights[rights];
  rights &= rights_masks[from] & rights_masks[to];
  hash   ^= zob_rights[rights];

  hash ^= zob_stm;

  pos->hash   = hash;
  pos->stm    ^= 1;
  pos->rights = (uint8_t)rights;
  pos->ep     = (uint8_t)ep;
  pos->hmc    = 0;

}

/*}}}*/
/*{{{  pre_promo_capture*/

static void pre_promo_capture(Position *const pos, const move_t move) {

  uint64_t *const RESTRICT all    = pos->all;
  uint64_t *const RESTRICT colour = pos->colour;

  const int from         = (move >> 6) & 0x3F;
  const int to           = move & 0x3F;
  const uint64_t from_bb = 1ULL << from;
  const uint64_t to_bb   = 1ULL << to;
  const int stm          = pos->stm;
  const int from_piece   = piece_index(PAWN, stm);
  const int to_piece     = pos->board[to];
  const int pro          = piece_index(((move >> 12) & 3) + 1, stm);

  all[from_piece] ^= from_bb;
  all[to_piece]   ^= to_bb;
  all[pro]        ^= to_bb;
  colour[stm]     ^= from_bb ^ to_bb;
  colour[stm^1]   ^= to_bb;
  pos->occupied   ^= from_bb;

  lazy.post_func = post_promo_capture;
  lazy.net_func  = net_promo_capture;
  lazy.arg0      = from_piece;
  lazy.arg1      = from;
  lazy.arg2      = to;
  lazy.arg3      = to_piece;
  lazy.arg4      = pro;

}

/*}}}*/

/*{{{  init_move_funcs*/

static void init_move_funcs(void) {

  move_funcs[0]  = pre_move;
  move_funcs[1]  = pre_move;
  move_funcs[2]  = pre_castle;
  move_funcs[3]  = pre_ep_push;
  move_funcs[4]  = pre_promo_push;
  move_funcs[5]  = pre_promo_push;
  move_funcs[6]  = pre_promo_push;
  move_funcs[7]  = pre_promo_push;
  move_funcs[8]  = pre_capture;
  move_funcs[9]  = pre_ep_capture;
  move_funcs[10] = pre_capture;
  //spare;
  move_funcs[12] = pre_promo_capture;
  move_funcs[13] = pre_promo_capture;
  move_funcs[14] = pre_promo_capture;
  move_funcs[15] = pre_promo_capture;

}

/*}}}*/
/*{{{  make_move_pre*/

static inline void make_move_pre(Position *const pos, const move_t move) {

  const int mt = (move >> 12) & 0xF;

  move_funcs[mt](pos, move);

  /* switch is slower
  switch (mt) {
    case 0:  pre_move(pos, move); break;
    case 1:  pre_move(pos, move); break;
    case 2:  pre_castle(pos, move); break;
    case 3:  pre_push(pos, move); break;
    case 4:  pre_promo_push(pos, move); break;
    case 5:  pre_promo_push(pos, move); break;
    case 6:  pre_promo_push(pos, move); break;
    case 7:  pre_promo_push(pos, move); break;
    case 8:  pre_capture(pos, move); break;
    case 9:  pre_ep_capture(pos, move); break;
    case 10: pre_capture(pos, move); break;
    case 11: break;
    case 12: pre_promo_capture(pos, move); break;
    case 13: pre_promo_capture(pos, move); break;
    case 14: pre_promo_capture(pos, move); break;
    case 15: pre_promo_capture(pos, move); break;
  }
  */

}

/*}}}*/
/*{{{  make_move_post*/
//
// Call the _post func set up by the _pre_func.
//

static inline void make_move_post(Position *const pos) {

  lazy.post_func(pos);

}


/*}}}*/

/*{{{  make_null_move*/

static void make_null_move(Position *const pos) {

  int ep        = pos->ep;
  uint64_t hash = pos->hash;

  hash ^= zob_ep[ep];
  ep   = 0;
  hash ^= zob_ep[ep];

  hash ^= zob_stm;

  pos->hash = hash;
  pos->stm  ^= 1;
  pos->ep   = (uint8_t)ep;
  pos->hmc  = 0;

}

/*}}}*/

/*}}}*/
/*{{{  debug*/

/*{{{  debug_slow_board_check*/

static int debug_slow_board_check(Node *const n1) {

  Node n;
  Node *n2 = &n;

  Position *const pos = &n1->pos;

  /*{{{  check ue*/
  
  net_copy(n1, n2);                      // copy the ue accumulators into n2
  net_slow_rebuild_accs(n1);             // rebuild the n1 accumulators from scratch
  
  for (int i=0; i < NET_H1_SIZE; i++) {  // compare them
    if (n1->acc1[i] != n2->acc1[i]) {
      fprintf(stderr, "a1 %d\n", i);
      return 1;
    }
    if (n1->acc2[i] != n2->acc2[i]) {
      fprintf(stderr, "a2 %d\n", i);
      return 1;
    }
  }
  
  /*}}}*/
  /*{{{  check bb == board*/
  
  uint64_t all[12] = {0};
  uint64_t colour[2] = {0};
  uint64_t occupied = 0;
  
  for (int sq=0; sq < 64; sq++) {
    int piece = pos->board[sq];
    if (piece == EMPTY) continue;
    all[piece]                         |= (1ULL << sq);
    colour[piece >= 6 ? BLACK : WHITE] |= (1ULL << sq);
    occupied                           |= (1ULL << sq);
  }
  
  for (int p = 0; p < 12; p++) {
    if (all[p] != pos->all[p]) {
      fprintf(stderr, "verify_from_board: mismatch in all[%d]\n", p);
      return 1;
    }
  }
  
  if (colour[WHITE] != pos->colour[WHITE]) {
    fprintf(stderr, "verify_from_board: mismatch in colour[WHITE]\n");
    return 1;
  }
  
  if (colour[BLACK] != pos->colour[BLACK]) {
    fprintf(stderr, "verify_from_board: mismatch in colour[BLACK]\n");
    return 1;
  }
  
  if (occupied != pos->occupied) {
    fprintf(stderr, "verify_from_board: mismatch in occupied\n");
    return 1;
  }
  
  /*}}}*/
  /*{{{  check board == bb*/
  
  uint8_t board[64];
  
  for (int sq=0; sq < 64; sq++)
    board[sq] = EMPTY;
  
  for (int p=0; p < 12; p++) {
    uint64_t bb = pos->all[p];
    while (bb) {
      int sq = __builtin_ctzll(bb);
      board[sq] = p;
      bb &= bb - 1;
    }
  }
  
  for (int sq=0; sq < 64; sq++) {
    if (board[sq] != pos->board[sq]) {
      fprintf(stderr, "verify_from_bitboards: mismatch at square %d (expected %d got %d)\n",
                      sq, pos->board[sq], board[sq]);
      return 1;
    }
  }
  
  /*}}}*/
  /*{{{  check hash*/
  
  uint64_t hash = 0;
  
  for (int sq=0; sq < 64; sq++) {
    int piece = pos->board[sq];
    if (piece == EMPTY) continue;
    hash ^= zob_pieces[zob_index(piece, sq)];
  }
  
  hash ^= zob_rights[n1->pos.rights];
  hash ^= zob_ep[n1->pos.ep];
  
  if (n1->pos.stm == BLACK)
    hash ^= zob_stm;
  
  if (hash != n1->pos.hash) {
    return 1;
    fprintf(stderr, "inc %" PRIx64 " rebuilt %" PRIx64 "\n", n1->pos.hash, hash);
  }
  
  /*}}}*/

  return 0;

}

/*}}}*/

/*}}}*/
/*{{{  board*/

/*{{{  ucinewgame*/

static void ucinewgame(void) {

  memset(ss, 0, sizeof(ss));

  if (!tt)
    init_tt(TT_DEFAULT);

  tt_reset();

}

/*}}}*/
/*{{{  init_zob*/

static void init_zob(void) {

  for (int i=0; i < 12*64; i++)
    zob_pieces[i] = rand64();

  zob_stm = rand64();

  for (int i=0; i < 16; i++)
    zob_rights[i] = rand64();

  for (int i=0; i < 64; i++)
    zob_ep[i] = rand64();

  zob_ep[0]     = 0;
  zob_rights[0] = 0;

}

/*}}}*/
/*{{{  mat_draw*/

static int mat_draw(const Position *const pos) {

  const uint64_t *const a = pos->all;
  const int num_pieces    = popcount(pos->occupied);

  if (num_pieces == 2)
    return 1;

  else if (num_pieces == 3 && (a[WKNIGHT] | a[BKNIGHT] | a[WBISHOP] | a[BBISHOP]))
    return 1;

  return 0;

}

/*}}}*/
/*{{{  eval*/

static inline int eval(Node *const node) {

  if (mat_draw(&node->pos))
    return 0;

  return net_eval(node);

}

/*}}}*/
/*{{{  play_move*/

static void play_move(Node *const node, char *uci_move) {

  char buf[6];

  Position *const pos   = &node->pos;
  const int stm         = pos->stm;
  const int opp         = stm ^ 1;
  const int stm_king_sq = piece_index(KING, stm);
  const int in_check    = is_attacked(pos, bsf(pos->all[stm_king_sq]), opp);
  move_t move           = 0;

  init_next_search_move(node, in_check, 0);

  while ((move = get_next_search_move(node))) {

    format_move(move, buf);
    if (!strcmp(uci_move, buf)) {
      make_move_pre(pos, move);
      make_move_post(pos);
      net_update_accs(node);
      return;
    }
  }

  fprintf(stderr, "info cannot find uci move %s\n", uci_move);

}

/*}}}*/
/*{{{  position*/

static void position(Node *const node, const char *board_fen, const char *stm_str, const char *rights_str, const char *ep_str, int num_uci_moves, char **uci_moves) {

  Position *const pos = &node->pos;

  static const int char_to_piece[128] = {
    ['p'] = 0, ['n'] = 1, ['b'] = 2, ['r'] = 3, ['q'] = 4, ['k'] = 5,
  };

  memset(pos, 0, sizeof(Position));

  /*{{{  board*/
  
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
  
      pos->all[index]     |= bb;
      pos->occupied       |= bb;
      pos->colour[colour] |= bb;
  
      pos->board[sq] = index;
  
      pos->hash ^= zob_pieces[zob_index(index, sq)];
  
      sq++;
  
    }
  }
  
  /*}}}*/
  /*{{{  stm*/
  
  pos->stm = (stm_str[0] == 'w') ? WHITE : BLACK;
  
  if (pos->stm == BLACK)
    pos->hash ^= zob_stm;
  
  /*}}}*/
  /*{{{  rights*/
  
  pos->rights = 0;
  
  for (const char *p = rights_str; *p; ++p) {
    switch (*p) {
      case 'K': pos->rights |= WHITE_RIGHTS_KING;  break;
      case 'Q': pos->rights |= WHITE_RIGHTS_QUEEN; break;
      case 'k': pos->rights |= BLACK_RIGHTS_KING;  break;
      case 'q': pos->rights |= BLACK_RIGHTS_QUEEN; break;
    }
  }
  
  pos->hash ^= zob_rights[pos->rights];
  
  /*}}}*/
  /*{{{  ep*/
  
  if (ep_str[0] == '-') {
    pos->ep = 0;  // 0 is not a legal ep square so this is ok
  }
  
  else {
  
    int file = ep_str[0] - 'a';
    int rank = ep_str[1] - '1';
  
    pos->ep = rank * 8 + file;
  
  }
  
  pos->hash ^= zob_ep[pos->ep];
  
  /*}}}*/

  net_slow_rebuild_accs(node);

  /*{{{  play the uci moves*/
  
  reset_hash_history(0);
  update_hash_history(pos, 0);
  
  for (int m=1; m <= num_uci_moves; m++) {
    play_move(node, uci_moves[m-1]);
    update_hash_history(pos, m);
  }
  
  reset_hash_history(num_uci_moves);
  
  /*}}}*/

  reset_piece_to_history();
  reset_killers();

}

/*}}}*/
/*{{{  et*/

static void et (void) {

  const int num_fens = 50;

  for (int i=0; i < num_fens; i++) {

    const Bench *b = &bench_data[i];
    position(&ss[0], b->fen, b->stm, b->rights, b->ep, 0, NULL);
    int e = net_eval(&ss[0]);
    printf("%d %s %s %s %s\n", e, b->fen, b->stm, b->rights, b->ep);

  }
}

/*}}}*/
/*{{{  probably_legal*/

static move_t probably_legal(const Position *const pos, move_t move) {

  if (!move) {
    return 0;
  }

  const int from       = (move >> 6) & 0x3F;
  const int to         = move & 0x3F;
  const int from_piece = pos->board[from];
  const int to_piece   = pos->board[to];
  const int stm        = pos->stm;

  if (from_piece == EMPTY){
    return 0;
  }

  if (from_piece/6 != stm) {
    return 0;
  }

  if (to_piece != EMPTY && to_piece/6 == stm) {
    return 0;
  }

  return move;

}

/*}}}*/
/*{{{  is_draw*/

static int is_draw(const Position *const pos, const int ply) {

  const int hmc = pos->hmc;

  if (hmc >= 100) {
    return 1;
  }

  if (mat_draw(pos)) {
    return 1;
  }

  if (hmc < 4) {
    return 0;
  }

  const uint64_t *const h = hh.hash;
  const int n_uci         = hh.num_uci_moves;
  const int start         = n_uci + ply;
  const uint64_t h0       = h[start];
  const int limit         = start - hmc;
  int idx                 = start - 4;
  int reps                = 0;

  while (idx >= limit) {

    assert(idx >= 0);
    assert(idx < 1024);

    if (h0 == h[idx]) {
      /*{{{  rep*/
      
      if (idx > n_uci) {
        return 1;
      }
      
      reps++;
      
      if (reps == 2) {
        return 1;
      }
      
      /*}}}*/
    }

    idx -= 2;

  }

  return 0;

}

/*}}}*/
/*{{{  pos_copy*/

static inline void pos_copy(const Position *const from_pos, Position *const to_pos) {

  *to_pos = *from_pos;

}

/*}}}*/
/*{{{  see*/

/*{{{  get_least_valuable_piece*/

static uint64_t get_least_valuable_piece(const Position *const pos, const uint64_t attadef, const int by_side, int *piece) {

  const int base = by_side ? 6 : 0;

  if ((attadef & pos->colour[by_side]) == 0)
    return 0;

  for (int i=base; i < base + 6; i++) {
    uint64_t subset = attadef & pos->all[i];
    if (subset) {
      *piece = i;
      return subset & -subset;
    }
  }

  return 0;

}

/*}}}*/
/*{{{  rook_attackers_to*/

static uint64_t rook_attackers_to(const Position *const pos, const uint64_t occ, const int to_sq) {

  const Attack *const RESTRICT a = &rook_attacks[to_sq];
  uint64_t rays                  = a->attacks[magic_index(occ & a->mask, a->magic, a->shift)];

  return rays & (pos->all[ROOK] | pos->all[6+ROOK] | pos->all[QUEEN] | pos->all[6+QUEEN]);

}

/*}}}*/
/*{{{  bishop_attackers_to*/

static uint64_t bishop_attackers_to(const Position *const pos, const uint64_t occ, const int to_sq) {

  const Attack *const RESTRICT a = &bishop_attacks[to_sq];
  uint64_t rays                  = a->attacks[magic_index(occ & a->mask, a->magic, a->shift)];

  return rays & (pos->all[BISHOP] | pos->all[6+BISHOP] | pos->all[QUEEN] | pos->all[6+QUEEN]);

}

/*}}}*/
/*{{{  static_attackers_to*/

static uint64_t static_attackers_to(const Position *const pos, const int to_sq) {

  uint64_t attackers = 0ULL;

  const uint64_t kn = knight_attacks[to_sq];

  attackers |= pos->all[KNIGHT]   & kn;
  attackers |= pos->all[6+KNIGHT] & kn;

  const uint64_t k = king_attacks[to_sq];

  attackers |= pos->all[KING]   & k;
  attackers |= pos->all[6+KING] & k;

  attackers |= pos->all[PAWN]   & pawn_attacks[WHITE][to_sq];
  attackers |= pos->all[6+PAWN] & pawn_attacks[BLACK][to_sq];

  return attackers;

}

/*}}}*/
/*{{{  see_ge*/

static int see_ge(const Position *const pos, const move_t move, int threshold) {

  if (!lut(lut_see, move))
    return 1;

  const int stm     = pos->stm;
  const int from_sq = (move >> 6) & 0x3F;
  const int to_sq   = move & 0x3F;
  int attacker      = pos->board[from_sq];
  const int target  = pos->board[to_sq];

  assert(target != EMPTY);

  if (see_values[target] - see_values[attacker] >= threshold)
    return 1;

  uint64_t used           = 0ULL;
  uint64_t occ            = pos->occupied;
  uint64_t from_set       = 1ULL << from_sq;
  const uint64_t stat_atk = static_attackers_to(pos, to_sq);
  uint64_t rook_atk       = rook_attackers_to(pos, occ, to_sq);
  uint64_t bish_atk       = bishop_attackers_to(pos, occ, to_sq);
  uint64_t attadef        = (stat_atk | rook_atk | bish_atk);

  if ((attadef & from_set) == 0)
    return 0;

  int gain[32], d = 0;
  gain[0] = see_values[target] - threshold;

  do {

    d++;
    gain[d] = see_values[attacker] - gain[d-1];

    if ((gain[d] < 0) && (-gain[d-1] < 0))
      break;

    used    |= from_set;
    attadef ^= from_set;
    occ     ^= from_set;

    const uint64_t moved_bb = from_set;

    if (moved_bb & (rank_mask[to_sq] | file_mask[to_sq]))
      rook_atk = rook_attackers_to(pos, occ, to_sq);

    if (moved_bb & (diag_mask[to_sq] | anti_mask[to_sq]))
      bish_atk = bishop_attackers_to(pos, occ, to_sq);

    attadef  = (stat_atk | rook_atk | bish_atk) & ~used;
    from_set = get_least_valuable_piece(pos, attadef, (stm ^ (d & 1)), &attacker);

  } while (from_set);

  while (--d)
    gain[d-1] = -(( -gain[d-1] > gain[d]) ? -gain[d-1] : gain[d]);

  return gain[0] >= 0;

}

/*}}}*/

/*}}}*/
/*{{{  is_pawn_endgame*/

static inline int is_pawn_endgame(const Position *const pos) {

  const uint64_t *const a = pos->all;

  return pos->occupied == (a[WKING] | a[WPAWN] | a[BKING] | a[BPAWN]);

}

/*}}}*/

/*}}}*/
/*{{{  search*/

/*{{{  collect_pv*/

static inline void collect_pv(Node *const this_node, const Node *const next_node, const move_t move) {

  memcpy(this_node->pv, next_node->pv, (size_t)next_node->pv_len * sizeof(move_t));

  this_node->pv_len                  = next_node->pv_len;
  this_node->pv[this_node->pv_len++] = move;

}

/*}}}*/
/*{{{  qsearch*/

static int qsearch(const int ply, int alpha, const int beta) {

  assert(ply < MAX_PLY);

  Node *const RESTRICT this_node = &ss[ply];
  const Position *const this_pos = &this_node->pos;
  this_node->pv_len              = 0;

  /*{{{  run out of ply*/
  
  if (ply >= SAFE_PLY) {
  
    return eval(this_node);
  
  }
  
  /*}}}*/
  /*{{{  run out of time*/
  
  tc.nodes++;
  
  if ((tc.nodes & 1023) == 0) {
  
    check_tc();
  
    if (tc.finished)
      return 0;
  
  }
  
  /*}}}*/
  /*{{{  stand pat*/
  
  const int ev = eval(this_node);
  
  if (ev >= beta)
    return beta;
  
  if (ev > alpha)
    alpha = ev;
  
  /*}}}*/
  /*{{{  tt*/
  
  const TT *const RESTRICT entry = tt_get(this_pos);
  
  if (entry) {
  
    const int flags = entry->flags;
    const int score = tt_get_adjusted_score(ply, entry->score);
  
    if (flags == TT_EXACT || (flags == TT_BETA && score >= beta) || (flags == TT_ALPHA && score <= alpha)) {
      return score;
    }
  
  }
  
  const move_t tt_move = entry ? probably_legal(this_pos, entry->move) : 0;
  
  /*}}}*/

  Node *const RESTRICT next_node          = &ss[ply+1];
  Position *const next_pos                = &next_node->pos;
  const int stm                           = this_pos->stm;
  const int opp                           = stm ^ 1;
  const int stm_king_idx                  = piece_index(KING, stm);
  const uint64_t *const next_stm_king_ptr = &next_pos->all[stm_king_idx];
  move_t move                             = 0;

  init_next_qsearch_move(this_node, tt_move);

  while ((move = get_next_qsearch_move(this_node))) {

    /*{{{  prune*/
    
    if (!see_ge(this_pos, move, -50)) {
    
      continue;
    
    }
    
    /*}}}*/
    /*{{{  copy make*/
    
    pos_copy(this_pos, next_pos);
    make_move_pre(next_pos, move);
    
    if (is_attacked(next_pos, bsf(*next_stm_king_ptr), opp))
      continue;
    
    make_move_post(next_pos);
    net_copy(this_node, next_node);
    net_update_accs(next_node);
    
    /*}}}*/

    const int score = -qsearch(ply+1, -beta, -alpha);

    if (score >= beta) {
      tt_put(this_pos, TT_BETA, 0, tt_put_adjusted_score(ply, score), move);
      return score;
    }

    if (score > alpha) {
      alpha = score;
    }
  }

  return alpha;

}

/*}}}*/
/*{{{  search*/

static int search(const int ply, int depth, int alpha, const int beta) {

  assert(ply < MAX_PLY);

  Node *const RESTRICT this_node = &ss[ply];
  const Position *const this_pos = &this_node->pos;
  this_node->pv_len              = 0;

  /*{{{  run out of ply*/
  
  if (ply >= SAFE_PLY) {
  
    return eval(this_node);
  
  }
  
  /*}}}*/

  const int stm          = this_pos->stm;
  const int opp          = stm ^ 1;
  const int stm_king_idx = piece_index(KING, stm);
  const int in_check     = is_attacked(this_pos, bsf(this_pos->all[stm_king_idx]), opp);
  const int is_root      = ply == 0;
  const int is_pv        = is_root || (beta - alpha != 1);

  this_node->in_check = in_check;

  /*{{{  horizon*/
  
  if (depth <= 0 && in_check == 0) {
  
    return qsearch(ply, alpha, beta);
  
  }
  
  depth = max(0, depth);
  
  /*}}}*/
  /*{{{  run out of time*/
  
  tc.nodes++;
  
  if ((tc.nodes & 1023) == 0) {
  
    check_tc();
  
    if (tc.finished)
      return 0;
  
  }
  
  /*}}}*/
  /*{{{  draw*/
  
  if (!is_root && is_draw(this_pos, ply)) {
  
    return 0;
  
  }
  
  /*}}}*/
  /*{{{  tt*/
  
  const TT *const RESTRICT entry = tt_get(this_pos);
  
  if (!is_pv && entry && entry->depth >= depth) {
  
    const int flags = entry->flags;
    const int score = tt_get_adjusted_score(ply, entry->score);
  
    if (flags == TT_EXACT || (flags == TT_BETA && score >= beta) || (flags == TT_ALPHA && score <= alpha)) {
      return score;
    }
  
  }
  
  const move_t tt_move = entry ? probably_legal(this_pos, entry->move) : 0;
  
  /*}}}*/
  /*{{{  iir*/
  /*
  if (!is_root && is_pv && !tt_move && depth > 9) {
  
    depth--;  // https://www.talkchess.com/forum3/viewtopic.php?f=7&t=74769
  
  }
  */
  /*}}}*/

  Node *const RESTRICT next_node = &ss[ply + 1];
  Position *const next_pos       = &next_node->pos;
  const int orig_alpha           = alpha;
  const int ev                   = eval(this_node);
  this_node->ev                  = ev;
  int r                          = 0;
  int e                          = 0;
  const int improving            = (ply >= 2 && !ss[ply-2].in_check) ? ev > ss[ply-2].ev : 0;

  /*{{{  beta prune*/
  
  if (!is_pv && !in_check && depth <= 8 && (ev - depth * 100) >= (beta - improving * 50)) {
  
    return ev;
  
  }
  
  /*}}}*/
  /*{{{  nmp*/
  
  if (!is_pv && !in_check && depth > 2 && ev > beta && !is_pawn_endgame(this_pos)) {
  
    r = 3;
  
    pos_copy(this_pos, next_pos);
    make_null_move(next_pos);
    net_copy(this_node, next_node);
  
    const int score = -search(ply+1, depth-1-r, -beta, -beta+1);
  
    if (score >= beta)
      return score > MATE_LIMIT ? beta : score;
  
    if (tc.finished)
      return 0;
  
  }
  
  /*}}}*/
  /*{{{  alpha prune*/
  
  if (!is_root && !in_check && depth <= 4 && alpha > -MATE_LIMIT && ev + 500 * depth <= alpha) {
  
    const int score = qsearch(ply, alpha, alpha + 1);
  
    if (score <= alpha) {
      return score;
    }
  
    this_node->pv_len = 0;
    this_node->ev     = ev;
  
  }
  
  /*}}}*/

  move_t move                             = 0;
  move_t best_move                        = 0;
  int score                               = alpha;
  int best_score                          = alpha;
  int num_legal_moves                     = 0;
  int num_prunable_moves                  = 0;
  const uint64_t *const next_stm_king_ptr = &next_pos->all[stm_king_idx];

  init_next_search_move(this_node, in_check, tt_move);

  while ((move = get_next_search_move(this_node))) {

    const int prunable = lut(lut_prune, move);

    /*{{{  prune*/
    
    if (prunable && !is_pv && !in_check && alpha > -MATE_LIMIT) {
    
      if (depth <= 2 && num_prunable_moves > 5 * depth) {
        continue;
      }
    
      if (num_legal_moves > 0 && depth <= 4 && (ev + depth * 120) < alpha) {
        continue;
      }
    
    }
    
    /*}}}*/
    /*{{{  copy make*/
    
    pos_copy(this_pos, next_pos);
    make_move_pre(next_pos, move);
    
    if (is_attacked(next_pos, bsf(*next_stm_king_ptr), opp))
      continue;
    
    make_move_post(next_pos);
    net_copy(this_node, next_node);
    net_update_accs(next_node);
    update_hash_history(next_pos, ply+1);
    
    /*}}}*/

    num_legal_moves++;
    if (prunable)
      num_prunable_moves++;

    /*{{{  search*/
    
    /*{{{  e/r*/
    
    e = 0;
    r = 0;
    
    if (in_check) {
    
      e = 1;
    
    }
    
    else if (depth >= 3 && num_legal_moves >= 6) {
    
      r = depth / 5 + num_legal_moves / 20;
    
    }
    
    /*}}}*/
    
    const int null_window = (is_pv && num_legal_moves > 1) || r;
    score                 = alpha;
    
    if (null_window) {
      score = -search(ply+1, depth+e-1-r, -alpha-1, -alpha);
    }
    
    if (tc.finished)
      return 0;
    
    if (score > alpha || !null_window) {
      score = -search(ply+1, depth+e-1, -beta, -alpha);
    }
    
    if (tc.finished)
      return 0;
    
    /*}}}*/
    /*{{{  alpha raise*/
    
    if (score > alpha) {
    
      alpha      = score;
      best_score = score;
      best_move  = move;
    
      if (is_root) {
    
        tc.bm = best_move;
        tc.bs = best_score;
    
      }
    
      if (is_pv) {
    
        collect_pv(this_node, next_node, move);
    
      }
    
    }
    
    /*}}}*/
    /*{{{  beta raise*/
    
    if (score >= beta) {
    
      /*{{{  update piece-to history*/
      
      if (lut(lut_history, move)) {
      
        const int bonus = depth * depth;
        const int limit = this_node->next_move - 1;
      
        update_piece_to_history(this_pos, move, bonus);
      
        for (int i=0; i < limit; i++) {
      
          const move_t pmove = this_node->moves[i];
      
          assert(move != pmove && "search: last move in limit");
      
          if (lut(lut_history, pmove)) {
            update_piece_to_history(this_pos, pmove, -bonus);
          }
      
        }
      
      }
      
      /*}}}*/
      /*{{{  update killers*/
      
      if (move != tt_move && lut(lut_killer, move)) {
      
        update_killer(this_node, move);
      
      }
      
      /*}}}*/
    
      tt_put(this_pos, TT_BETA, depth, tt_put_adjusted_score(ply, best_score), best_move);
    
      return score;
    
    }
    
    /*}}}*/

  }

  /*{{{  only one move*/
  
  if (is_root && num_legal_moves == 1) {
  
    tc.finished = 1;
  
  }
  
  /*}}}*/
  /*{{{  mate or slatemate*/
  
  if (num_legal_moves == 0) {
  
    return this_node->in_check ? (-MATE + ply) : 0;
  
  }
  
  /*}}}*/

  tt_put(this_pos, (alpha > orig_alpha ? TT_EXACT : TT_ALPHA), depth, tt_put_adjusted_score(ply, best_score), best_move);

  return best_score;

}

/*}}}*/
/*{{{  go*/
//
// This is Clockwork shaped with different magic numbers.
//

static void go(void) {

  char bm_str[6];
  char pv_str[MAX_PLY * 6 + 1];

  int alpha        = 0;
  int beta         = 0;
  int score        = 0;
  int delta        = 0;
  move_t *const pv = ss[0].pv;
  int pv_len       = ss[0].pv_len;
  int next_pv_char = 0;

  for (int depth=1; depth <= tc.max_depth; depth++) {

    alpha = -INF;
    beta  = INF;
    delta = 20;

    if (depth >= 4) {
      alpha = score - delta;
      beta  = score + delta;
    }

    /*{{{  asp window*/
    
    while (1) {
    
      score = search(0, depth, alpha, beta);
    
      if (tc.finished)
        break;
    
      if (score <= alpha) {
        alpha = score - delta;
      }
      else if (score >= beta) {
        beta  = score + delta;
      }
      else {
        break;
      }
    
      delta += delta;
    
    }
    
    /*}}}*/
    /*{{{  uci report*/
    
    pv_len       = ss[0].pv_len;
    next_pv_char = 0;
    
    for (int i=pv_len-1; i >= 0; i--) {
      next_pv_char += format_move(pv[i], &pv_str[next_pv_char]);
      pv_str[next_pv_char++] = ' ';
    }
    
    pv_str[next_pv_char] = '\0';
    
    uint64_t end_ms     = now_ms();
    uint64_t elapsed_ms = end_ms - tc.start_time;
    uint64_t nps        = (tc.nodes * 1000ULL) / (elapsed_ms ? elapsed_ms : 1);
    
    if (abs(tc.bs) < MATE_LIMIT)
      printf("info depth %d score cp %d time %" PRIu64 " nodes %" PRIu64 " nps %" PRIu64 " pv %s\n", depth, tc.bs, elapsed_ms, tc.nodes, nps, pv_str);
    else {
      const int mate_score = max(1, (tc.bs < 0) ? (tc.bs - MATE) / 2 : (MATE - tc.bs) / 2);
      printf("info depth %d score mate %d time %" PRIu64 " nodes %" PRIu64 " nps %" PRIu64 " pv %s\n", depth, mate_score, elapsed_ms, tc.nodes, nps, pv_str);
    }
    
    /*}}}*/

    if (tc.finished)
      break;

    age_piece_to_history();

  }

  format_move(tc.bm, bm_str);
  printf("bestmove %s\n", bm_str);

}

/*}}}*/
/*{{{  bench*/

static void bench (void) {

  ucinewgame();

  const int num_fens   = bench_data_len;
  uint64_t start_ms    = now_ms();
  uint64_t total_nodes = 0;

  for (int i=0; i < num_fens; i++) {

    const Bench *b = &bench_data[i];

    printf("%s %s %s %s\n", b->fen, b->stm, b->rights, b->ep);
    position(&ss[0], b->fen, b->stm, b->rights, b->ep, 0, NULL);

    tc           = (TimeControl){0};
    tc.max_depth = 12;

    go();

    total_nodes += tc.nodes;

  }

  uint64_t end_ms     = now_ms();
  uint64_t elapsed_ms = end_ms - start_ms;
  uint64_t nps        = (total_nodes * 1000ULL) / (elapsed_ms ? elapsed_ms : 1);

  printf("time %" PRIu64 " nodes %" PRIu64 " nps %" PRIu64 "\n", elapsed_ms, total_nodes, nps);

}

/*}}}*/

/*}}}*/
/*{{{  threads*/

static pthread_t search_thread;
static _Atomic int search_running = 0;

static void *go_thread_fn(void *arg) {
  (void)arg;
  go();
  atomic_store(&search_running, 0);
  return NULL;
}

static void join_search_if_running(void) {
  if (atomic_load(&search_running)) {
    atomic_store(&tc.finished, 1);
    pthread_join(search_thread, NULL);
    atomic_store(&search_running, 0);
  }
}

/*}}}*/
/*{{{  perft*/

/*{{{  perft*/

static uint64_t perft(const int ply, const int depth) {

  if (depth == 0)
    return 1;

  Node *const this_node                   = &ss[ply];
  Node *const next_node                   = &ss[ply+1];
  const Position *const this_pos          = &this_node->pos;
  Position *const next_pos                = &next_node->pos;
  const int stm                           = this_pos->stm;
  const int opp                           = stm ^ 1;
  const int stm_king_idx                  = piece_index(KING, stm);
  const int stm_king_sq                   = bsf(this_pos->all[stm_king_idx]);
  const int in_check                      = is_attacked(this_pos, stm_king_sq, opp);
  const uint64_t *const next_stm_king_ptr = &next_pos->all[stm_king_idx];
  uint64_t tot_nodes                      = 0;
  move_t move                             = 0;

  init_next_search_move(this_node, in_check, 0);

  while ((move = get_next_search_move(this_node))) {

    /*{{{  copy make*/
    
    pos_copy(this_pos, next_pos);
    make_move_pre(next_pos, move);
    
    if (is_attacked(next_pos, bsf(*next_stm_king_ptr), opp))
      continue;
    
    /*}}}*/

    lazy.post_func(next_pos);
    tot_nodes += perft(ply+1, depth-1);

  }

  return tot_nodes;

}

/*}}}*/
/*{{{  perft_tests*/

static void perft_tests (void) {

  const int num_tests  = perft_data_len;
  uint64_t start_ms    = now_ms();
  uint64_t total_nodes = 0;
  int errors           = 0;

  for (int i=0; i < num_tests; i++) {

    const Perft *p = &perft_data[i];

    position(&ss[0], p->fen, p->stm, p->rights, p->ep, 0, NULL);

    uint64_t num_nodes = perft(0, p->depth);
    total_nodes        += num_nodes;

    if (num_nodes != p->expected)
      errors += 1;

    printf("%s %s %d (%" PRIu64 ") %" PRIu64 " %" PRIu64 "\n",
      p->label,
      p->fen,
      p->depth,
      num_nodes - p->expected,
      num_nodes,
      p->expected
    );

  }

  uint64_t end_ms     = now_ms();
  uint64_t elapsed_ms = end_ms - start_ms;
  uint64_t nps        = (total_nodes * 1000ULL) / (elapsed_ms ? elapsed_ms : 1);

  printf("time %" PRIu64 " nps %" PRIu64 "\n", elapsed_ms, nps);
  printf("errors %d\n", errors);

}

/*}}}*/

/*}}}*/
/*{{{  init_once*/

static int init_once(void) {

  assert(INT16_MIN < -(MAX_HISTORY + 10));

  memset(ss, 0, sizeof(ss));

#ifndef NDEBUG
  uint64_t start_ms = now_ms();
#endif

  init_line_masks();
  init_move_funcs();
  init_rights_masks();
  init_zob();

  init_pawn_attacks();
  init_knight_attacks();
  init_bishop_attacks();

  init_rook_attacks();
  init_king_attacks();

  init_all_attacks();

#ifndef NDEBUG
  uint64_t elapsed_ms = now_ms() - start_ms;
  printf("init %zu\n", elapsed_ms);
#endif

  if (init_weights())
    return 1;

  assert(sizeof(Node)%64 == 0);
  assert(sizeof(Position)%64 == 0);
  assert(sizeof(Attack)%64 == 0);

  ASSERT_ALIGNED64(raw_attacks);
  ASSERT_ALIGNED64(ss);
  ASSERT_ALIGNED64(ss[0].acc1);
  ASSERT_ALIGNED64(ss[0].acc2);
  ASSERT_ALIGNED64(ss[1].acc1);
  ASSERT_ALIGNED64(ss[1].acc2);
  ASSERT_ALIGNED64(net_h1_w);
  ASSERT_ALIGNED64(net_h2_w);
  ASSERT_ALIGNED64(net_h1_b);
  ASSERT_ALIGNED64(net_o_w);

  return 0;

}

/*}}}*/
/*{{{  uci*/

/*{{{  uci_tokens*/

static int uci_tokens(int num_tokens, char **tokens) {

  if (num_tokens == 0)
    return 0;

  if (strlen(tokens[0]) == 0)
    return 0;

  const char *cmd = tokens[0];
  const char *sub = tokens[num_tokens > 1 ? 1 : 0];

  if (!strcmp(cmd, "isready")) {
    /*{{{  isready*/
    
    printf("readyok\n");
    
    /*}}}*/
  }
  else if (!strcmp(cmd, "position") || !strcmp(cmd, "p")) {
    /*{{{  position*/
    
    if (!tt) {
      printf("info run a ucinewgame command or setoption name Hash value 16 (etc) command first\n");
      return 0;
    }
    
    /*{{{  get pointer to moves and number of moves*/
    
    char **moves_pointer = NULL;
    int num_moves        = 0;
    int moves_index      = find_token("moves", num_tokens, tokens);
    
    if (moves_index != -1)
      num_moves = num_tokens - (moves_index + 1);
    
    if (num_moves > 0)
      moves_pointer = &tokens[moves_index + 1];
    
    else
      num_moves = 0;
    
    /*}}}*/
    
    if (!strcmp(sub, "startpos") || !strcmp(sub, "s"))
    
      position(&ss[0], "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR", "w", "KQkq", "-", num_moves, moves_pointer);
    
    else if (!strcmp(sub, "fen") || !strcmp(sub, "f"))
    
      position(&ss[0], tokens[2], tokens[3], tokens[4], tokens[5], num_moves, moves_pointer);
    
    /*}}}*/
  }
  else if (!strcmp(cmd, "go") || !strcmp(cmd, "g")) {
    /*{{{  go*/
    
    if (!tt) {
      printf("info run a ucinewgame command or setoption name Hash value 16 (etc) command first\n");
      return 0;
    }
    
    if (ss[0].pos.hash == 0) {
      printf("info run a position startpos (etc) command first\n");
      return 0;
    }
    
    int64_t wtime = 0;
    int64_t winc = 0;
    int64_t btime = 0;
    int64_t binc = 0;
    int64_t max_nodes = 0;
    int64_t move_time = 0;
    int max_depth = 0;
    int moves_to_go = 0;
    
    /*{{{  get the go params*/
    
    int t = 1;
    
    while (t < num_tokens - 1) {
    
      const char *token = tokens[t];
    
      if (!strcmp(token, "wtime")) {
        t++;
        wtime = atoi(tokens[t]);
      }
      else if (!strcmp(token, "winc")) {
        t++;
        winc = atoi(tokens[t]);
      }
      else if (!strcmp(token, "btime")) {
        t++;
        btime = atoi(tokens[t]);
      }
      else if (!strcmp(token, "binc")) {
        t++;
        binc = atoi(tokens[t]);
      }
      else if (!strcmp(token, "depth") || !strcmp(token, "d")) {
        t++;
        max_depth = atoi(tokens[t]);
      }
      else if (!strcmp(token, "nodes") || !strcmp(token, "n")) {
        t++;
        max_nodes = atoi(tokens[t]);
      }
      else if (!strcmp(token, "movetime") || !strcmp(token, "m")) {
        t++;
        move_time = atoi(tokens[t]);
      }
      else if (!strcmp(token, "movestogo")) {
        t++;
        moves_to_go = atoi(tokens[t]);
      }
    
      t++;
    
    }
    
    if (num_tokens == 2 && !strcmp(tokens[1], "infinite")) {
      max_depth = MAX_PLY;
    }
    
    /*}}}*/
    
    tc_init(wtime, winc, btime, binc, max_nodes, move_time, max_depth, moves_to_go);
    
    atomic_store(&tc.finished, 0);
    join_search_if_running();
    
    if (pthread_create(&search_thread, NULL, go_thread_fn, NULL) == 0) {
      atomic_store(&search_running, 1);
    }
    else {
      go();
    }
    
    /*}}}*/
  }
  else if (!strcmp(cmd, "stop")) {
    /*{{{  stop*/
    
    atomic_store(&tc.finished, 1);
    join_search_if_running();
    
    tc.finished = 1;
    
    /*}}}*/
  }
  else if (!strcmp(cmd, "ucinewgame") || !strcmp(cmd, "u")) {
    /*{{{  ucinewgame*/
    
    ucinewgame();
    
    /*}}}*/
  }
  else if (!strcmp(cmd, "uci")) {
    /*{{{  uci*/
    
    printf("id name Cwtch %s\n", VERSION);
    printf("id author Colin Jenkins\n");
    printf("option name Hash type spin default %d min 1 max 1024\n", TT_DEFAULT);
    printf("uciok\n");
    
    /*}}}*/
  }
  else if (!strcmp(cmd, "setoption")) {
    /*{{{  setoption*/
    
    if (!strcmp(tokens[2], "Hash")) {
    
      init_tt(atoi(tokens[4]));
    
    }
    
    /*}}}*/
  }
  else if (!strcmp(cmd, "board") || !strcmp(cmd, "b")) {
    /*{{{  board*/
    
    print_board(&ss[0]);
    
    /*}}}*/
  }
  else if (!strcmp(cmd, "hash") || !strcmp(cmd, "h")) {
    /*{{{  hash size*/
    
    const size_t mb = (tt_entries * sizeof(TT)) / (1024 * 2024);
    
    printf("%zu mb %zu slots %zu bytes/slot\n", mb, tt_entries, sizeof(TT));
    
    /*}}}*/
  }
  else if (!strcmp(cmd, "eval") || !strcmp(cmd, "e")) {
    /*{{{  eval*/
    
    const int e = eval(&ss[0]);
    
    printf("%d\n", e);
    
    /*}}}*/
  }
  else if (!strcmp(cmd, "net") || !strcmp(cmd, "n")) {
    /*{{{  net*/
    
    printf("(%d -> %d)x2 -> 1\n", NET_I_SIZE, NET_H1_SIZE);
    
    /*}}}*/
  }
  else if (!strcmp(cmd, "build")) {
    /*{{{  build*/
    
    printf("%s\n", BUILD);
    
    /*}}}*/
  }
  else if (!strcmp(cmd, "draw") || !strcmp(cmd, "d")) {
    /*{{{  draw*/
    
    printf("hmc %d num uci moves %d is draw %d\n", ss[0].pos.hmc, hh.num_uci_moves, is_draw(&ss[0].pos, 0));
    
    /*}}}*/
  }
  else if (!strcmp(cmd, "perft") || !strcmp(cmd, "f")) {
    /*{{{  perft*/
    
    const int depth = atoi(sub);
    uint64_t start_ms = now_ms();
    uint64_t total_nodes = 0;
    
    uint64_t num_nodes = perft(0, depth);
    total_nodes += num_nodes;
    
    uint64_t end_ms     = now_ms();
    uint64_t elapsed_ms = end_ms - start_ms;
    uint64_t nps        = (total_nodes * 1000ULL) / (elapsed_ms ? elapsed_ms : 1);
    
    printf("time %" PRIu64 " nodes %" PRIu64 " nps %" PRIu64 "\n", elapsed_ms, total_nodes, nps);
    
    /*}}}*/
  }
  else if (!strcmp(cmd, "bench") || !strcmp(cmd, "h")) {
    /*{{{  bench*/
    
    bench();
    
    /*}}}*/
  }
  else if (!strcmp(cmd, "et")) {
    /*{{{  eval tests*/
    
    et();
    
    /*}}}*/
  }
  else if (!strcmp(cmd, "pt")) {
    /*{{{  perft tests*/
    
    perft_tests();
    
    /*}}}*/
  }
  else if (!strcmp(cmd, "quit") || !strcmp(cmd, "q")) {
    /*{{{  quit*/
    
    atomic_store(&tc.finished, 1);
    join_search_if_running();
    
    return 1;
    
    /*}}}*/
  }
  else {
    printf("%s ?\n", cmd);
  }

  return 0;

}

/*}}}*/
/*{{{  uci_exec*/

static int uci_exec(char *line) {

  char *tokens[UCI_TOKENS];
  char *token;

  int num_tokens = 0;

  token = strtok(line, " \t\n");

  while (token != NULL && num_tokens < UCI_TOKENS) {

    tokens[num_tokens++] = token;
    token = strtok(NULL, " \r\t\n");

  }

  return uci_tokens(num_tokens, tokens);

}

/*}}}*/
/*{{{  uci_loop*/

static void uci_loop(int argc, char **argv) {

  setvbuf(stdout, NULL, _IONBF, 0);

  char chunk[UCI_LINE_LENGTH];

  for (int i=1; i < argc; i++) {
    if (uci_exec(argv[i]))
      return;
  }

  while (fgets(chunk, sizeof(chunk), stdin) != NULL) {
    if (uci_exec(chunk))
      return;
  }

}

/*}}}*/

/*}}}*/

/*{{{  main*/

int main(int argc, char **argv) {

  if (init_once()) {
    return 1;
  }

  uci_loop(argc, argv);

  free(tt);

  return 0;

}

/*}}}*/

