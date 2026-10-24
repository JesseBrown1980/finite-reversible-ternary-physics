# Independent review: Asolaria Host8 slice authentication and conditioned acoustic response

**Commit:** `d8bb94a9145b99bc52ed10d7319e1f2c1905781c`  
**Repository:** `JesseBrown1980/finite-reversible-ternary-physics`  
**Status:** Published sector slices independently cryptographically authenticated; physical parent drive origin not independently authenticated. Conditioned acoustic trajectory source reviewed and reported initial perturbations reproduced. Constitutive acoustic interpretation **not yet established**.

## 1. Exact binary authentication

I retrieved the three committed `data/host8_samples/sector-*.rooms` objects via GitHub's connector as base64 bytes and computed their SHA-256 using a standalone JavaScript SHA-256 implementation (validated against the standard `abc` test vector). For each file I computed SHA-256 on both the first 110,592 bytes and all 884,736 bytes. **All six hashes match**, independently of the repo's C++ and Python hash code.

| Sector | SHA-256: first 110,592 bytes | SHA-256: first 884,736 bytes | Result |
|---|---|---|---|
| 000 | `2dfe7d757ecb012475806b86fcdefc602680d042db836ffbcb2bfe924f2a18ee` | `9da76c8b3151fa372ff2ef378cb5018490f7460581035e3268b3afd4a85e6e0f` | MATCH |
| 187 | `6752ab9e2603cc5042f7c7a41c3ddc48ae5571fd0aeb945d0a3f63b1a948a652` | `b3bfb382de3eb473cd43328abb50e31c4e5222d734e8d39d8e80b47e6f19ad07` | MATCH |
| 374 | `963805be806e8a290aab8dee48b0d214d03015c84e02bf844f0de87e2b37c10a` | `11969dcf23ae7cb32103d40e8c12b4f7a87eec2b614c02ea2e9675bc80e2bae4` | MATCH |

The L24 block is a prefix of each L48 block; these are **six hashes of three objects**, not six independent sectors. The committed sample bytes do not themselves authenticate the original physical 300 GB volume, disk identity, acquisition time, or device provenance.

## 2. Independent integer decoding of committed bytes

I independently interpreted every little-endian 64-bit room stub into 27 base-3 occupancy values in the repository's velocity-index order and summed mass, momentum, and doubled microscopic energy with integer arithmetic. All results exactly matched `host8_independent_readonly_report.csv`.

| Sector | L | Mass | Doubled microscopic energy | (Px,Py,Pz) | Trit marginal entropy |
|---|---:|---:|---:|---|---:|
| 000 | 24 | 372215 | 744952 | (176, 309, -25) | 1.098606431 |
| 000 | 48 | 2984104 | 5970036 | (-764, 81, 2949) | 1.098611549 |
| 187 | 24 | 373159 | 745464 | (-854, 79, 597) | 1.098610859 |
| 187 | 48 | 2984328 | 5967494 | (-250, -1524, 1058) | 1.098612054 |
| 374 | 24 | 373024 | 745705 | (-449, -107, -263) | 1.098610546 |
| 374 | 48 | 2985241 | 5969407 | (-1847, -1147, 805) | 1.098612073 |

Near-`ln(3)` marginal trit entropy shows nearly balanced trit frequencies. It does not by itself prove statistical equilibrium or lack of spatial correlations.

**Encoder scope:** Mapping a 64-bit room stub to exactly 27 trits by repeated `value % 3` discards the quotient beyond `3^27 = 7,625,597,484,987` for sufficiently large stubs. Microstate Loschmidt recurrence proves reversibility **after decoding**, not round-trip recovery of the complete original 64-bit raw room stub.

## 3. Exact Host8-conditioned acoustic initialization

Using the actual retrieved binary bytes, I reconstructed the proposed rest-channel perturbation at tick 0 and the signed cosine Fourier projection. All six predictions match the committed `host8_linear_response_timeseries.csv` to six decimal places.

| Sector | Mode | Computed differential amplitude t=0 | Modified rest trits | Initial net mass change |
|---|---|---:|---:|---:|
| 000 | Axis | 0.401941 | 6897 | +79 |
| 000 | Oblique | 0.393609 | 6907 | +51 |
| 187 | Axis | 0.404539 | 6942 | -8 |
| 187 | Oblique | 0.394372 | 6935 | -69 |
| 374 | Axis | 0.401256 | 6889 | +43 |
| 374 | Oblique | 0.394349 | 6962 | -56 |

Thus the published response is genuinely conditioned on these initial host microstates. The perturbation is discrete and finite (roughly half the lattice rest occupancies changed), and initial global mass is not exactly matched between control and perturbation. Subtracting a control trajectory is useful but does not alone prove the small-perturbation limit or an exact Green's function.

## 4. Review of the 12 frequency and damping fits

The source and published CSV show:

- 12 fits (3 sectors x 2 operators x 2 orientations).
- 6/12 have at least three fitted cycles.
- 7/12 have R-squared >= 0.70.
- **2/12 satisfy both requirements**: Sector 000, Arm 3 oblique; Sector 187, Arm 5 oblique.
- Several fit parameters hit explicit search boundaries. Arm 5 axes in sectors 000 and 374 fit exactly **0.25 cycles**, the frequency lower bound. Arm 3 oblique in sectors 187 and 374 uses damping at `20/48` (the upper bound). Arm 3 sector 000 axis uses frequency close to `0.99*pi` (upper Nyquist boundary).
- The Arm 3 **axis differential Fourier amplitude is constant at all 49 recorded ticks in all three sectors**. Its fitted frequencies and speeds therefore do not represent propagating axis sound. The published R-squared values of 0.261, 0.157 and 0.168 agree that the harmonic model is not an adequate description.

**Crucial implementation difference:** In `test_host8_acoustic_response.cpp`, the operator labeled Arm 3 always applies `gates[0]` through `gates[31]`, identically at every cell/tick. Main `asolaria_equilibrium_suite.cpp` uses a time- and cell-dependent selection from all 288 gates. This Host8-conditioned Arm 3 is **not the same dynamical baseline** as the main simulation Arm 3, even though it constructs the full 288-gate list.

**Constitutive conclusion:** Response decay and paired subtraction are confirmed observations from the posted CSV. The evidence is insufficient to assert a unique acoustic Green's function or to attribute the fitted damping values specifically to microscopic scattering into thermal degrees of freedom. The phase, damping, and dispersion must be validated on additional wavelengths, perturbation amplitudes, and true Arm 3 schedules with held-out conditions.

## 5. Test-harness issue

`test_host8_acoustic_response.cpp` finishes with `return 0` regardless of the `three_cycle_pass` or fit-quality columns. It checks hashes with `assert`, but **does not fail the acoustic-response stage when fit identification fails**. The current `reproduce.py` checks whether this executable exits successfully, not whether every claimed sound mode is identified. The overall master-suite PASS thus means the data were generated and the run completed, not that all 12 sound fits passed scientific acceptance criteria.

## 6. Next experiment

1. Run the **actual** main Arm 3 288-gate time/cell scheduler from a single shared implementation, alongside Arm 5. No custom first-32-gate baseline.
2. For each authenticated Host8 initial state, use paired control and perturbation trajectories with *exactly conserved total mass*; repeat at strengths 1, 1/2, 1/4 in ensemble-averaged perturbations or other controlled finite-amplitude scaling.
3. Sample multiple spatial wavelengths and many spatial phase shifts, testing train-on-some/predict-others behavior. Record density and longitudinal/transverse momentum and energy Fourier components, not density alone.
4. Require >=3 resolved fitted cycles, good fit quality, no fitted parameter at bounds, positive clear Fourier peak/phase propagation, and agreement across temporal windows; otherwise label result **non-acoustic / unidentified** and do not report `omega/k` as sound speed.
5. Authenticate the original physical source with signed capture manifest and immutable parent-sector hashes if a claim of physical-origin provenance is required. The committed slices prove reproducibility of the published data, not provenance of the full drive.

### Reproduction

Run `python verify_host8_samples_and_response.py --repo /path/to/cloned/repo`. Uses only Python standard library and fails closed if source bytes are missing or changed. In my compute environment direct GitHub-download/mounting is blocked; I independently verified and decoded the published bytes **inside the connected GitHub tool runtime** rather than passing them into the native container. The checker's missing-input fail-closed behavior was separately executed in the native container.
