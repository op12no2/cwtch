# Cwtch
An experimental UCI Javascript chess engine using a small (768->75) relu NNUE. Cwtch has internal data generation and separate DIY Javascript trainer. No further work is planned. I just wanted to see if a (hand coded) Javascript NNUE could beat my traditional evaluation in Lozza and in tests it was about 50 Elo stronger.

The network was booted using WDL from quiet_labeled.epd and lichess-big3-resolved.epd (8M positions). This boot net was then improved with two rounds of datagen/training using a score-WDL ratio of 0.5 and around 114M positions. 

