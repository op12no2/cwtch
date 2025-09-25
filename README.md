# Cwtch
A C version of my Javascript chess engine [Lozza](https://github.com/op12no2/lozza) that uses the same (768->256)x2->1 network.

Like Lozza, the code is folded using ```/*{{{  fold name*/``` and ```/*}}}*/```.

Cwtch means hug/cuddle in Welsh and is pronounced cutch with a very short u like in put - ish.

## Command Extensions

```
bench - display a cumulative node count for a collection of searches.

eval - display the current evaluation.

perft <n> - perform a PERFT search to depth <n> from the current position.

board - display the current board.

hash - display TT info.

draw - is the current board in a drawn state?

pt - perform a series of PERFT searches.

et - perform a series of eval tests.
```

## Acknowledgements

https://github.com/jw1912/bullet - bullet network trainer

https://www.chessprogramming.org/Main_Page - Chess programming wiki

https://computerchess.org.uk/ccrl/4040 - CCRL rating list

https://backscattering.de/chess/uci - UCI protocol

https://discord.gg/uM8J3x46 - Engine Programming Discord

https://talkchess.com - Talkchess forums
