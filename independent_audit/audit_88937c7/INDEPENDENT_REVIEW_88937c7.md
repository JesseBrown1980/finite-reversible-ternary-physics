# Independent review — finite-reversible-ternary-physics (`88937c7`)

**Review date:** 2026-10-24  
**Pinned commit:** `88937c75a6311bf067309089b5a7d8dcb4bf2d4a`  
**Repositories:** `JesseBrown1980/finite-reversible-ternary-physics` and `JesseBrown1980/finite-reversible-ternary-system-generate-the-experimentally-measured-laws-of-physics-from`  
**Environment:** GCC 14.2, Debian Linux container, `-O3 -std=c++17 -Wall -Wextra -Werror -static`.  
**Execution:** Independent C++17 reimplementation, not a claim to have run the original GitHub C++ executable or read Host8 disk bytes.

## What was verified

- Both GitHub repositories were at exactly the specified commit.
- New `face_lut.h` contains 729 entries; **180 nonidentity states** (24.691%) and 549 fixed points.
- From the seven representative orbit-state pairs `(8,360), (17,49), (23,343), (50,658), (53,341), (80,688), (107,427)`, independently reconstructing the map under all 48 signed cubic transformations **reproduced every one of the 729 published table entries**. No conflicting orbit assignments.
- Exhaustive checks on all 729 face configurations: **zero failures** of involution, integer mass/momentum/energy conservation, or commutation with any of the 48 cubic transformations.
- Independent native C++17 implementation reconstructed the face table and three 4-channel edge involutions. It passed 5,000 random full-cell state conservation, shell conservation, involution, and `C(Rs)=R(C(s))` checks across 48 group operations (zero failures).
- Independent 8³-lattice, two-step `F(Rs)=R(F(s))`: **zero mismatches across 663,552 trit comparisons**.
- Reproduced exactly the published *initial collision activity* for the face sound perturbation (**3,456/13,824 cells**) and thermal slab (**13,824/13,824 cells**).
- The deterministic 48-step thermal-slab simulation preserved global particle counts in each speed shell separately: rest **13,824**, face **82,944**, edge **165,888**, corner **110,592** before and after.
- The three edge gate LUTs each activate **30 of 81** equally weighted local four-trit inputs. Hence for independently uniform random occupancies, the exact full-cell activation probability is `1 - (549/729) * (51/81)^3 = 81.2024916%`; an independently seeded 5,000-state sample activated **4,093 / 5,000 = 81.86%**. The reported **97.6%** random microcanonical activity is not established by this test and would require its exact sampling distribution/seed; it is not the generic uniform-random expectation.
- Fail-fast exit handling added in the original local and lattice equivariance tests and compilation guards in `reproduce.py`. Note that errors are accumulated and returned after the test, rather than exiting at the first mismatch.

## Critical physical interpretation

**The new operator is block-diagonal across velocity shells**: it is the direct product of an equivariant face-shell involution and three equivariant edge-shell involutions, with identity on the rest and corner shells. Consequently, the collision step conserves not only total mass, momentum, and kinetic energy but also **the number of particles in each velocity shell independently**. Because streaming never changes a particle's velocity channel, the global totals for rest, face, edge, and corner remain conserved for the entire dynamics. Therefore the implementation currently enables **within-shell scattering in multiple shells, but not inter-shell exchange**. Its claim of cross-shell coupling/ergodic thermalization is not established by the implementation.

**Active collision counts are not measurements of heat conduction, viscosity or damping.** The `test_multishell_equivariance.cpp` acoustic and thermal tests count changed cells after **one collision step**; they do not extract diffusion coefficients, sound speeds, temperature trajectories or vortex-stretching rates for the new kernel. The hot/cold slab starts with cell particle densities of **31 and 23**, respectively (rather than constant density), so it simultaneously injects a density contrast. Its net momentum is zero globally, but each half has structured momentum.

**The main benchmark engine is still the earlier operator.** The commit retains the same `asolaria_equilibrium_suite.cpp` Git blob (`d44fa88804c20ef03290cfb32eba5811577193f4`) and prior acoustic/TGV/thermal results. The new `face_lut.h` is included by `test_multishell_equivariance.cpp`, not incorporated into the main fluid-acoustic/thermal benchmark operator. The prior measured acoustic dispersion and transport curves do **not** validate this new collision operator.

The independent local/L8 tests establish exact *cubic* `O_h` equivariance, not continuous SO(3) invariance or a continuum Navier–Stokes regularity theorem.

The original Host8 block bytes were not available in this container. The published digest/provenance claims can be read, but not independently authenticated here.

## Recommended next experiment

Introduce a reversible `O_h`-equivariant operator that couples distinct velocity shells while conserving only the intended global invariants. Since face and edge velocity channels are disjoint from corner/rest, applying separate operators cannot produce inter-shell energy exchange. One way forward is to **search full-state orbit pairs** under `O_h` with exactly matching `(mass, vector momentum, kinetic energy)` and compatible stabilizers that have *different shell occupation totals*. Use orbit-equivariant involution swaps on such pairs. Then retest reversible multi-shell dynamics and run the actual TGV, matched-|k| acoustic, and thermal transport benchmarks using the new kernel with negative controls and appropriate physical diagnostic extraction.

## Local execution

```sh
g++ -O3 -std=c++17 -Wall -Wextra -Werror -static independent_multishell.cpp -o independent_multishell
./independent_multishell
```

See `run.log` for actual container output. Note that `independent_multishell.cpp` reconstructs the published table algorithmically from seven seed pairs instead of importing `face_lut.h`, making this a useful independent confirmation of the table and its behavior.
