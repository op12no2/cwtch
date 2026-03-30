# Cwtch

Cwtch is a UCI chess engine written in C. As a command line program it needs a chess user interface to operate within; e.g. CuteChess, Arena etc.

## Command extensions

- quit | q - close Cwtch.
- bench | h [_d_] - get a node count and nps over a collection of searches with optional depth _d_, the default being 10 which is quick.
- eval | e - display an evaluation for the current position.
- board | b - display the board for the current position.
- perft | f _d_ - performs a PERFT search to depth _d_ on the current position and report nps.
- pt [_d_] - perform a set of PERFT searches. If _d_ is present depths greater than _d_ are skipped.
- et - perform a collection of test evaluations and display an evaluation sum.
- net | n - display network attributes.
- loadnet | ln [_path_] - load an alternative net specified by _path_.
- datagen | dg _path_ _hours_ - write self-play games to _path_ in viriformat for _hours_ hours. see also ```bin/datagen```. Configure using the constants in ```src/datagen.c```.

Commands can be given on the command line, for example: ```./cwtch ucinewgame "position startpos" b "go depth 10"```.

## UCI options

- option name Hash type spin default 256 min 1 max 1024
- option name LoadNet type string default

## Cwtch's net

Cwtch's net was booted from a 'zero' random init and iteratively improved over 8 generations of self-play and training using bullet. It's a straight-forward quantised 768->(512*2)->1 squared ReLU architecture; see ```src/bullet.rs```.

## References & acknowledgements

- https://www.chessprogramming.org/Main_Page - Chess programming wiki
- https://computerchess.org.uk/ccrl/4040 - CCRL rating list
- https://backscattering.de/chess/uci - UCI protocol
- https://talkchess.com - Talkchess forums
- https://github.com/jw1912/bullet - bullet network trainer
- https://discord.gg/pntchvGU - Engine Programming Discord
- https://analog-hors.github.io/site/magic-bitboards - magic bitboards intro
- https://github.com/graphitemaster/incbin - incbin
- https://github.com/cosmobobak/viriformat - viriformat
