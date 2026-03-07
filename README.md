# Cwtch

Cwtch is a UCI chess engine written in C. As a command line program it needs a chess user interface to operate within; e.g. CuteChess, Arena etc.

## Command extensions

- quit/q - close Cwtch.
- bench/h [_d_] - get a node count and nps over a collection of searches with optional depth _d_, the default being 10 which is quick.
- eval/e - display an evaluation for the current position.
- board/b - display the board for the current position.
- perft/f _d_ - performs a PERFT search to depth _d_ on the current position and report nps.
- pt [_d_] - perform a set of PERFT searches. If _d_ is present depths greater than _d_ are skipped.
- et - perform a collection of test evaluations and display evaluation sum.
- net/n - display network attributes.

Commands can be given on the command line, for example: ```./cwtch ucinewgame "position startpos" b "go depth 10"```.

## Cwtch's net

Cwtch's net was booted from ```quiet_labeled.epd``` and ```lichess-big3-resolved.epd```, then iteratively improved through six generations of self play and training; initially using a diy trainer and more recently with ```bullet```. Currently it's a simple quantised 768->(384*2)->1 squared ReLU architecture, trained on about 600M positions.

## References & Acknowledgements

- https://www.chessprogramming.org/Main_Page - Chess programming wiki
- https://computerchess.org.uk/ccrl/4040 - CCRL rating list
- https://backscattering.de/chess/uci - UCI protocol
- https://talkchess.com - Talkchess forums
- https://github.com/jw1912/bullet - bullet network trainer
- https://discord.gg/pntchvGU - Engine Programming Discord

