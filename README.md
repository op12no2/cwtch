# Cwtch
An experimental UCI Javascript chess engine with a simple NNUE eval, internal data generation and DIY Javascript trainer.
## To testers
Feel free to test Cwtch, but beware it's early days and there may well be gremlins. Please download the latest release rather than use the repo version, which is knobbled a bit for development purposes. Note that the PV is currently just the best move and there are no UCI hash updates. 
## Goal
Determine if a (hand-coded) Javascript NNUE eval is viable by testing against my HCE engine [Lozza](https://github.com/op12no2/lozza).
## Progress
#### Latest net
```
[c:\projects\cwtch]node cwtch n q
build 1                                                                                                                                               
datagen softnodes 6000 hardnodes 120000 randomply 10 firstply 16                                                                                      
origin quiet-labeled.epd && lichess-big3-resolved.epd wdl boot and 2 rounds of RL                                                                     
i size 768                                                                                                                                            
h1 size 75                                                                                                                                            
h1 accumulators 1                                                                                                                                     
o phase buckets 0                                                                                                                                     
lr 0.001                                                                                                                                              
activation relu                                                                                                                                       
stretch 100                                                                                                                                           
interp 0.5                                                                                                                                            
batch size 500                                                                                                                                        
num_batches 227547                                                                                                                                    
positions 113773500                                                                                                                                   
opt adam                                                                                                                                              
l2reg false                                                                                                                                           
epochs 157                                                                                                                                            
loss 0.023536278157536648                                                                                                                             
min h1 weight 0.000005815227268612944                                                                                                                 
max h1 weight 98.90455627441406                                                                                                                       
min o weight 0.8036063313484192                                                                                                                       
max o weight 328.6114196777344 
```
#### Latest result
```
tc=60+1
Score of cwtch vs lozza: 411 - 252 - 220  [0.590] 883
...      cwtch playing White: 203 - 134 - 104  [0.578] 441
...      cwtch playing Black: 208 - 118 - 116  [0.602] 442
...      White vs Black: 321 - 342 - 220  [0.488] 883
Elo difference: 63.3 +/- 20.1, LOS: 100.0 %, DrawRatio: 24.9 %
SPRT: llr 2.96 (100.6%), lbound -2.94, ubound 2.94 - H1 was accepted
```
#### Known bugs
Killers need aligning with colour - either use 2 per node indexed by colourIndex() or a different root node for black.
## Plan
Engine: Proper PV, external nets (currently inlined), use bitboards, add SEE, more search heuristics, better move ordering and selection.

Network: Try perspective, quantisation, adam2, srelu, screlu, etc. At some point reboot from a random init or Lozza HCE. 
## Acknowledgements
I used [Ethereal](https://github.com/AndyGrant/Ethereal) to shape Cwtch's search and stole some of the pruning constants, which I'm gradually changing to suit Cwtch's slower speed and different eval.

The [Engine Programming](https://discord.com/invite/F6W6mMsTGN) discord server for answering my newbie questions.

The [Chess Programming Wiki](https://www.chessprogramming.org).

Stockfish's [NNUE](https://github.com/official-stockfish/nnue-pytorch/blob/master/docs/nnue.md) Overview.
