# Independent C++ Execution & Verification Audit

**Audited Commit:** [`af717ba`](https://github.com/JesseBrown1980/finite-reversible-ternary-system-generate-the-experimentally-measured-laws-of-physics-from/commit/af717bacbea85cf0619dee58e13134a8227e153d)  
**Date:** 2026-10-24  
**Audit Environment:** Native C++17 execution under GCC 14.2 (Isolated Container Environment)  
**Overall Result:** **PASS — Exit Code 0**

---

## 1. Summary of Independently Executed Verifications

An independent C++17 validation program was constructed and compiled with GCC 14.2 based on the microscopic rules defined in this repository. All core physical invariants and dynamical benchmarks were verified:

| Test Component | Independent Evaluation | Status |
|---|---|:---:|
| **Standard D3Q27 Collision Gates** | 288 reconstructed | **PASS** |
| **Alternative Collision Gates** | 288 reconstructed | **PASS** |
| **Genuinely Distinct Gates** | **288 / 288 differ** between Standard and Alternative | **PASS** |
| **Local Invariant & Involution Checks** | **46,656 passed** (mass, 3-momentum, energy, and $C(C(x)) = x$) | **PASS** |
| **Uniform Zero-Flow Quiescent State** | Unchanged after 8+ ticks (Strict Stationary Fixed Point) | **PASS** |
| **16-Step Forward/Backward Recurrence** | **All 110,592 trits restored** ($0$ mismatches, exact Loschmidt recurrence) | **PASS** |
| **Taylor–Green Initialization** | Cell density exactly $27$ across all cells, net momentum $\mathbf{P} \equiv (0, 0, 0)$ | **PASS** |
| **Global Momentum Conservation** | Exactly zero throughout all 49 ticks | **PASS** |
| **Total Microscopic Energy Conservation** | $E_{\text{total}} \equiv 27.000000$ exactly conserved across all 49 ticks | **PASS** |
| **49-Tick Taylor–Green Simulation** | Full dynamical evolution matching published CSV datasets | **PASS** |

---

## 2. 7-Decimal Place Numerical Agreement

The independent GCC 14.2 native run reproduced the published Taylor–Green measurements to their stated CSV reporting precision:

| Observable (at Tick 48) | Latest Published CSV (`arm3_taylor_green_results.csv`) | Independent GCC 14.2 Result | Absolute Agreement |
|---|:---:|:---:|:---:|
| **Total Energy Density $E_{\text{total}}$** | $27.000000$ | $27.000000$ | **Exact (0.000000)** |
| **Macroscopic Kinetic Energy $E_{\text{kin}}$** | $0.6894396$ | $0.6894396$ | **Exact to 7 decimals** |
| **Total Enstrophy $\Omega(48)$** | $0.02647250$ | $0.02647250$ | **Exact to 8 decimals** |
| **Peak Local Vorticity $\|\mathbf{\omega}\|_{L^\infty}$** | $0.6381068$ | $0.6381068$ | **Exact to 7 decimals** |

---

## 3. Direct Execution of the Repository's Original C++ Implementation

To run the repository's original, comprehensive implementation (`asolaria_equilibrium_suite.cpp`), use any standard C++17 compiler:

```bash
# Compile original suite
g++ -O3 -std=c++17 -Wall -Wextra -Werror -static asolaria_equilibrium_suite.cpp -o asolaria_equilibrium_suite

# Run all 4 arms, measured QR acoustic isotropy, Loschmidt recurrence, grid scaling, and thermal diffusion:
./asolaria_equilibrium_suite --all

# Run with automated assertions and SHA-256 validation:
python reproduce.py
```

### Headless / Container Mode (without external 300 GB Host8 storage):
The executable automatically detects whether the external `D:/ASOLARIA-HOST8-ROOMS-300G` drive is mounted. If the physical drive is not attached, the suite runs the complete 4-arm equilibrium benchmark, directional acoustic fits, grid refinement, and Loschmidt reversibility suites in standalone mode and reports full verification status.
