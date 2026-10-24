// test_equivariant_matched_k.cpp
// ============================================================================
// Asolaria Matched-|k| Acoustic Dispersion and Isotropy Benchmark
// Directly compares Legacy Kernel (Arm 3) vs. Oh-Equivariant Inter-Shell Kernel (Arm 5)
// across matched wavevectors:
//   k1 = (3, 0, 0)   vs   k2 = (2, 2, 1)
// where both satisfy |k|^2 = 9 * (2*pi/L)^2 (identically matched wavelength).
// Uses QR-variable-projection damped harmonic fit (acoustic_damped_fit.h).
// ============================================================================

#include "acoustic_damped_fit.h"
#include "equivariant_intershell_kernel.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <numeric>
#include <cassert>

using std::array;
using std::vector;
using std::string;
using std::setw;
using std::setprecision;
using std::fixed;
using u8  = std::uint8_t;
using u64 = std::uint64_t;

constexpr int K = 27;

static inline int cell_index(int x, int y, int z, int L) {
    return (x * L + y) * L + z;
}

// ----------------------------------------------------------------------------
// Legacy Collision Operator (Arm 3)
// ----------------------------------------------------------------------------
struct LegacyCollision {
    int a, b, c, d;
    array<u8, 81> lut;
};

static vector<LegacyCollision> make_legacy_collisions(const array<asolaria::Vel, K>& v) {
    vector<LegacyCollision> cols;
    for (int a = 0; a < K; ++a) {
        for (int b = a + 1; b < K; ++b) {
            for (int c = b + 1; c < K; ++c) {
                for (int d = c + 1; d < K; ++d) {
                    if (v[a].x + v[b].x + v[c].x + v[d].x == 0 &&
                        v[a].y + v[b].y + v[c].y + v[d].y == 0 &&
                        v[a].z + v[b].z + v[c].z + v[d].z == 0 &&
                        v[a].e + v[b].e + v[c].e + v[d].e == 8) {
                        LegacyCollision col{a, b, c, d, {}};
                        std::map<array<int, 5>, vector<int>> classes;
                        for (int mask = 0; mask < 81; ++mask) {
                            col.lut[mask] = static_cast<u8>(mask);
                            classes[asolaria::get_invariants(asolaria::decode_mask(mask), {a, b, c, d}, v)].push_back(mask);
                        }
                        for (const auto& kv : classes) {
                            vector<int> codes;
                            for (int code : kv.second) if (code != 40) codes.push_back(code);
                            for (size_t j = 0; j + 1 < codes.size(); j += 2) {
                                int c1 = codes[j]; int c2 = codes[j + 1];
                                col.lut[c1] = static_cast<u8>(c2);
                                col.lut[c2] = static_cast<u8>(c1);
                            }
                        }
                        cols.push_back(col);
                    }
                }
            }
        }
    }
    return cols;
}

static void step_universe_legacy(vector<u8>& state, vector<u8>& next_state,
                                const vector<LegacyCollision>& cols,
                                const array<asolaria::Vel, K>& v, int L, int t) {
    int V = L * L * L;
    size_t total_c = cols.size();
    size_t gates_per_cell = 12;
    for (int cell = 0; cell < V; ++cell) {
        int base = cell * K;
        for (size_t j = 0; j < gates_per_cell; ++j) {
            size_t which = (static_cast<size_t>(t) * gates_per_cell +
                            static_cast<size_t>(cell) * 17 +
                            static_cast<size_t>(j) * 37) % total_c;
            const auto& col = cols[which];
            int in_mask = asolaria::encode_mask(state[base + col.a], state[base + col.b], state[base + col.c], state[base + col.d]);
            auto dec = asolaria::decode_mask(col.lut[in_mask]);
            state[base + col.a] = static_cast<u8>(dec[0]);
            state[base + col.b] = static_cast<u8>(dec[1]);
            state[base + col.c] = static_cast<u8>(dec[2]);
            state[base + col.d] = static_cast<u8>(dec[3]);
        }
    }
    for (int x = 0; x < L; ++x) {
        for (int y = 0; y < L; ++y) {
            for (int z = 0; z < L; ++z) {
                int src = cell_index(x, y, z, L);
                for (int ki = 0; ki < K; ++ki) {
                    int nx = (x + v[ki].x + L) % L;
                    int ny = (y + v[ki].y + L) % L;
                    int nz = (z + v[ki].z + L) % L;
                    int dst = cell_index(nx, ny, nz, L);
                    next_state[dst * K + ki] = state[src * K + ki];
                }
            }
        }
    }
    state = next_state;
}

// ----------------------------------------------------------------------------
// Equivariant Inter-Shell Operator (Arm 5)
// ----------------------------------------------------------------------------
static void step_lattice_equivariant(vector<u8>& state, vector<u8>& next_state,
                                    const vector<asolaria::Gate>& edge_gates,
                                    const asolaria::InterShellData& d,
                                    const array<asolaria::Vel, K>& v, int L) {
    int V = L * L * L;
    for (int cell = 0; cell < V; ++cell) {
        asolaria::apply_full_symmetric_collision(&state[cell * K], edge_gates, d);
    }
    for (int x = 0; x < L; ++x) {
        for (int y = 0; y < L; ++y) {
            for (int z = 0; z < L; ++z) {
                int src = cell_index(x, y, z, L);
                for (int ki = 0; ki < K; ++ki) {
                    int nx = (x + v[ki].x + L) % L;
                    int ny = (y + v[ki].y + L) % L;
                    int nz = (z + v[ki].z + L) % L;
                    int dst = cell_index(nx, ny, nz, L);
                    next_state[dst * K + ki] = state[src * K + ki];
                }
            }
        }
    }
    state = next_state;
}

// ----------------------------------------------------------------------------
// Matched-|k| Simulation & Measurement
// ----------------------------------------------------------------------------
struct MatchedKResult {
    string kernel;
    int L;
    string name;
    int kx, ky, kz;
    double k_mag;
    double omega;
    double cs;
    double alpha;
    double r_squared;
    double cycles;
};

static MatchedKResult run_matched_k_probe(int L, int kx, int ky, int kz, const string& name,
                                         bool use_equivariant,
                                         const vector<asolaria::Gate>& edge_gates,
                                         const asolaria::InterShellData& d,
                                         const vector<LegacyCollision>& leg_cols,
                                         const array<asolaria::Vel, K>& v) {
    int V = L * L * L;
    const double PI = std::acos(-1.0);
    double k_mag = (2.0 * PI / L) * std::sqrt(kx * kx + ky * ky + kz * kz);

    vector<u8> state(V * K, 1);
    vector<u8> next_state(V * K, 1);
    vector<double> cos_field(V, 0.0);

    for (int x = 0; x < L; ++x) {
        for (int y = 0; y < L; ++y) {
            for (int z = 0; z < L; ++z) {
                int c = cell_index(x, y, z, L);
                double phi = (2.0 * PI / L) * (kx * (x + 0.5) + ky * (y + 0.5) + kz * (z + 0.5));
                double cval = std::cos(phi);
                cos_field[c] = cval;

                int base = c * K;
                if (cval > 0.4) {
                    for (int k = 0; k < K; ++k) if (v[k].e >= 1) state[base + k] = 2;
                } else if (cval < -0.4) {
                    for (int k = 0; k < K; ++k) if (v[k].e >= 1) state[base + k] = 0;
                }
            }
        }
    }

    int ticks = 60;
    vector<double> t_series, amp_series;
    for (int t = 0; t <= ticks; ++t) {
        double amp = 0.0;
        for (int c = 0; c < V; ++c) {
            int rho = 0;
            for (int k = 0; k < K; ++k) rho += state[c * K + k];
            amp += (rho - 27) * cos_field[c];
        }
        t_series.push_back(static_cast<double>(t));
        amp_series.push_back(amp);
        if (t == ticks) break;

        if (use_equivariant) {
            step_lattice_equivariant(state, next_state, edge_gates, d, v, L);
        } else {
            step_universe_legacy(state, next_state, leg_cols, v, L, t);
        }
    }

    const auto fit = acoustic::fit(t_series, amp_series);
    double cs = fit.valid ? (fit.omega / k_mag) : 0.0;
    string kname = use_equivariant ? "Arm 5 (Equivariant Inter-Shell)" : "Arm 3 (Legacy)";
    return {kname, L, name, kx, ky, kz, k_mag, fit.omega, cs, fit.alpha, fit.r_squared, fit.cycles};
}

int main() {
    auto v = asolaria::make_vels();
    auto edge_gates = asolaria::make_edge_gates(v);
    auto d = asolaria::init_intershell_data(v);
    auto leg_cols = make_legacy_collisions(v);

    std::cout << "=======================================================================\n";
    std::cout << " ASOLARIA MATCHED-|k| ACOUSTIC DISPERSION & ISOTROPY BENCHMARK\n";
    std::cout << " Comparing: k1 = (3,0,0) vs k2 = (2,2,1) [Both |k|^2 = 9*(2pi/L)^2]\n";
    std::cout << " Legacy Kernel (Arm 3) vs Exact Oh-Equivariant Inter-Shell Kernel (Arm 5)\n";
    std::cout << "=======================================================================\n";

    std::ofstream csv("arm5_matched_k_dispersion_results.csv");
    csv << "kernel,L,name,kx,ky,kz,k_mag,fitted_omega,measured_cs,damping_alpha,r_squared,cycles\n";

    vector<MatchedKResult> all_results;
    bool all_fits_valid = true;

    for (int L : {24, 48}) {
        std::cout << "\n==================================================\n";
        std::cout << " Domain L = " << L << "x" << L << "x" << L << " (|k| = "
                  << fixed << setprecision(4) << (6.0 * std::acos(-1.0) / L) << ")\n";
        std::cout << "==================================================\n";

        // 1. Legacy
        std::cout << "\n--- Legacy Collision Kernel (Arm 3) ---\n";
        auto leg_r1 = run_matched_k_probe(L, 3, 0, 0, "Axis_3_0_0", false, edge_gates, d, leg_cols, v);
        auto leg_r2 = run_matched_k_probe(L, 2, 2, 1, "Oblique_2_2_1", false, edge_gates, d, leg_cols, v);
        all_results.push_back(leg_r1);
        all_results.push_back(leg_r2);

        double leg_mean_cs = (leg_r1.cs + leg_r2.cs) / 2.0;
        double leg_aniso = std::abs(leg_r1.cs - leg_r2.cs) / leg_mean_cs * 100.0;

        std::cout << "  - " << setw(18) << leg_r1.name << " : c_s = " << setprecision(5) << leg_r1.cs
                  << " | alpha = " << setprecision(5) << leg_r1.alpha
                  << " | R^2 = " << setprecision(5) << leg_r1.r_squared
                  << " | cycles = " << setprecision(2) << leg_r1.cycles << "\n";
        std::cout << "  - " << setw(18) << leg_r2.name << " : c_s = " << setprecision(5) << leg_r2.cs
                  << " | alpha = " << setprecision(5) << leg_r2.alpha
                  << " | R^2 = " << setprecision(5) << leg_r2.r_squared
                  << " | cycles = " << setprecision(2) << leg_r2.cycles << "\n";
        std::cout << "  ===> Legacy Matched-|k| Anisotropy: " << setprecision(2) << leg_aniso << "%\n";

        // 2. Equivariant Inter-Shell
        std::cout << "\n--- Equivariant Inter-Shell Kernel (Arm 5) ---\n";
        auto eq_r1 = run_matched_k_probe(L, 3, 0, 0, "Axis_3_0_0", true, edge_gates, d, leg_cols, v);
        auto eq_r2 = run_matched_k_probe(L, 2, 2, 1, "Oblique_2_2_1", true, edge_gates, d, leg_cols, v);
        all_results.push_back(eq_r1);
        all_results.push_back(eq_r2);

        double eq_mean_cs = (eq_r1.cs + eq_r2.cs) / 2.0;
        double eq_aniso = std::abs(eq_r1.cs - eq_r2.cs) / eq_mean_cs * 100.0;

        std::cout << "  - " << setw(18) << eq_r1.name << " : c_s = " << setprecision(5) << eq_r1.cs
                  << " | alpha = " << setprecision(5) << eq_r1.alpha
                  << " | R^2 = " << setprecision(5) << eq_r1.r_squared
                  << " | cycles = " << setprecision(2) << eq_r1.cycles << "\n";
        std::cout << "  - " << setw(18) << eq_r2.name << " : c_s = " << setprecision(5) << eq_r2.cs
                  << " | alpha = " << setprecision(5) << eq_r2.alpha
                  << " | R^2 = " << setprecision(5) << eq_r2.r_squared
                  << " | cycles = " << setprecision(2) << eq_r2.cycles << "\n";
        std::cout << "  ===> Arm 5 Matched-|k| Anisotropy: " << setprecision(2) << eq_aniso << "%\n";

        double reduction = leg_aniso / eq_aniso;
        std::cout << "  ===> Anisotropy Reduction Factor: " << setprecision(2) << reduction << "x\n";

        if (eq_r1.r_squared < 0.70 || eq_r2.r_squared < 0.70) all_fits_valid = false;
        if (eq_aniso >= leg_aniso) all_fits_valid = false;
    }

    for (const auto& r : all_results) {
        csv << r.kernel << "," << r.L << "," << r.name << ","
            << r.kx << "," << r.ky << "," << r.kz << ","
            << r.k_mag << "," << r.omega << "," << r.cs << ","
            << r.alpha << "," << r.r_squared << "," << r.cycles << "\n";
    }
    csv.close();

    std::cout << "\n=======================================================================\n";
    std::cout << " Results saved to arm5_matched_k_dispersion_results.csv\n";
    std::cout << " Matched-|k| Acoustic Benchmark: " << (all_fits_valid ? "PASS" : "FAIL") << "\n";
    std::cout << "=======================================================================\n";

    assert(all_fits_valid);
    return all_fits_valid ? 0 : 1;
}
