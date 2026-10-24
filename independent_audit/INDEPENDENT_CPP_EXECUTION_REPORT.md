# Independent Native C++ Execution Audit — Asolaria Physics

**Audit date:** 2026-10-24  
**GitHub source examined:** `JesseBrown1980/finite-reversible-ternary-system-generate-the-experimentally-measured-laws-of-physics-from`, `main` SHA `af717bacbea85cf0619dee58e13134a8227e153d`.  
**Local compiler:** GCC 14.2.0, Debian Linux, `-O3 -std=c++17 -Wall -Wextra -Werror -pedantic`.

## Explicit scope

**Actually compiled and executed:** Two **independent C++17 programs**, written against the repository's documented microscopic rules (D3Q27, 288 conserving involutive collision gates, distinct parity-filtered rule, code-40 fixed point, 32 local gates per cell per tick, exact integer streaming), plus an independent SciPy fitted-observer script. The programs ran in the assistant's local Linux container and exited with status 0. They do not contain the repository's original C++ source and should not be described as a direct execution of `asolaria_equilibrium_suite.cpp`.

**Not executed:** The original GitHub `reproduce.py`, the original `asolaria_equilibrium_suite.cpp`, or its Host8 I/O. Container direct network DNS access to GitHub is disabled, while connector access was read-only for content; the physical `D:/ASOLARIA-HOST8-ROOMS-300G` volume is not mounted here.

## Local test 1: exact ternary collision and reversibility (PASS)

- Exactly **288** active gates reconstructed for standard rule and **288** for alternative.
- Exactly **288 of 288** gates show distinct lookup mappings under the parity-filtered alternative.
- Exhaustive **46,656 local-state transition checks** (288 × 81 × two variants) pass exact mass, 3 momentum-component and doubled-energy conservation, involution, and the all-ones fixed-point test.
- All-ones initial microstate remains exactly unchanged after 8 full simulation ticks.
- A **16-forward, 16-backward** trajectory recovers every one of **110,592** occupancy trits of a 16³ Taylor–Green initial state.
- For the 24³ Taylor–Green simulation (49 observed ticks), exact integer global mass, momentum, and energy checks remain satisfied every tick.

## Local test 2: 49-step Taylor–Green (PASS)

- Initial density at each cell: 27.
- Initial energy density: 27.
- Initial macroscopic kinetic energy: `0.142661179698`.
- Initial enstrophy: `0.001339988823`.
- Final macroscopic kinetic energy: `0.6894396138`.
- Final enstrophy: `0.02647250444`.
- Final maximum local vorticity: `0.63810675218`.
- Checkpoints `0,1,2,3,6,24,48` agree with the currently committed `arm3_taylor_green_results.csv` to the GitHub CSV's displayed six-decimal precision.

## Local test 3: thermal transport (PASS for reproduction, not for Fourier interpretation)

The independently simulated hot/cold slab reproduces GitHub's contrast values:

| Tick | Hot minus cold |
|---:|---:|
| 0 | +8.000000 |
| 7 | -0.339120 |
| 12 | -6.469907 |
| 20 | -0.293981 |
| 27 | +5.590278 |
| 36 | -2.671296 |

The sign-reversing, oscillatory behavior is real for the implemented microdynamics. This does **not** by itself establish Fourier diffusion, second sound, or the Cattaneo–Maxwell constitutive equation. Those require further quantitative fitting and discrimination between transport models.

## Local test 4: dynamical directional acoustic waves (PASS for reproduction)

The independent C++ simulation evolved four real density perturbations for 60 ticks and exported 244 signed Fourier-mode observations. Independent SciPy variable-projection fitting of a damped frequency-and-phase model produced:

| Direction | Measured phase speed | Damping parameter | R² |
|---|---:|---:|---:|
| X | 0.914241 | 0.016947 | 0.905862 |
| Y | 0.948241 | 0.021444 | 0.884784 |
| Z | 0.843471 | 0.023557 | 0.910391 |
| (1,1,1) diagonal | 0.848767 | 0.053863 | 0.966779 |

- Mean speed: **0.888680** lattice units/tick.
- Directional anisotropy `(max(speed)-min(speed))/mean(speed)`: **11.7894%**.
- All four independently refitted speeds agree with the committed `isotropy_results.csv` at its printed six-decimal precision.

The acoustic measurements are **not** hard-coded in the current repository and are reproducible with a separate implementation. The data do **not** support a claim of *exact* or *100%* directional isotropy: measured anisotropy is ~11.8%. The program's `anisotropy < 15%` tolerance is passed.

## Fixed-grid boundedness and continuum claim

The fixed-grid occupancy bound remains sound. The committed grid refinement CSV indicates physical vorticity peaks roughly `11.468`, `18.5849`, and `23.6538` at N=16, 24, and 32, respectively. These values increase with resolution; mesh-independent continuum regularity has **not** been established. Finite state capacity at fixed lattice spacing does not solve the Clay continuum Navier–Stokes problem by itself.

## Host8 execution limitation and code behavior

Current GitHub source `run_host8_suite()` opens three Windows drive files `sector-000.rooms`, `sector-187.rooms`, and `sector-374.rooms` and processes **13,824 64-bit stubs**, or 110,592 bytes per probed sector. When a sector is absent, the source issues `[SKIP]` and continues without failing the suite. Consequently, an `--all` process exit code of 0 is insufficient to certify that physical Host8 runs occurred. This container has no access to the original Host8 files; its physical-disk claims remain externally unverified here.

## Reproduce the independent local executions

```bash
g++ -O3 -std=c++17 -Wall -Wextra -Werror -pedantic independent_cpp_equilibrium.cpp -o independent_cpp_equilibrium
./independent_cpp_equilibrium
g++ -O3 -std=c++17 -Wall -Wextra -Werror -pedantic independent_physics_probes.cpp -o independent_physics_probes
./independent_physics_probes
python independent_fit.py  # NumPy, pandas and SciPy required
```

Source files and generated CSVs are provided in the accompanying ZIP. They are original **independent checker implementations**, not redistributed GitHub source.
