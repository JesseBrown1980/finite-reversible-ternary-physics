# Independent peer review — Asolaria matched-|k| Arm 5 benchmark

**Audited commit:** `f76dfe63da948257cc3e492b049f11ec0b0b39eb`, `JesseBrown1980/finite-reversible-ternary-physics` (both repositories are synchronized).

**Verification scope:** Source-level inspection through GitHub connector; independently executed Python 3 combinatorial gate count and arithmetic checks against the published 8-row `arm5_matched_k_dispersion_results.csv`. This audit did **not** execute the original 60-tick C++ code or refit unpublished signed time-series data. No Host8 physical-disk access.

## Positive findings

1. The code genuinely advances the Arm 5 equivariant inter-shell collision kernel for two equally long wavevectors, `(3,0,0)` and `(2,2,1)` with squared wavenumber `9*(2*pi/L)^2`.
2. Reported anisotropies are correctly calculated from the *published fitted speeds* using `200*abs(c1-c2)/(c1+c2)`. `L=24`: 101.8478% (legacy labeled) vs 29.8034% (Arm 5), arithmetic reduction 3.4173x. `L=48`: 163.1094% vs 32.2511%, reduction 5.0575x.
3. The published `reproduce.py` explicitly checks compilation and exit status of the new benchmark and compares the anisotropies.

## Critical comparability flaw: not the original Arm 3

The new `test_equivariant_matched_k.cpp` does **not** use the original Arm 3 collision family:

- Main `asolaria_equilibrium_suite.cpp`: all **288** four-channel collision gates, with default **32** gates per cell per tick (`make_collisions` and `step_universe`).
- New benchmark's `make_legacy_collisions`: only quadruplets with `sum velocity=(0,0,0)` and `sum |velocity|^2 = 8`. Independent enumeration obtains **105** candidate quadruplets, of which just **15** have an active local transposition. It executes only **12** gates per cell per tick.

Therefore its fitted `Arm 3 (Legacy)` numbers are for a different, much more restrictive model. They do not provide an apples-to-apples measured improvement over the main Arm 3.

This also explains why the same repository's prior `matched_k_dispersion_results.csv` has L=48 legacy speeds 0.895982 / 0.939775 (4.77% anisotropy), not the new benchmark's 1.00719 / 0.102327 (163.11% anisotropy). These are different legacy baselines.

## Nonlinear initial condition and weak wave identification

The new probe initializes all 26 moving channels of a cell to either `2` at cosine crest or `0` at trough, keeping the rest occupancy at `1`. Local density therefore ranges from **1** to **53**, compared to background 27. This is a nearly **96%** modulation of background density, inappropriate as an unqualified small-signal linear acoustic dispersion test.

The shared acoustic fitter `acoustic_damped_fit.h` explicitly says caller must separately check **at least three observed fitted cycles**, frequency-boundary solutions and cross-window stability. The current benchmark accepts `R² >= 0.70` for Arm 5, no three-cycle gate, and does not save raw time-series data needed for independent refitting.

| L | Label | Direction | R² | Cycles | Three-cycle criterion |
| ---: | --- | --- | ---: | ---: | --- |
| 24 | New legacy | Oblique | 0.5864 | 2.4530 | FAIL |
| 24 | Arm 5 | Oblique | 0.7038 | 5.5721 | PASS |
| 48 | New legacy | Oblique | 0.7184 | 0.3837 | FAIL |
| 48 | Arm 5 | Oblique | 0.8865 | 2.7201 | FAIL |

The L=48 baseline frequency is poorly identified (well under one fitted cycle), making the advertised 5.06x factor fragile.

## Symmetry interpretation

The already independently validated identity `F(Rs)=R(Fs)` for every cubic `R` in `O_h` remains a valid exact discrete symmetry result. However, no cubic symmetry sends an axis vector `(3,0,0)` to the oblique vector `(2,2,1)`. Exact cubic equivariance does **not** mathematically imply equal fitted speeds in these two directions. Large-scale rotational isotropy still needs a measured dispersion/convergence limit.

Arm 5's reported speed anisotropy actually rises from **29.80% at L=24** to **32.25% at L=48**; these two points do not demonstrate convergence to isotropy.

## Recommended acceptance criteria

1. Use the **exact original 288-gate, 32-collisions/cell/tick** Arm 3 implementation for the matched-k comparison, with identical initial states and observer.
2. Construct a weak perturbation, e.g. coarse-grained `|delta rho|/rho_0 <= 5%`, using ternary-compatible sparse cell perturbations / ensemble averages. Check multiple amplitudes.
3. Export the signed Fourier time series for each arm, L, direction, and seed. Run independent frequency fits; reject fewer than three cycles, boundary frequencies, unstable cross-window solutions, and weak R². Use additional time samples when necessary.
4. Run at L=24,48,96 and possibly 192, plus cubic-axis/face/oblique directions with equal wavevector magnitude where possible, multiple seeds. Compute uncertainty intervals for speed and damping.
5. Predefine the comparison and acceptance thresholds before analyzing results. Verify claims of `O(k²)` scaling numerically rather than inferring them from a single low-L point.

## Reproduce this review

```bash
python verify_matched_k.py
```

Requires only Python 3 standard library and the CSV snapshot. `check_output.txt` records the locally executed results.
