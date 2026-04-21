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

## Building

You'll need the relevant net. Email me ```op12no2@gmail.com the``` the value of ```NET_WEIGHTS_PATH``` in ```src/types.h``` and I'll send it to you. Then just update that as needed line and ```make release``` or ```make release-win``` to create binaries in ```./releases```.

## References & acknowledgements

- https://www.chessprogramming.org/Main_Page - Chess programming wiki
- https://computerchess.org.uk/ccrl/4040 - CCRL rating list
- https://backscattering.de/chess/uci - UCI protocol
- https://talkchess.com - Talkchess forums
- https://github.com/jw1912/bullet - bullet network trainer
- https://discord.gg/pntchvGU - Engine Programming Discord
- https://analog-hors.github.io/site/magic-bitboards - Magic bitboards intro
- https://github.com/graphitemaster/incbin - incbin
- https://github.com/cosmobobak/viriformat - viriformat
- https://huggingface.co/datasets/op12no2/cwtch_zero - Cwtch training data
