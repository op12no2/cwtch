#!/usr/bin/env python3
"""
Fit cwtch_cp = A * sf_cp + B over all FENs in an epd, chunked,
printing a running fit every CHUNK fens so it can be killed early
once the numbers stabilise.

Usage:
  bin/normalise.py [chunk_size]
"""
import subprocess, sys, re, os, signal

EPD   = "/mnt/d/hcetraining/lichess-big3-resolved.epd"
SF    = "/mnt/d/engines/sf/sf"
CWTCH = "./releases/cwtch_5_x86_64_v3"

CHUNK = int(sys.argv[1]) if len(sys.argv) > 1 else 10000

def start(cmd):
    return subprocess.Popen(cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                            stderr=subprocess.STDOUT, text=True, bufsize=1)

def read_until(p, sentinel):
    buf = []
    while True:
        line = p.stdout.readline()
        if not line:
            break
        buf.append(line)
        if sentinel in line:
            break
    return "".join(buf)

SF_RX = re.compile(r"Final evaluation\s+([+-]?[0-9.]+)")
CW_RX = re.compile(r"eval:\s*(-?\d+)\s*cp")

SUB = 200  # sub-chunk so child stdout pipe doesn't fill and deadlock

def _eval_one(engine, fens, rx, scale):
    vals = []
    for i in range(0, len(fens), SUB):
        part = fens[i:i+SUB]
        script = []
        for f in part:
            script.append(f"position fen {f}")
            script.append("eval")
        script.append("isready")
        engine.stdin.write("\n".join(script) + "\n"); engine.stdin.flush()
        out = read_until(engine, "readyok")
        vals.extend(float(m.group(1)) * scale for m in rx.finditer(out))
    return vals

def eval_chunk(sf, cw, fens):
    sf_vals = _eval_one(sf, fens, SF_RX, 100.0)
    cw_vals = _eval_one(cw, fens, CW_RX, 1.0)
    return sf_vals, cw_vals

def fit(x, y):
    n = len(x)
    if n < 2: return None
    sx = sum(x); sy = sum(y)
    sxx = sum(a*a for a in x); sxy = sum(a*b for a,b in zip(x,y))
    denom = n*sxx - sx*sx
    if denom == 0: return None
    A = (n*sxy - sx*sy) / denom
    B = (sy - A*sx) / n
    mean_y = sy/n
    ss_tot = sum((b-mean_y)**2 for b in y) or 1.0
    ss_res = sum((b - (A*a+B))**2 for a,b in zip(x,y))
    return A, B, 1 - ss_res/ss_tot

def bucket_of(pc):
    if pc >= 26: return "opening  (>=26)"
    if pc >= 20: return "middle   (20-25)"
    if pc >= 14: return "late-mid (14-19)"
    if pc >= 8:  return "endgame  (8-13) "
    return          "deep-eg  (<8)   "

def piece_count(fen):
    return sum(1 for c in fen.split()[0] if c.isalpha() and c.lower() != 'k')

def report(all_sf, all_cw, all_fens, total_seen):
    pairs = [(s,c,f) for s,c,f in zip(all_sf,all_cw,all_fens)
             if abs(s) < 400 and abs(c) < 1500]
    xs = [p[0] for p in pairs]; ys = [p[1] for p in pairs]
    r = fit(xs, ys)
    if not r: return
    A, B, r2 = r
    print(f"\n=== after {total_seen} fens ({len(pairs)} kept) ===")
    print(f"  overall  A={A:.4f}  B={B:+.2f}  r^2={r2:.4f}")
    print(f"  => to normalise cwtch eval: multiply internal cp by {1.0/A:.4f} (i.e. divide by {A:.4f})")
    buckets = {}
    for s,c,f in pairs:
        b = bucket_of(piece_count(f))
        buckets.setdefault(b, ([], []))
        buckets[b][0].append(s); buckets[b][1].append(c)
    for label in sorted(buckets):
        bx, by = buckets[label]
        if len(bx) < 20:
            print(f"  {label}  n={len(bx):6d}  (too few)"); continue
        r2b = fit(bx, by)
        if r2b:
            Ab,Bb,rr = r2b
            print(f"  {label}  n={len(bx):6d}  A={Ab:.4f}  B={Bb:+7.2f}  r^2={rr:.4f}")
    sys.stdout.flush()

def main():
    sf = start([SF])
    sf.stdin.write("uci\n"); sf.stdin.flush()
    read_until(sf, "uciok")
    sf.stdin.write("ucinewgame\nisready\n"); sf.stdin.flush()
    read_until(sf, "readyok")
    cw = start([CWTCH])
    cw.stdin.write("isready\n"); cw.stdin.flush()
    read_until(cw, "readyok")

    all_sf = []; all_cw = []; all_fens = []
    total_seen = 0
    chunk_fens = []

    try:
        with open(EPD) as fp:
            for ln in fp:
                parts = ln.strip().split()
                if len(parts) < 4: continue
                fen = " ".join(parts[:4]) + " 0 1"
                chunk_fens.append(fen)
                if len(chunk_fens) >= CHUNK:
                    sv, cv = eval_chunk(sf, cw, chunk_fens)
                    n = min(len(sv), len(cv), len(chunk_fens))
                    # cwtch is side-to-move; flip for white-POV.
                    cv = [v if chunk_fens[i].split()[1]=='w' else -v for i,v in enumerate(cv[:n])]
                    sv = sv[:n]
                    fs = chunk_fens[:n]
                    all_sf.extend(sv); all_cw.extend(cv); all_fens.extend(fs)
                    total_seen += len(chunk_fens)
                    chunk_fens = []
                    report(all_sf, all_cw, all_fens, total_seen)
    except KeyboardInterrupt:
        print("\ninterrupted — final report:")
        report(all_sf, all_cw, all_fens, total_seen)
    finally:
        try: sf.stdin.write("quit\n"); sf.stdin.flush()
        except: pass
        try: cw.stdin.write("quit\n"); cw.stdin.flush()
        except: pass

if __name__ == "__main__":
    main()
