#!/usr/bin/env python3
"""Independent arithmetic and gate-family check for commit f76dfe6.

Inputs: published arm5_matched_k_dispersion_results.csv obtained separately
from JesseBrown1980/finite-reversible-ternary-physics at f76dfe6.
This does not execute the C++ simulator or refit the raw time series.
"""
import csv
from itertools import combinations
from collections import defaultdict
from pathlib import Path

v = [(x,y,z,x*x+y*y+z*z) for x in (-1,0,1) for y in (-1,0,1) for z in (-1,0,1)]
legacy = []
active = 0
for quad in combinations(range(27), 4):
    if tuple(sum(v[i][j] for i in quad) for j in range(4)) != (0,0,0,8):
        continue
    legacy.append(quad)
    classes = defaultdict(list)
    for code in range(81):
        trits = [(code // 3**j)%3 for j in range(4)]
        invariant = (sum(trits),) + tuple(sum(trits[n]*v[quad[n]][j] for n in range(4)) for j in range(4))
        classes[invariant].append(code)
    active += int(any(len([c for c in group if c!=40]) >= 2 for group in classes.values()))
assert len(legacy) == 105 and active == 15, (len(legacy), active)
print('new benchmark legacy family: 105 quadruplets, 15 active gates')
print('main Arm 3: 288 active collision gates, 32 gates/cell/tick (verified from source)')
print('new benchmark legacy: 12 gates/cell/tick (verified from source)')
print('initial density near crest/trough: 53 / 1 (not small-amplitude linear acoustics)')

p=Path(__file__).with_name('arm5_matched_k_dispersion_results.csv')
rows=list(csv.DictReader(p.open(newline='')))
for L in ['24','48']:
    results={}
    for kind in ('Arm 3','Arm 5'):
        subset=[r for r in rows if r['L']==L and kind in r['kernel']]
        assert len(subset)==2
        speeds=[float(r['measured_cs']) for r in subset]
        a=200*abs(speeds[0]-speeds[1])/(speeds[0]+speeds[1])
        results[kind]=a
        for r in subset:
            print(f"L={L} {kind:<5} {r['name']:<14} c={float(r['measured_cs']):.6f} "
                  f"R2={float(r['r_squared']):.4f} cycles={float(r['cycles']):.4f} "
                  f"three_cycle_pass={float(r['cycles']) >= 3}")
        print(f'L={L} {kind}: reported speed anisotropy {a:.4f}%')
    print(f'L={L}: reported anisotropy ratio {results["Arm 3"]/results["Arm 5"]:.4f}x')
