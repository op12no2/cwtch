# Cwtch

A UCI chess engine written in C. Cwtch is a command line program that needs a chess user interface to operate within; for example [cutechess](https://cutechess.com/).

The code is folded using ```/*{{{  fold name*/``` and ```/*}}}*/```.

Cwtch means hug/cuddle in Welsh and is pronounced cutch.

## Command extensions

```
bench - display a cumulative node count for a collection of searches.

eval - display the current evaluation.

perft <n> - perform a PERFT search to depth <n> from the current position.

board - display the current board.

hash - display TT info.

draw - is the current board in a drawn state?

pt - perform a series of PERFT searches.

et - perform a series of eval tests.

build - display the build number.

net - display the network size.
```

## UCI options

```
option name Hash type spin default 16 min 1 max 1024
```

## Acknowledgements

https://github.com/jw1912/bullet - bullet network trainer

https://www.chessprogramming.org/Main_Page - Chess programming wiki

https://computerchess.org.uk/ccrl/4040 - CCRL rating list

https://backscattering.de/chess/uci - UCI protocol

https://discord.gg/uM8J3x46 - Engine Programming Discord

https://talkchess.com - Talkchess forums
