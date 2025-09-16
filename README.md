# Cwtch
A chess engine written in C. UCI protocol. Reads/writes stdin/stdout. Use a chess user interface to play against it.

WIP

Needs ```cwtch.c```, ```weights.h``` and ```makefile``` to make. Currently OK under Linux and MSYS32 UCRT32 on Windows.  See latest commit for correct bench available via ```./cwtch bench q```.

Single threaded, no ```stop``` command.

The code is folded using ```/*{{{  fold name*/``` and ```/*}}}*/```.

## Command Extensions

```
bench - display a cumulative node count for a colleciton of searches.

eval - display the current evaluation.

perft n - perform a PERFT search to depth n.

board - display the current board.

hash - display TT info.

draw - is the current board in a drawn state?

pt - perform a series of PERFT searches.

et - perform a series of eval tests.

quit
```


