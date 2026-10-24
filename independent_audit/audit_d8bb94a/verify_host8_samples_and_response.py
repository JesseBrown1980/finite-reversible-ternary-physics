#!/usr/bin/env python3
"""Independent, read-only checks of d8bb94a Host8 slices and response fits.

Run from a checkout or with --repo /path/to/finite-reversible-ternary-physics.
Requires Python standard library only. Rejects missing or changed binary blocks.
"""
import argparse
import csv
import hashlib
import math
import struct
from pathlib import Path

EXPECTED = {
    '000': ('2dfe7d757ecb012475806b86fcdefc602680d042db836ffbcb2bfe924f2a18ee', '9da76c8b3151fa372ff2ef378cb5018490f7460581035e3268b3afd4a85e6e0f',
            (372215, 744952, (176,309,-25)), (2984104,5970036,(-764,81,2949))),
    '187': ('6752ab9e2603cc5042f7c7a41c3ddc48ae5571fd0aeb945d0a3f63b1a948a652', 'b3bfb382de3eb473cd43328abb50e31c4e5222d734e8d39d8e80b47e6f19ad07',
            (373159,745464,(-854,79,597)), (2984328,5967494,(-250,-1524,1058))),
    '374': ('963805be806e8a290aab8dee48b0d214d03015c84e02bf844f0de87e2b37c10a', '11969dcf23ae7cb32103d40e8c12b4f7a87eec2b614c02ea2e9675bc80e2bae4',
            (373024,745705,(-449,-107,-263)), (2985241,5969407,(-1847,-1147,805))),
}
VELOCITIES = [(x,y,z,x*x+y*y+z*z) for x in (-1,0,1) for y in (-1,0,1) for z in (-1,0,1)]
assert VELOCITIES[13] == (0,0,0,0)


def decode_and_measure(raw, L, need_rest=False):
    N = L**3
    occupancy = [0,0,0]
    mass = e2 = px = py = pz = 0
    rests = bytearray(N) if need_rest else None
    for cell in range(N):
        (v,) = struct.unpack_from('<Q', raw, cell * 8)
        for ki, (x,y,z,e) in enumerate(VELOCITIES):
            v,t = divmod(v,3)
            mass += t
            e2 += t*e
            px += t*x; py += t*y; pz += t*z
            occupancy[t] += 1
            if ki == 13 and need_rest: rests[cell] = t
    ntrits = N*27
    entropy = -sum((v/ntrits)*math.log(v/ntrits) for v in occupancy if v)
    return (mass,e2,(px,py,pz),entropy,rests)


def perturbation_t0(rests, kx,ky,kz, L=24):
    amp = 0.0
    dmass=modified=0
    for x in range(L):
        for y in range(L):
            for z in range(L):
                idx=(x*L+y)*L+z
                phase=2*math.pi*(kx*x+ky*y+kz*z)/L
                c=math.cos(phase)
                v=rests[idx]
                delta = 1 if c>0.3 and v<2 else (-1 if c < -0.3 and v>0 else 0)
                dmass+=delta
                modified+=(delta!=0)
                amp+=delta*c
    return amp/(L**3), modified, dmass


def audit(root):
    failures=0
    for sec,(sha24,sha48,metrics24,metrics48) in EXPECTED.items():
        path=root/'data'/'host8_samples'/f'sector-{sec}.rooms'
        if not path.is_file():
            print(f'FAIL: missing {path}')
            failures+=1
            continue
        raw=path.read_bytes()
        if len(raw)!=884736:
            print(f'FAIL: {path} length={len(raw)} expected=884736')
            failures+=1
            continue
        for L,expect_hash,expect_metrics in [(24,sha24,metrics24),(48,sha48,metrics48)]:
            data=raw[:8*L**3]
            digest=hashlib.sha256(data).hexdigest()
            metrics=decode_and_measure(data,L,need_rest=L==24)
            good=(digest==expect_hash and metrics[:3]==expect_metrics)
            failures+=(not good)
            print(f'{"PASS" if good else "FAIL"} sector={sec} L={L} SHA256={digest} ' +
                  f'mass={metrics[0]} energy_2x={metrics[1]} momentum={metrics[2]} entropy={metrics[3]:.9f}')
            if L==24:
                for name,k in [('axis',(3,0,0)),('oblique',(2,2,1))]:
                    amp,edited,dmass=perturbation_t0(metrics[4],*k)
                    print(f'      init {name}: delta_amp_t0={amp:.9f}, edited_rest_trits={edited}, net_mass_change={dmass}')

    file=root/'host8_linear_response_results.csv'
    if not file.exists():
        print('FAIL: missing Host8 response summary CSV')
        failures+=1
    else:
        with file.open(newline='') as f:
            records=list(csv.DictReader(f))
        qualified=0
        print('\nACOUSTIC FIT AUDIT (>=3 fitted cycles AND R^2>=0.70; boundary flagged):')
        for row in records:
            cyc=float(row['cycles']); r2=float(row['r_squared']); alpha=float(row['damping_alpha']); omega=float(row['fitted_omega'])
            valid=cyc>=3 and r2>=0.70
            qualified+=valid
            boundary=(abs(omega-math.pi/(2*48))<1e-5 or abs(omega-.99*math.pi)<1e-5 or abs(alpha-20/48)<1e-5)
            print(f' {"QUALIFIED" if valid and not boundary else "NOT QUALIFIED"} {row["kernel"]:18} sector {row["sector"]} '
                  f'{row["name"]} cycles={cyc:.3f} R2={r2:.3f} boundary={int(boundary)}')
        print(f'Without boundary screening: {qualified}/{len(records)} fits meet both numeric thresholds')
        if len(records)!=12:
            failures+=1
            print('FAIL: expected 12 published acoustic fits')
    print(f'\n{"PASS" if failures==0 else "FAIL"}: cryptographic/data integrity checks; acoustic fits are diagnostic, not proof of a wave law')
    return failures


if __name__ == '__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('--repo',type=Path,default=Path('.'))
    args=parser.parse_args()
    raise SystemExit(1 if audit(args.repo.resolve()) else 0)
