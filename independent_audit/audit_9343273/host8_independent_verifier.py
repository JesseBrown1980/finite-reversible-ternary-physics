#!/usr/bin/env python3
"""Strict, read-only Host8 sampled-sector authenticator. No silent skips.
Reproduces commit 9343273 SHA-256 byte ranges independently from C++.
Read-only: Does not claim testing the entire 300 GB device or dynamics.
Use: python host8_independent_verifier.py --host8-dir 'D:/ASOLARIA-HOST8-ROOMS-300G'
Use: python host8_independent_verifier.py --self-test
"""
import argparse
import csv
import hashlib
import io
import json
from pathlib import Path
import tempfile

MANIFEST = {
    '000': {
      24: '2dfe7d757ecb012475806b86fcdefc602680d042db836ffbcb2bfe924f2a18ee',
      48: '9da76c8b3151fa372ff2ef378cb5018490f7460581035e3268b3afd4a85e6e0f',
    },
    '187': {
      24: '6752ab9e2603cc5042f7c7a41c3ddc48ae5571fd0aeb945d0a3f63b1a948a652',
      48: 'b3bfb382de3eb473cd43328abb50e31c4e5222d734e8d39d8e80b47e6f19ad07',
    },
    '374': {
      24: '963805be806e8a290aab8dee48b0d214d03015c84e02bf844f0de87e2b37c10a',
      48: '11969dcf23ae7cb32103d40e8c12b4f7a87eec2b614c02ea2e9675bc80e2bae4',
    },
}


def validate_volume(directory: Path, manifest=MANIFEST):
    failures = []
    rows = []
    for sector, hashes in manifest.items():
        file = directory / f'sector-{sector}.rooms'
        largest = max(hashes)
        required = largest ** 3 * 8
        try:
            with file.open('rb') as stream:
                data = stream.read(required)
        except (OSError, IOError) as exc:
            failures.append(f'MISSING/UNREADABLE sector {sector}: {exc}')
            for L in sorted(hashes):
                rows.append({'sector':sector,'L':L,'bytes':L**3*8,'status':'UNAVAILABLE','sha256':'','expected':hashes[L], 'mass':'','energy_2x':'','px':'','py':'','pz':''})
            continue
        if len(data) != required:
            failures.append(f'SHORT READ sector {sector}: {len(data)} vs {required} bytes')
        for L, expected in sorted(hashes.items()):
            size = L ** 3 * 8
            chunk = data[:size]
            digest = hashlib.sha256(chunk).hexdigest() if len(chunk)==size else ''
            passed = digest == expected
            if not passed:
                failures.append(f'HASH MISMATCH sector {sector}, L={L}: {digest or "SHORT_READ"}')
            mass, energy, px, py, pz = 0,0,0,0,0
            if len(chunk)==size:
                # Mirrors the C++ unsigned-64 bit remainder-by-three decoder exactly.
                for cell in range(L**3):
                    val = int.from_bytes(chunk[cell*8:(cell+1)*8], 'little')
                    for x in range(-1,2):
                        for y in range(-1,2):
                            for z in range(-1,2):
                                n = val % 3; val //= 3
                                mass += n; energy += n*(x*x+y*y+z*z)
                                px += n*x; py += n*y; pz += n*z
            rows.append({'sector':sector,'L':L,'bytes':size,'status':'AUTHENTICATED' if passed else 'FAILED',
                         'sha256':digest,'expected':expected,'mass':mass,'energy_2x':energy,
                         'px':px,'py':py,'pz':pz})
    return rows, failures


def self_test():
    with tempfile.TemporaryDirectory(prefix='asolaria-host8-synthetic-') as tmp:
        directory=Path(tmp)
        manifest={}
        for i,sector in enumerate(MANIFEST):
            data=bytes((j*(19+i*2)+7*i)%256 for j in range(48**3*8))
            (directory/f'sector-{sector}.rooms').write_bytes(data)
            manifest[sector]={L:hashlib.sha256(data[:L**3*8]).hexdigest() for L in (24,48)}
        rows,failures=validate_volume(directory,manifest)
        assert not failures and len(rows)==6 and all(r['status']=='AUTHENTICATED' for r in rows)
        f=directory/'sector-187.rooms'
        with f.open('r+b') as s:
            s.seek(103);old=s.read(1);s.seek(103);s.write(bytes([old[0]^0x01]))
        _,failures=validate_volume(directory,manifest)
        assert len(failures)==2, failures
        (directory/'sector-374.rooms').unlink()
        _,failures=validate_volume(directory,manifest)
        assert len(failures)==3,failures
        print('PASS synthetic fixture: six authentic samples accepted; one-byte tampering of overlapping prefixes rejected twice; absent sector rejected')


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--host8-dir',type=Path, default=Path('D:/ASOLARIA-HOST8-ROOMS-300G'))
    p.add_argument('--output',type=Path,default=Path('host8_independent_readonly_report.csv'))
    p.add_argument('--self-test',action='store_true')
    a=p.parse_args()
    if a.self_test:
        self_test();return 0
    rows,failures=validate_volume(a.host8_dir)
    for r in rows: print(f"sector={r['sector']} L={r['L']} status={r['status']} SHA256={r['sha256'] or 'UNAVAILABLE'}")
    # Export even on failure for forensic traceability. A failed run exits nonzero.
    with a.output.open('w',newline='') as f:
        writer=csv.DictWriter(f,fieldnames=list(rows[0]));writer.writeheader();writer.writerows(rows)
    print(f'Audit report written: {a.output}')
    print(f'Unique authenticated input scope: 3 files × {48**3*8} bytes = {3*48**3*8:,} bytes (~2.53 MiB); L24 is a prefix of L48')
    if failures:
        for item in failures: print('FAIL:',item)
        return 2
    print('PASS: All 6 sample-range checksums match published manifest. This is not a full 300-GB scan or a dynamical proof.')
    return 0


if __name__=='__main__':
    raise SystemExit(main())
