# Asolaria finite ternary physics — Independent Host8 and matched-|k| audit

**Pinned commit:** [`9343273a3fc497feb7d5d9b3dd3eae84b8ed776b`](https://github.com/JesseBrown1980/finite-reversible-ternary-physics/commit/9343273a3fc497feb7d5d9b3dd3eae84b8ed776b)  
**Date:** 2026-10-24  
**Repositories:** `finite-reversible-ternary-physics` and `finite-reversible-ternary-system-generate-the-experimentally-measured-laws-of-physics-from` both reported exactly this `main` SHA in the connected GitHub API.

## Scope and access declaration

- **Actual 300 GB Host8 disk:** NOT MOUNTED / NOT ACCESSED from this Linux container. `/mnt/data`, `/media`, and `/run/media` do not contain the physical `D:/ASOLARIA-HOST8-ROOMS-300G` volume or its `sector-000.rooms`, `sector-187.rooms`, and `sector-374.rooms` files. No physical binary SHA-256 is claimed to have been recomputed here.
- **Published GitHub source and CSV:** Inspected using the connected GitHub API at the exact pinned commit.
- **Independent physical-format authenticator:** Implemented and executed locally: refuses missing sectors, short reads, and incorrect hashes. Synthetic input passed; intentional corruption and deletion failed as expected. Against the non-mounted physical path, exits **2**, with explicit missing-sector errors.
- **Independent native collision/streaming smoke tests:** GCC 14.2, strict C++17 (`-O3 -std=c++17 -Wall -Wextra -Werror -static`). On seeded **synthetic** 64-bit room bytes, both L24 and L48 passed global exact conservation and 8-forward/8-inverse trit-for-trit recovery. This test is **not** claimed to reproduce physical-sector outcomes.
- **Acoustic source and output audit:** Independently recomputed the summary comparisons and analyzed all 632 published Fourier amplitude measurements, including a discrete spectral analysis independent of the damped-fit optimizer. Did **not** compile or rerun the exact published 9343273 acoustic C++ executable in this turn.

## 1. Matched-wavevector acoustic results

The revised `test_equivariant_matched_k.cpp` genuinely constructs the 288-gate Arm 3 family by collecting collision-invariant quadruplets and runs **32 gates/cell/tick**. The earlier restricted 105-gate/12-gates benchmark was removed. The initial wave uses `rho in [26,28]`, with opposite face-channel changes and a rest-channel adjustment, and sets momentum to zero initially. Both directions have identical `|k|=3*(2*pi/L)`.

Published output `arm5_matched_k_dispersion_results.csv`:

| Grid | Kernel | Axis `cs` | Oblique `cs` | Computed anisotropy | Axis cycles | Oblique cycles | Axis R² | Oblique R² |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| 24 | Standard Arm 3 | 0.913418 | 0.907478 | 0.652% | 6.8506 | 6.8061 | 0.7591 | 0.7120 |
| 24 | Equivariant Arm 5 | 0.334052 | 0.606648 | 57.96% | **2.5054 (FAIL)** | 4.5499 | 0.9715 | 0.7343 |
| 48 | Standard Arm 3 | 0.899501 | 0.935439 | 3.917% | 5.3970 | 5.6126 | 0.9734 | 0.9394 |
| 48 | Equivariant Arm 5 | 0.274418 | 0.742673 | 92.08% | **1.6465 (FAIL)** | 4.4560 | 0.7342 | 0.6891 |

**Interpretation:** The experimental corrected baseline is materially fairer. Under these conditions, standard Arm 3 has substantially closer axis-oblique acoustic speeds than Arm 5. The Arm 5 axis speeds cannot yet be considered well identified acoustic modes, because they fail the three-cycle acceptance criterion; the Arm 5 L48 oblique fit also has R² below 0.70. Thus the 92.08% figure is the *reported fitted-speed difference*, not a precision-calibrated constitutive anisotropy.

The new source records `three_cycle_pass` for each fit, but the benchmark's `all_checks_passed` only rejects low R² (threshold 0.70 for Arm 3, 0.65 for Arm 5). It does not reject a failed three-cycle flag. `reproduce.py` enforces the Arm 3 L48 cycle flags but does not enforce Arm 5's. Consequently a global PASS can coexist with the two recorded failed Arm 5 fits.

## 2. Independent raw-signal spectral check

I parsed all 632 published samples in `arm5_matched_k_timeseries.csv` (4 kernel-direction combinations times 61 samples for L24, and 4 combinations times 97 samples for L48). For each time series, I subtracted its mean and computed discrete Fourier powers directly, without using the repo's `acoustic_damped_fit.h` fit. These DFT peaks are approximate integer-cycle diagnostics, *not* substitute precise phase-velocity fits.

| Grid | Kernel | Direction | Dominant DFT cycle bin | Max absolute amplitude / initial absolute amplitude |
|---|---|---|---:|---:|
| 24 | Arm 3 | Axis | 7 | 1.00 |
| 24 | Arm 3 | Oblique | 7 | 1.00 |
| 24 | Arm 5 | Axis | 3 | **7.00** |
| 24 | Arm 5 | Oblique | 5 | **3.53** |
| 48 | Arm 3 | Axis | 5 | 1.00 |
| 48 | Arm 3 | Oblique | 6 | 1.00 |
| 48 | Arm 5 | Axis | 2 | **5.48** |
| 48 | Arm 5 | Oblique | 5 | **2.38** |

Thus the low-frequency Arm 5 axis behavior is visible directly in the raw signed Fourier measurements and is not purely a numerical optimizer artifact. Its amplitude growth indicates substantial mode transfer/amplification within this observation period. One should not assume the underlying perturbation remains in a clean, single damped *linear* acoustic eigenmode without independent amplitude-scaling and projection tests.

## 3. Published Host8 provenance scope

The benchmark's `load_host8_rooms` reads `8*L³` bytes beginning at offset **0** in each chosen sector, SHA-256-hashes the raw byte block, and expands each little-endian 64-bit room value to **27 base-3 digits** via repeated `%3` and `/3`. For each sector, L24 is the **prefix** of the L48 block:

- L24 block: 13,824 rooms / **110,592 bytes**
- L48 block: 110,592 rooms / **884,736 bytes**
- Unique blocks across three sampled sectors: `3 * 884,736 = 2,654,208 bytes`, about 2.53 MiB, *not 300 GB*.

The published `host8_matched_k_results.csv` contains six rows labeled VERIFIED, with the expected six published SHA-256 strings, `arm5_dE=0`, and `arm5_loschmidt_mismatches=0`. These are **GitHub-published assertions** from the remote Host8 run. The true physical binary bytes cannot be independently authenticated in this container without the drive or those block bytes. SHA-256 checksums authenticate only bytes whose actual contents are available to hash.

**Important experiment separation:** The acoustic matched-|k| functions build their own clean wave state (`vector<u8> state(...,1)`) and never initialize from Host8 sector bytes. The Host8 portion independently uses the sampled room states to test **24-step micro-energy conservation** and **8+8 reversal**. It does *not* estimate the acoustic speeds on Host8-initialized states.

The C++ `host8_available` test prints `[SKIP]` when the drive is absent and **does not force a nonzero exit**. `reproduce.py` similarly enters standalone mode without Host8. Therefore `python reproduce.py` PASS does not by itself prove physical Host8 access. Use an explicit fail-closed required-host test for Host8 validation.

## 4. Independently executed Host8-format tests (synthetic only)

I generated one random `884,736`-byte synthetic room block with Python `Random(9343273)`. It is not the Host8 sector file. The independent C++17 collision/streaming implementation loads 64-bit little-endian words and decomposes each into 27 ternary occupancies, then runs the published full inter-shell collision algebra.

| Synthetic lattice | Reconstructed trits | Forward exact conservation | Forward 8 + inverse 8 |
|---|---:|---|---|
| L24 | 373,248 | 24 ticks PASS | 0 mismatches |
| L48 | 2,985,984 | 8 ticks PASS | 0 mismatches |

The separate Python SHA-256 verifier's `--self-test` did 6 hashes, corrupted one byte in the overlapping prefix (two hash failures), and removed a whole sector (missing-sector failure). It also correctly failed for the actual unmounted `D:/...` path.

## 5. Reproducibility instructions on the actual Host8 Windows machine

1. In the existing local working repo `D:\asolaria-ternary-fluid`, use the pinned commit `9343273a3fc497feb7d5d9b3dd3eae84b8ed776b`.
2. Place the provided `host8_independent_verifier.py` in a local writable folder, and run:

   `python host8_independent_verifier.py --host8-dir "D:\ASOLARIA-HOST8-ROOMS-300G" --output host8_independent_readonly_report.csv`

   This never modifies room files; it validates all six SHA-256 checks against **actual** sector bytes and records decoded exact mass, doubled microscopic energy, and momentum. It returns **nonzero** if any sector is absent, short, or corrupt.
3. Run the repository's `test_equivariant_matched_k.cpp` binary on that same host for its original forward/reverse physics test. Inspect both stdout and `host8_matched_k_results.csv` and retain the actual 6 hashes. **Fail if physical loading was skipped.** Prefer requiring `--host8-required` in the repository itself rather than an optional soft skip.
4. For an independent container audit, export only each sector's first **884,736 bytes** (the L24 hash can be checked from the prefix) rather than copying the full 300 GB drive. The three blocks total ~2.53 MiB. Upload them securely; verify their contents and provenance before reusing them as initial states. Do not publish sensitive raw storage contents without checking their origin and permissions.

## 6. Next physics acceptance criteria

- The *same* clean initial conditions and the **true 288-gate, 32-operations** legacy reference for all runs.
- Treat `three_cycle_pass==0` as an invalid fitted phase speed; enforce the threshold for **both** Arm 3 and Arm 5, not just Arm 3.
- Require frequency away from fit bounds and stable cross-window estimates; publish uncertainties, bootstrap across seed/phase/amplitude realizations, and an amplitude-response sweep to establish a linear regime.
- Record acoustic mode projection **and** momentum/energy/heat flux, to distinguish slowly evolving ballistic, diffusive, and acoustic modes rather than fitting each to one sinusoid.
- For a truly Host8-conditioned physics benchmark, seed the *same* clean perturbation onto **authenticated** Host8 microstates, and compare to unperturbed controls for each block/offset, with preserved sample hashes and replay receipts.

## Verdict

**Confirmed**: Commit synchronized; exact 288-gate source baseline; published CSV computations; spectral-mode diagnostics; independently executed native C++ exact conservation/reversal on synthetic room-formatted inputs; fail-closed verifier self-test.  
**Unavailable here**: Direct I/O or cryptographic authentication of the physical Host8 300 GB files, and a direct native rerun of the exact 9343273 acoustic benchmark executable.  
**Not established**: Continuous rotational isotropy, a uniquely determined physical acoustic constitutive law for Arm 5, or 300 GB-wide data provenance.
