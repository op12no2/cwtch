# Cwtch

Javascript UCI chess engine with NNUE eval.

In development but usable. 

See uciExec() for command rep.

Runs in [Node](https://nodejs.org/en) which is available for pretty much every platform.

## Example

```
c:> node cwtch
u                       # shortcut for ucinewgame
net                     # show net info and some metrics
bench                   # useful time and node count when testing
p s                     # shortcut for position startpos
board                   # display the board
g d 5                   # shortcut for go depth 5
e                       # show eval of current position 
et                      # eval tests
q                       # quit
```
`pt` does perft tests but takes a long time:-
```
c:> node cwtch pt q
```
## Chess User Interfaces

Can be run in chess UIs by using Node as the exectable and cwtch.js as an argument or using a batch file etc.


