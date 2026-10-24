# Asolaria finite ternary physics — independent verification of commit `295bd05`

**Audit date:** 2026-10-24  
**Primary:** https://github.com/JesseBrown1980/finite-reversible-ternary-physics  
**Archive:** https://github.com/JesseBrown1980/finite-reversible-ternary-system-generate-the-experimentally-measured-laws-of-physics-from  
**Both pinned heads:** `295bd05e57f0035702344dbbb0dc3c5579370f39`.

## Methods and scope

- Inspected the GitHub source of `asolaria_equilibrium_suite.cpp`, `equivariant_intershell_kernel.h`, `reproduce.py`, the revised thermal test, and the published CSV files at the pinned commit.
- Independently compiled and executed a C++17 implementation of the D3Q27 face/edge/inter-shell collision rules already independently reconstructed in the `da040c7` audit. Source is included here, self-contained, in `independent_arm5.cpp` and `independent_heat.cpp`.
- Used exact integer local occupancy/collision/streaming and floating-point only for the macroscopic observables; the independent compiler invocation was `g++ -O3 -std=c++17 -Wall -Wextra -Werror -static` (exit code 0).
- Executed 49 observations (48 transitions) on a 24x24x24 periodic lattice for Arm 5 and for a separate correct initial thermal-gradient control. Verified exact integer total particle number, doubled micro-energy and global momentum every tick.
- Compared output with the committed `arm5_equivariant_results.csv` and `corrected_thermal_timeseries.csv`. These are **independent reimplementations**, not claimed executions of the complete repository's original master C++ executable or physical Host8 disk.

## Arm 5 reproducibility

| Observable, tick 48 | Committed CSV | Independent C++ | Verdict |
|---|---:|---:|---|
| Total mass | 373248 | 373248 | Exact |
| Total microscopic energy density | 27 | 27 | Exact |
| Macroscopic kinetic energy density | 0.409804 | 0.409804289078 | Matches rounding |
| Enstrophy | 0.0149241 | 0.014924122222 | Matches rounding |
| Coherent TGV amplitude | 0.0744248 | 0.074424847737 | Matches rounding |
| Trit marginal Shannon entropy | 1.05208 | 1.05208285679 | Matches rounding |
| Peak local vorticity | 0.3692 | 0.369200130106 | Matches rounding |
| Density range | 17..38 | 17..38 | Exact |

Mass = 373248 occupancy units, doubled microscopic energy = 746496, net momentum = `(0,0,0)` at every observation. Independent CSV is `independent_arm5_results.csv`; stdout `execution.log`.

## Fair comparison with legacy Arm 3

Both Arm 3 and Arm 5 use the same exact orthogonal Taylor–Green initial condition. Numerical facts recomputed from **all 49 published records**:

| Metric | Legacy Arm 3 | Arm 5 equivariant |
|---|---:|---:|
| Coherent TGV amplitude, tick 0 | 0.197874 | 0.197874 |
| Coherent TGV amplitude, tick 24 | 0.0603961 | 0.0238340 |
| Coherent TGV amplitude, tick 48 | 0.0139984 | 0.0744248 |
| Mean coherent amplitude over 49 records | 0.0764573 | 0.0777688 |
| Mean enstrophy over 49 records | 0.0259813 | 0.0119526 |
| Mean density variance over 49 records | 17.3588 | 9.9452 |
| Kinetic energy, tick 48 | 0.689440 | 0.409804 |

The **5.3167x** improvement in coherent amplitude at tick 48 is genuine and reproducible. It is **not** uniformly superior: Arm 5 wins on 27 of 49 sampled ticks; at tick 24 its amplitude is less than half of Arm 3. This is consistent with recurrent/oscillatory behavior. The mean coherent amplitudes differ by only about 1.7% across the trajectory, so the one-tick ratio must not be generalized to 'always better preserved' without more trials and longer windows.

Lower enstrophy and variance do not by themselves establish an experimentally correct viscosity, correct Navier–Stokes constitutive law, or a physical rate of thermalization.

## Correct initial thermal gradient: reproduced

Initial conditions: each cell has density 27, zero momentum, microscopic doubled energy 56 on the left half and 52 on the right half. This correctly resolves the previous 'same-energy halves' objection.

| Tick | Hot-half energy | Cold-half energy | Hot–cold difference | Hot-half mean density | Cold-half mean density |
|---:|---:|---:|---:|---:|---:|
| 0 | 56 | 52 | +4 | 27 | 27 |
| 1 | 55 | 53 | +2 | 26.3333 | 27.6667 |
| 6 | 52.6667 | 55.3333 | −2.6667 | 25.6667 | 28.3333 |
| 12 | 48.6667 | 59.3333 | −10.6667 | 23.6667 | 30.3333 |
| 24 | 57.3333 | 50.6667 | +6.6667 | 28.3333 | 25.6667 |
| 48 | 60 | 48 | +12 | 31 | 23 |

The independent native run reproduced every listed checkpoint, and global mass, energy and momentum remained exactly conserved at all 49 ticks. The independent observations are `independent_corrected_heat.csv` and `thermal_execution.log`.

**Important physical distinction:** zero **global** momentum does not mean zero **local** momentum. Local x-momentum RMS develops from zero at tick 0 to 1.63299 at tick 1, 2 at tick 6, and remains nonzero at later times. Local density also ceases to be uniform immediately after streaming (individual cells at tick 1 range from density 23 to 31). The heat contrast is therefore dynamically coupled to compressible/density and momentum modes.

The repeated contrast sign reversals are inconsistent with a simple monotone Fourier relaxation in this periodic setup, but **do not uniquely confirm a Cattaneo–Maxwell or second-sound constitutive law**. A single time series in an exactly reversible finite lattice may contain wave propagation, advection, coherent recurrence, multimode interference and finite-size effects. To establish a specific constitutive PDE, fit full spatial temperature/energy and heat-flux fields over several wavevectors with independent coefficients and compare out-of-sample predictions; assess density/momentum coupling separately.

## Remaining claim and harness boundaries

1. **Old acoustic/dispersion data still belong to legacy operator.** The Git blob SHA IDs of `isotropy_results.csv`, `rotational_dispersion_results.csv`, and `matched_k_dispersion_results.csv` did not change in commit 295bd05. Therefore the stated 1.14% low-frequency anisotropy and 4.77% matched-|k| comparison **have not yet been established for the new Arm 5 collision dynamics**.
2. **Exact cubic `O_h` equivariance is not continuous `SO(3)` invariance.** Exact finite-group symmetry is substantial; separately measure the continuum direction/scale dependence for the *new* operator.
3. **Arm 5 acceptance criterion is weak relative to the headline.** `reproduce.py` checks Arm 5's energy invariance and finite-grid vorticity bound, not the 5.3x endpoint preservation advantage, seed robustness, or wavelength-specific transport laws. Add quantitative, pinned comparator checks with tolerances and repeated seeds. The checks for old matched-|k| anisotropy print a pass even when anisotropy is nonzero.
4. **Physical Host8 sectors were unavailable locally.** GitHub code and recorded hashes can be reviewed, but the original physical 300 GB substrate bytes were not independently authenticated in this audit.
5. **Finite-grid boundedness remains different from the continuous Clay Navier–Stokes problem**, which requires mesh-uniform control and an appropriate continuum limit or other rigorous connection to that PDE.

## Recommended next acceptance experiment

Execute **Arm 5 acoustic dispersion and thermal constitutive tests** using the actual new `step_universe_equivariant` update, with:

- equal-magnitude directions `(3,0,0)` and `(2,2,1)`, two or more wavelengths, and multiple independent initial seeds;
- signed density, velocity, temperature/energy and heat-flux Fourier modes in both directions;
- separate linear thermal and coupled thermal-acoustic fit hypotheses; comparison on held-out wavelengths;
- quantitative acceptance criteria fixed *before* fitting, plus exact integer conservation, reversal and 48-operation cubic-equivariance checks.

This provides a falsifiable bridge from a verified finite-state microphysics rule to emergent macroscopic transport physics without assuming the desired PDE.
