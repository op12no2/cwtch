# Cwtch

An experimental Javascript UCI chess engine with NNUE evaluation.

Internal data generation and DIY Javascript trainer.

Currently +45 over Lozza with this internal net:-

```
datagen softnodes 6000 hardnodes 12000 random 10ply first 16ply
h1 size 75                                                                                                                                         
lr 0.001                                                                                                                                           
batch size 500                                                                                                                                     
activation relu                                                                                                                                    
stretch 100                                                                                                                                        
interp 0.5                                                                                                                                         
num_batches 227547                                                                                                                                 
opt Adam                                                                                                                                           
l2reg false                                                                                                                                        
epochs 157                                                                                                                                         
loss 0.023536278157536648
```

See the ```skipP``` function in ```trainer.js``` for how the data is filtered during training:-

https://github.com/op12no2/cwtch/blob/main/trainer.js#L450

If you want to try cwtch, it can be fired up in Node.js like Lozza. For details see the readme in the Lozza 2.5 release.

https://github.com/op12no2/lozza/releases/tag/2.5
