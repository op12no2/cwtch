#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "datagen.h"
#include "types.h"
#include "builtins.h"
#include "nodes.h"
#include "pos.h"
#include "move.h"
#include "makemove.h"
#include "tt.h"
#include "hh.h"
#include "timecontrol.h"
#include "go.h"
#include "position.h"
#include "iterate.h"
#include "net.h"

#define DG_RANDOM_PLIES   10
#define DG_SEARCH_NODES   5000
#define DG_DRAW_SCORE     10
#define DG_DRAW_COUNT     10
#define DG_DRAW_PLY       40
#define DG_MAX_GAME_MOVES 512
#define DG_REPORT_SECS    10
#define DG_FILE_PREFIX    "data"

// --- viriformat constants ---

#define VIRI_TYPE_NORMAL    0
#define VIRI_TYPE_EP        1
#define VIRI_TYPE_CASTLE    2
#define VIRI_TYPE_PROMO     3

#define VIRI_WDL_BLACK_WIN  0
#define VIRI_WDL_DRAW       1
#define VIRI_WDL_WHITE_WIN  2

// --- RNG (xorshift64, local to datagen) ---

static uint64_t dg_seed;

static void dg_seed_rng(void) {
  uint64_t x;
  dg_seed = time_ms() ^ (uint64_t)(uintptr_t)&x;
  if (!dg_seed) dg_seed = 1;
}

static uint64_t dg_rand(void) {
  dg_seed ^= dg_seed >> 12;
  dg_seed ^= dg_seed << 25;
  dg_seed ^= dg_seed >> 27;
  return dg_seed * 2685821657736338717ULL;
}

// --- viriformat: PackedBoard (32 bytes) ---

static void pos_to_packed_board(const Position *pos, uint8_t *buf, uint8_t wdl) {

  memset(buf, 0, 32);

  // bytes 0-7: occupancy
  uint64_t occ = pos->occupied;
  memcpy(buf, &occ, 8);

  // bytes 8-23: nibble-packed pieces
  // walk set bits of occupancy, for each encode a 4-bit nibble
  uint64_t tmp = occ;
  int idx = 0;
  while (tmp) {
    int sq = bsf(tmp);
    tmp &= tmp - 1;

    uint8_t piece = pos->board[sq];
    int color = piece_colour(piece);
    int type = piece_type(piece);

    // unmoved rook detection
    if (type == ROOK) {
      if (sq == H1 && (pos->rights & WHITE_RIGHTS_KING))  type = 6;
      if (sq == A1 && (pos->rights & WHITE_RIGHTS_QUEEN)) type = 6;
      if (sq == H8 && (pos->rights & BLACK_RIGHTS_KING))  type = 6;
      if (sq == A8 && (pos->rights & BLACK_RIGHTS_QUEEN)) type = 6;
    }

    uint8_t nibble = (color << 3) | type;
    int byte_idx = 8 + (idx / 2);
    if (idx & 1)
      buf[byte_idx] |= nibble << 4;
    else
      buf[byte_idx] = nibble;
    idx++;
  }

  // byte 24: stm_ep_square
  uint8_t ep = pos->ep ? pos->ep : 64;
  buf[24] = (pos->stm << 7) | (ep & 0x7F);

  // byte 25: halfmove clock
  buf[25] = pos->hmc;

  // bytes 26-27: fullmove number (0)
  // bytes 28-29: eval (0)

  // byte 30: wdl
  buf[30] = wdl;

  // byte 31: extra (0)

}

// --- viriformat: move conversion ---

static uint16_t move_to_viri(move_t move) {

  int from = (move >> 6) & 0x3F;
  int to = move & 0x3F;
  int type = VIRI_TYPE_NORMAL;
  int promo = 0;

  if (move & MOVE_FLAG_EPCAPTURE) {
    type = VIRI_TYPE_EP;
  }
  else if (move & MOVE_FLAG_CASTLE) {
    type = VIRI_TYPE_CASTLE;
    // convert king destination to rook square (king-takes-rook)
    if (to == G1) to = H1;
    else if (to == C1) to = A1;
    else if (to == G8) to = H8;
    else if (to == C8) to = A8;
  }
  else if (move & MOVE_FLAG_PROMOTE) {
    type = VIRI_TYPE_PROMO;
    // cwtch promo piece: bits 13-12 of flags area = piece type (KNIGHT=1..QUEEN=4)
    int piece = (move >> 12) & 0x7;
    promo = piece - 1; // KNIGHT=0, BISHOP=1, ROOK=2, QUEEN=3
  }

  return (uint16_t)(from | (to << 6) | (promo << 12) | (type << 14));

}

// --- viriformat: move+score entry ---

typedef struct {
  uint16_t move;
  int16_t score;
} ViriMove;

// --- legal move generation ---

static int gen_legal_moves(move_t *legal) {

  Node *node = &nodes[0];
  Position *pos = &node->pos;
  const int stm = pos->stm;
  const int opp = stm ^ 1;
  const int king_idx = piece_index(KING, stm);
  const int in_check = is_attacked(pos, bsf(pos->all[king_idx]), opp);

  init_next_search_move(node, in_check, 0);

  int count = 0;
  move_t move;
  Node tmp;

  while ((move = get_next_search_move(node))) {
    pos_copy(pos, &tmp.pos);
    make_move(&tmp, move);
    if (!is_attacked(&tmp.pos, bsf(tmp.pos.all[king_idx]), opp)) {
      legal[count++] = move;
    }
  }

  return count;

}

// --- play one game, return number of moves written ---

static int play_game(FILE *fp) {

  ViriMove entries[DG_MAX_GAME_MOVES];
  int num_entries = 0;
  move_t legal[MAX_MOVES];

  new_game();
  position(&nodes[0], "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR", "w", "KQkq", "-", 0, 0, NULL);

  // random opening: RANDOM_PLIES + 0 or 1 extra (to randomise stm)
  int num_random = DG_RANDOM_PLIES + (dg_rand() & 1);

  for (int i = 0; i < num_random; i++) {
    int n = gen_legal_moves(legal);
    if (n == 0)
      return 0;

    move_t pick = legal[dg_rand() % n];
    char buf[6];
    format_move(pick, buf);
    play_move(&nodes[0], buf);
    hh_push(nodes[0].pos.hash);
  }

  if (is_mat_draw(&nodes[0].pos))
    return 0;

  // save starting position (after random plies)
  Position start_pos;
  pos_copy(&nodes[0].pos, &start_pos);

  // scored play
  int draw_count = 0;
  uint8_t wdl = VIRI_WDL_DRAW;

  for (int ply = 0; ply < DG_MAX_GAME_MOVES; ply++) {

    Position *pos = &nodes[0].pos;
    const int stm = pos->stm;
    const int opp = stm ^ 1;
    const int king_idx = piece_index(KING, stm);
    const int in_check = is_attacked(pos, bsf(pos->all[king_idx]), opp);

    int n = gen_legal_moves(legal);

    if (n == 0) {
      if (in_check)
        wdl = (stm == WHITE) ? VIRI_WDL_BLACK_WIN : VIRI_WDL_WHITE_WIN;
      break;
    }

    // search
    init_tc(0, 0, 0, 0, DG_SEARCH_NODES, 0, 0, 0);
    time_control.best_move = 0;
    time_control.best_score = 0;
    go(1);

    move_t best = time_control.best_move;
    int score = time_control.best_score;

    if (!best)
      best = legal[0];

    int white_score = (stm == WHITE) ? score : -score;

    // record move + score
    if (num_entries < DG_MAX_GAME_MOVES) {
      entries[num_entries].move = move_to_viri(best);
      entries[num_entries].score = (int16_t)white_score;
      num_entries++;
    }

    // adjudication
    if (abs(score) <= DG_DRAW_SCORE)
      draw_count++;
    else
      draw_count = 0;

    if (draw_count >= DG_DRAW_COUNT && ply >= DG_DRAW_PLY) {
      wdl = VIRI_WDL_DRAW;
      break;
    }

    // play the move
    char buf[6];
    format_move(best, buf);
    play_move(&nodes[0], buf);
    hh_push(nodes[0].pos.hash);

    // draw checks
    hh_set_root();
    if (is_draw(0, nodes[0].pos.hash, nodes[0].pos.hmc) || is_mat_draw(&nodes[0].pos)) {
      wdl = VIRI_WDL_DRAW;
      break;
    }

  }

  if (num_entries == 0)
    return 0;

  // build complete game record in one buffer
  // 32 (PackedBoard) + 4 * num_entries (moves) + 4 (terminator)
  uint8_t buf[32 + DG_MAX_GAME_MOVES * 4 + 4];
  int len = 0;

  pos_to_packed_board(&start_pos, buf, wdl);
  len += 32;

  memcpy(buf + len, entries, 4 * num_entries);
  len += 4 * num_entries;

  memset(buf + len, 0, 4);
  len += 4;

  fwrite(buf, len, 1, fp);

  return num_entries;

}

// --- main datagen loop ---

void datagen(const char *directory, double hours) {

  dg_seed_rng();

  char filename[512];
  snprintf(filename, sizeof(filename), "%s/" DG_FILE_PREFIX "%llu.vf",
    directory, (unsigned long long)dg_rand());

  FILE *fp = fopen(filename, "wb");
  if (!fp) {
    printf("error: cannot open %s\n", filename);
    return;
  }

  static char iobuf[1 << 20];
  setvbuf(fp, iobuf, _IOFBF, sizeof(iobuf));

  printf("datagen: writing to %s for %g hours\n", filename, hours);

  uint64_t start_time = time_ms();
  uint64_t end_time = start_time + (uint64_t)(hours * 3600.0 * 1000.0);
  uint64_t total_positions = 0;
  uint64_t total_games = 0;
  uint64_t last_report = start_time;

  while (time_ms() < end_time) {

    int moves = play_game(fp);
    total_positions += moves;
    total_games++;

    uint64_t now = time_ms();
    if (now - last_report >= DG_REPORT_SECS * 1000) {
      uint64_t elapsed = now - start_time;
      uint64_t pps = elapsed ? (total_positions * 1000ULL / elapsed) : 0;
      uint64_t remaining_ms = end_time > now ? end_time - now : 0;
      int rem_h = (int)(remaining_ms / 3600000ULL);
      int rem_m = (int)((remaining_ms % 3600000ULL) / 60000ULL);
      printf("datagen: %llu positions %llu games %llu pos/s [%d:%02d left]\n",
        (unsigned long long)total_positions,
        (unsigned long long)total_games,
        (unsigned long long)pps,
        rem_h, rem_m);
      fflush(stdout);
      fflush(fp);
      last_report = now;
    }

  }

  fclose(fp);

  printf("datagen: done. %llu positions %llu games written to %s\n",
    (unsigned long long)total_positions,
    (unsigned long long)total_games,
    filename);

}
