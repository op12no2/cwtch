## overview

Cwtch is a UCI chess engine written in C. 

## toolchain

- clang in WSL2 Bash environment

## build

- makefile does not need tweaking when new files are added to the repo; it's auto
- `make`                    - Creates ./cwtch (native, -O3 -march=x86-64-v3)
- `make win`                - Cross-compiles ./cwtch.exe for Windows (mingw + lld)
- `make debug`              - Build with -O1 -g for valgrind/gdb
- `make clean`              - Remove build artifacts
- `make rebuild`            - Clean and rebuild
- `make release`            - Build all Linux release binaries into ./releases/ (x86_64, x86_64_v3, x86_64_v4)
- `make release-win`        - Same as release but .exe for Windows

The makefile injects `-DBUILD="$(VERSION)"` — UCI id banner, single source of truth for version (edit `VERSION` at top of makefile; release binary filenames follow suit).

## source structure

src/
  main.c          - Entry point, UCI loop
  types.h         - Piece/square enums, inline helpers
  pos.h/c         - Position struct, FEN parsing, is_attacked()
  position.h/c    - position() (uci command)
  go.h/c          - go() (uci command)
  move.h/c        - Move encoding (32-bit), formatting
  nodes.h/c       - Node struct with accumulators, global search stack
  bitboard.h/c    - Attack tables, magic number generation
  movegen.h/c     - Move generation (quiets/captures)
  makemove.h/c    - Move execution; records deferred accumulator update
  search.h/c      - Alpha-beta search with PVS
  qsearch.h/c     - Quiescence search
  net.h/c         - NNUE evaluation and accumulator functions (INCBIN'd weights)
  incbin.h        - Third-party header for embedding binary blobs
  evaluate.h/c    - Thin eval wrapper
  perft.h/c       - PERFT testing (68 positions)
  uci.h/c         - UCI protocol
  input.h/c       - Async stdin reader (for `stop`/`quit` during search)
  pv.h/c          - Principal variation table
  report.h/c      - `info ...` UCI output during search
  hh.h/c          - Hash history for 2/3 rep and 50 move rule
  tt.h/c          - Transposition table
  builtins.h      - popcount, bsf wrappers
  zobrist.h/c     - Zobrist hashing (incremental updates in makemove)
  bench.h/c       - Benchmark positions for search testing
  iterate.h/c     - Move iteration and sorting
  timecontrol.h/c - Time management
  history.h/c     - Piece-to history
  debug.h/c       - Debug verification for incremental updates
  see.h/c         - see_ge()
  datagen.h/c     - Self-play data generation (viriformat output)
  bullet.rs       - Rust trainer config for the bullet NN trainer (not compiled into cwtch)

## testing

### perft (move generation correctness)
- `./cwtch pt` - Run all 68 PERFT tests (~190s)
- `./cwtch "pt 4"` - Run only tests with depth <= 4 (~0.1s, 25 tests)
- `./cwtch "pt 5"` - Run only tests with depth <= 5 (~5s, 31 tests)
- `./cwtch "p s" "f 5"` - Set startpos and run PERFT depth 5

### bench (search correctness)
- note this is a wsl2 machine and nps is quite variable sadly.
- `./cwtch bench` - Run 50 position benchmark, reports node count and nps
- Node count must match reference: use `./releases/cwtch bench` to get expected value
- If node count differs, something is broken (eval, search, or accumulator updates)
- The default depth is 10 which is quick, use `./cwtch bench <depth>` for deeper searches if deemed necessary; e.g. `./cwtch bench 12`.
- The reported nps is not that reliable since it's WSL2 with other processes running.

### et (eval correctness)
- `./cwtch et` - Run eval on test positions, prints individual evals and sum
- Use `./releases/cwtch et` to get reference eval sum
- Useful when tweaking evaluation or accumulator code

### reference builds
- `./releases/cwtch` - Last known good build for comparison
- Always compare bench node count and et eval sum against reference after changes

Command-line arguments are executed as UCI commands then Cwtch exits. Without arguments, runs interactive UCI loop.

## code style

- Clear straightforward code but with performance in mind
- 2 space indentation
- else, return, continue and break on next line

```
if (x) {

}
else {

}
```

## nets

- The compiled binary embeds the net via `INCBIN` in `net.c`. The path is hardwired in `types.h` as `NET_WEIGHTS_PATH`; to build against a different net, edit that line.
- Config: `NET_H1_SIZE`, `NET_QA`, `NET_QB`, `NET_SCALE` in `types.h`. Current hidden layer is 1024.

### net architecture

Perspective network with two accumulators (one per side). Weights are pre-flipped so both accumulators use the same indexing. No king input buckets — basic 768 inputs. Output activation is squared ReLU (`sqrelu`).

### key files
- `net.h/c` - Weight loading, eval, and incremental update functions
- `nodes.h` - Node struct contains `accs[2][NET_H1_SIZE]` (1024 int16 elements each) plus `net_deferred` (pending op) and `accs_dirty` flag
- `makemove.c` - Records the deferred net op for each move type
- `search.c` / `qsearch.c` - Call `lazy_update_accs()` just before needing the eval

### accumulator updates (lazy)

`make_move()` does **not** touch the accumulators directly. It records a `net_deferred` op (`NET_OP_MOVE`, `NET_OP_CAPTURE`, etc.) and sets `accs_dirty = 1` on the child node. `lazy_update_accs()` applies the deferred op only when the node is actually evaluated, copying the parent's accumulators and running the matching updater:
- `net_move()` - quiet moves and pawn double push
- `net_capture()` - regular captures
- `net_ep_capture()` - en passant
- `net_castle()` - castling (king + rook in one loop)
- `net_promo_push()` - pawn promotion without capture
- `net_promo_capture()` - pawn promotion with capture

Each updater fuses all weight deltas for both accumulators into a single loop for vectorization.

### search integration
- `position()` calls `net_slow_rebuild_accs()` to initialize accumulators from scratch
- Child nodes start with `accs_dirty = 1`; the lazy update copies the parent's accs then applies the deferred op
- `net_eval()` reads directly from `node->accs` (caller is responsible for having called `lazy_update_accs()` first)
