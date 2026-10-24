#!/usr/bin/env python3
from pathlib import Path
from random import Random
from hashlib import sha256
import sys
path=Path(sys.argv[1]) if len(sys.argv)>1 else Path("synthetic_sector_000.rooms")
r=Random(9343273)
with path.open("wb") as f:
    for i in range(48**3):
        f.write(r.getrandbits(64).to_bytes(8,"little"))
print(path, path.stat().st_size,"bytes", sha256(path.read_bytes()).hexdigest())
