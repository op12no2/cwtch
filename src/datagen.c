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

#define DG_RANDOM_PLIES   16
#define DG_SEARCH_NODES   5000
#define DG_DRAW_SCORE     10
#define DG_DRAW_COUNT     10
#define DG_DRAW_PLY       40
#define DG_WIN_SCORE      1000
#define DG_WIN_COUNT      4
#define DG_MAX_GAME_MOVES 512
#define DG_REPORT_SECS    10

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

// --- FEN generation ---

static void pos_to_fen(const Position *pos, char *buf) {

  static const char piece_chars[] = "PNBRQKpnbrqk";
  char *p = buf;

  for (int rank = 7; rank >= 0; rank--) {
    int empty = 0;
    for (int file = 0; file < 8; file++) {
      int sq = rank * 8 + file;
      uint8_t piece = pos->board[sq];
      if (piece == EMPTY) {
        empty++;
      }
      else {
        if (empty) { *p++ = '0' + empty; empty = 0; }
        *p++ = piece_chars[piece];
      }
    }
    if (empty) *p++ = '0' + empty;
    if (rank > 0) *p++ = '/';
  }

  *p++ = ' ';
  *p++ = pos->stm == WHITE ? 'w' : 'b';

  *p++ = ' ';
  if (pos->rights == 0) {
    *p++ = '-';
  }
  else {
    if (pos->rights & WHITE_RIGHTS_KING)  *p++ = 'K';
    if (pos->rights & WHITE_RIGHTS_QUEEN) *p++ = 'Q';
    if (pos->rights & BLACK_RIGHTS_KING)  *p++ = 'k';
    if (pos->rights & BLACK_RIGHTS_QUEEN) *p++ = 'q';
  }

  *p++ = ' ';
  if (pos->ep == 0) {
    *p++ = '-';
  }
  else {
    *p++ = 'a' + (pos->ep & 7);
    *p++ = '1' + (pos->ep >> 3);
  }

  sprintf(p, " %d 1", pos->hmc);

}

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

// --- game entry ---

typedef struct {
  char fen[128];
  int score;
} GameEntry;

// --- play one game, return number of fens written ---

static int play_game(FILE *fp) {

  GameEntry entries[DG_MAX_GAME_MOVES];
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

  // scored play
  int draw_count = 0;
  int win_count = 0;
  const char *result = "0.5";

  for (int ply = 0; ply < DG_MAX_GAME_MOVES; ply++) {

    Position *pos = &nodes[0].pos;
    const int stm = pos->stm;
    const int opp = stm ^ 1;
    const int king_idx = piece_index(KING, stm);
    const int in_check = is_attacked(pos, bsf(pos->all[king_idx]), opp);

    int n = gen_legal_moves(legal);

    if (n == 0) {
      if (in_check)
        result = (stm == WHITE) ? "0.0" : "1.0";
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

    // record position (skip if in check, mate score, or capture)
    int is_capture = best & MOVE_FLAG_CAPTURE;
    if (!in_check && !is_capture && abs(score) < MATEISH && num_entries < DG_MAX_GAME_MOVES) {
      pos_to_fen(pos, entries[num_entries].fen);
      entries[num_entries].score = white_score;
      num_entries++;
    }

    // adjudication
    if (abs(score) <= DG_DRAW_SCORE)
      draw_count++;
    else
      draw_count = 0;

    if (draw_count >= DG_DRAW_COUNT && ply >= DG_DRAW_PLY) {
      result = "0.5";
      break;
    }

    if (abs(score) >= DG_WIN_SCORE)
      win_count++;
    else
      win_count = 0;

    if (win_count >= DG_WIN_COUNT) {
      result = (white_score > 0) ? "1.0" : "0.0";
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
      result = "0.5";
      break;
    }

  }

  // write all entries with the game result
  for (int i = 0; i < num_entries; i++) {
    fprintf(fp, "%s | %d | %s\n", entries[i].fen, entries[i].score, result);
  }

  return num_entries;

}

// --- main datagen loop ---

void datagen(const char *directory, int hours) {

  dg_seed_rng();

  char filename[512];
  snprintf(filename, sizeof(filename), "%s/data%llu.fen",
    directory, (unsigned long long)dg_rand());

  FILE *fp = fopen(filename, "w");
  if (!fp) {
    printf("error: cannot open %s\n", filename);
    return;
  }

  static char iobuf[1 << 20];
  setvbuf(fp, iobuf, _IOFBF, sizeof(iobuf));

  printf("datagen: writing to %s for %d hours\n", filename, hours);

  uint64_t start_time = time_ms();
  uint64_t end_time = start_time + (uint64_t)hours * 3600ULL * 1000ULL;
  uint64_t total_fens = 0;
  uint64_t total_games = 0;
  uint64_t last_report = start_time;

  while (time_ms() < end_time) {

    int fens = play_game(fp);
    total_fens += fens;
    total_games++;

    uint64_t now = time_ms();
    if (now - last_report >= DG_REPORT_SECS * 1000) {
      uint64_t elapsed = now - start_time;
      uint64_t fps = elapsed ? (total_fens * 1000ULL / elapsed) : 0;
      printf("datagen: %llu fens %llu games %llu fens/s\n",
        (unsigned long long)total_fens,
        (unsigned long long)total_games,
        (unsigned long long)fps);
      fflush(stdout);
      fflush(fp);
      last_report = now;
    }

  }

  fclose(fp);

  printf("datagen: done. %llu fens %llu games written to %s\n",
    (unsigned long long)total_fens,
    (unsigned long long)total_games,
    filename);

}
