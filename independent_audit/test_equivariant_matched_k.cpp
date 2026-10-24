// test_equivariant_matched_k.cpp
// ============================================================================
// Asolaria Matched-|k| Acoustic Dispersion and Isotropy Benchmark
// Directly compares True 288-Gate Arm 3 vs. Oh-Equivariant Inter-Shell Arm 5
// across matched wavevectors:
//   k1 = (3, 0, 0)   vs   k2 = (2, 2, 1)
// where both satisfy |k|^2 = 9 * (2*pi/L)^2 (identically matched wavenumber).
// Uses QR-variable-projection damped harmonic fit (acoustic_damped_fit.h).
// Includes Host 8 300 GB process room substrate validation.
// ============================================================================

#include "acoustic_damped_fit.h"
#include "equivariant_intershell_kernel.h"
#include "sha256.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <numeric>
#include <algorithm>
#include <map>
#include <cassert>

using std::array;
using std::vector;
using std::string;
using std::setw;
using std::setprecision;
using std::fixed;
using std::map;
using std::tuple;
using u8  = std::uint8_t;
using u64 = std::uint64_t;

constexpr int K = 27;

static inline int cell_index(int x, int y, int z, int L) {
    return (x * L + y) * L + z;
}

// ----------------------------------------------------------------------------
// True 288-Gate Standard Arm 3 Collision Operator (32 gates/cell/tick)
// ----------------------------------------------------------------------------
struct Arm3Gate {
    int a, b, c, d;
    array<u8, 81> lut;
};

static vector<Arm3Gate> make_arm3_gates(const array<asolaria::Vel, K>& v) {
    using PairKey = tuple<int, int, int, int>;
    map<PairKey, vector<array<int, 2>>> pair_map;
    for (int i = 0; i < K; ++i) {
        for (int j = i + 1; j < K; ++j) {
            auto a = v[i], b = v[j];
            pair_map[{a.x + b.x, a.y + b.y, a.z + b.z, a.e + b.e}].push_back({i, j});
        }
    }
    map<array<int, 4>, bool> unique_quads;
    for (const auto& kv : pair_map) {
        const auto& pairs = kv.second;
        for (size_t i = 0; i < pairs.size(); ++i) {
            for (size_t j = i + 1; j < pairs.size(); ++j) {
                auto p1 = pairs[i];
                auto p2 = pairs[j];
                array<int, 4> q = {p1[0], p1[1], p2[0], p2[1]};
                std::sort(q.begin(), q.end());
                if (std::adjacent_find(q.begin(), q.end()) == q.end()) {
                    unique_quads[q] = true;
                }
            }
        }
    }
    vector<Arm3Gate> gates;
    for (const auto& kv : unique_quads) {
        array<int, 4> q = kv.first;
        Arm3Gate g{q[0], q[1], q[2], q[3], {}};
        map<array<int, 5>, vector<int>> classes;
        for (int mask = 0; mask < 81; ++mask) {
            g.lut[mask] = static_cast<u8>(mask);
            classes[asolaria::get_invariants(asolaria::decode_mask(mask), q, v)].push_back(mask);
        }
        for (const auto& c_entry : classes) {
            vector<int> codes;
            for (int code : c_entry.second) if (code != 40) codes.push_back(code);
            for (size_t j = 0; j + 1 < codes.size(); j += 2) {
                int c1 = codes[j]; int c2 = codes[j + 1];
                g.lut[c1] = static_cast<u8>(c2);
                g.lut[c2] = static_cast<u8>(c1);
            }
        }
        gates.push_back(g);
    }
    return gates;
}

static void step_universe_arm3(vector<u8>& state, vector<u8>& next_state,
                              const vector<Arm3Gate>& gates,
                              const array<asolaria::Vel, K>& v, int L, int t) {
    int V = L * L * L;
    size_t total_c = gates.size();
    constexpr int gates_per_cell = 32;
    for (int cell = 0; cell < V; ++cell) {
        int base = cell * K;
        for (int j = 0; j < gates_per_cell; ++j) {
            size_t which = (static_cast<size_t>(t) * gates_per_cell +
                            static_cast<size_t>(cell) * 17 +
                            static_cast<size_t>(j) * 37) % total_c;
            const auto& g = gates[which];
            int in_mask = asolaria::encode_mask(state[base + g.a], state[base + g.b], state[base + g.c], state[base + g.d]);
            auto dec = asolaria::decode_mask(g.lut[in_mask]);
            state[base + g.a] = static_cast<u8>(dec[0]);
            state[base + g.b] = static_cast<u8>(dec[1]);
            state[base + g.c] = static_cast<u8>(dec[2]);
            state[base + g.d] = static_cast<u8>(dec[3]);
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
static void step_lattice_arm5(vector<u8>& state, vector<u8>& next_state,
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

static void step_lattice_arm5_backward(vector<u8>& state, vector<u8>& next_state,
                                       const vector<asolaria::Gate>& edge_gates,
                                       const asolaria::InterShellData& d,
                                       const array<asolaria::Vel, K>& v, int L) {
    int V = L * L * L;
    // 1. Backward streaming: undo spatial transport
    for (int x = 0; x < L; ++x) {
        for (int y = 0; y < L; ++y) {
            for (int z = 0; z < L; ++z) {
                int curr = cell_index(x, y, z, L);
                for (int ki = 0; ki < K; ++ki) {
                    int prev_x = (x - v[ki].x + L) % L;
                    int prev_y = (y - v[ki].y + L) % L;
                    int prev_z = (z - v[ki].z + L) % L;
                    int src = cell_index(prev_x, prev_y, prev_z, L);
                    next_state[src * K + ki] = state[curr * K + ki];
                }
            }
        }
    }
    state = next_state;
    // 2. Inverse collision: apply inverse equivariant collision map
    for (int cell = 0; cell < V; ++cell) {
        asolaria::apply_full_symmetric_collision_backward(&state[cell * K], edge_gates, d);
    }
}

// ----------------------------------------------------------------------------
// Host 8 Substrate Binary Loader
// ----------------------------------------------------------------------------
static bool load_host8_rooms(const string& path, u64 offset, int num_cells, vector<u8>& state, string& out_sha256) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    file.seekg(offset * 8);
    vector<u64> stubs(num_cells);
    file.read(reinterpret_cast<char*>(stubs.data()), num_cells * 8);
    if (!file) return false;

    out_sha256 = asolaria::SHA256::hash_bytes(reinterpret_cast<const uint8_t*>(stubs.data()), num_cells * 8);

    state.resize(num_cells * K);
    for (int cell = 0; cell < num_cells; ++cell) {
        u64 val = stubs[cell];
        for (int ki = 0; ki < K; ++ki) {
            state[cell * K + ki] = static_cast<u8>(val % 3);
            val /= 3;
        }
    }
    return true;
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
    bool three_cycle_pass;
};

static MatchedKResult run_matched_k_probe(int L, int kx, int ky, int kz, const string& name,
                                         bool use_equivariant,
                                         const vector<asolaria::Gate>& edge_gates,
                                         const asolaria::InterShellData& d,
                                         const vector<Arm3Gate>& arm3_gates,
                                         const array<asolaria::Vel, K>& v,
                                         int ticks,
                                         std::ofstream& ts_csv) {
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

                // Weak linear acoustic perturbation (density in [26, 28], net P = 0)
                int base = c * K;
                if (cval > 0.4) {
                    state[base + 13] = 0; // rest -1
                    state[base + 22] = 2; // face +1
                    state[base + 4]  = 2; // face +1 => Delta rho = +1
                } else if (cval < -0.4) {
                    state[base + 13] = 2; // rest +1
                    state[base + 22] = 0; // face -1
                    state[base + 4]  = 0; // face -1 => Delta rho = -1
                }
            }
        }
    }

    vector<double> t_series, amp_series;
    string kname = use_equivariant ? "Arm 5 (Equivariant Inter-Shell)" : "Arm 3 (Standard 288-Gate)";

    for (int t = 0; t <= ticks; ++t) {
        double amp = 0.0;
        for (int c = 0; c < V; ++c) {
            int rho = 0;
            for (int ki = 0; ki < K; ++ki) rho += state[c * K + ki];
            amp += (rho - 27) * cos_field[c];
        }
        t_series.push_back(static_cast<double>(t));
        amp_series.push_back(amp);
        ts_csv << kname << "," << L << "," << name << "," << t << "," << amp << "\n";
        if (t == ticks) break;

        if (use_equivariant) {
            step_lattice_arm5(state, next_state, edge_gates, d, v, L);
        } else {
            step_universe_arm3(state, next_state, arm3_gates, v, L, t);
        }
    }

    const auto fit = acoustic::fit(t_series, amp_series);
    double cs = fit.valid ? (fit.omega / k_mag) : 0.0;
    bool three_cycle = (fit.cycles >= 3.0);
    return {kname, L, name, kx, ky, kz, k_mag, fit.omega, cs, fit.alpha, fit.r_squared, fit.cycles, three_cycle};
}

int main() {
    auto v = asolaria::make_vels();
    auto arm3_gates = make_arm3_gates(v);
    auto arm5_edge_gates = asolaria::make_edge_gates(v);
    auto arm5_d = asolaria::init_intershell_data(v);

    std::cout << "=======================================================================\n";
    std::cout << " ASOLARIA MATCHED-|k| ACOUSTIC DISPERSION & ISOTROPY BENCHMARK\n";
    std::cout << " Baseline: TRUE Arm 3 (288 Gates, 32 Gates/Cell/Tick)\n";
    std::cout << " Candidate: Arm 5 (Exact Oh-Equivariant Inter-Shell Collision Kernel)\n";
    std::cout << " Perturbation: Linear Small-Signal Acoustic Wave (rho in [26, 28])\n";
    std::cout << " Comparing Matched Wavevectors: k1 = (3,0,0) vs k2 = (2,2,1)\n";
    std::cout << "=======================================================================\n";

    assert(arm3_gates.size() == 288);
    std::cout << "[PASS] Verified Arm 3 Gate Count: " << arm3_gates.size() << " / 288 distinct gates.\n";

    std::ofstream csv("arm5_matched_k_dispersion_results.csv");
    csv << "kernel,L,name,kx,ky,kz,k_mag,fitted_omega,measured_cs,damping_alpha,r_squared,cycles,three_cycle_pass\n";

    std::ofstream ts_csv("arm5_matched_k_timeseries.csv");
    ts_csv << "kernel,L,name,tick,fourier_amplitude\n";

    vector<MatchedKResult> all_results;
    bool all_checks_passed = true;

    for (int L : {24, 48}) {
        int ticks = (L == 48) ? 96 : 60;
        std::cout << "\n==================================================\n";
        std::cout << " Domain L = " << L << "x" << L << "x" << L
                  << " (|k| = " << fixed << setprecision(4) << (6.0 * std::acos(-1.0) / L)
                  << ", Ticks = " << ticks << ")\n";
        std::cout << "==================================================\n";

        // 1. True Arm 3 Baseline
        std::cout << "\n--- Arm 3 Baseline (288 Gates, 32 Gates/Cell/Tick) ---\n";
        auto a3_r1 = run_matched_k_probe(L, 3, 0, 0, "Axis_3_0_0", false, arm5_edge_gates, arm5_d, arm3_gates, v, ticks, ts_csv);
        auto a3_r2 = run_matched_k_probe(L, 2, 2, 1, "Oblique_2_2_1", false, arm5_edge_gates, arm5_d, arm3_gates, v, ticks, ts_csv);
        all_results.push_back(a3_r1);
        all_results.push_back(a3_r2);

        double a3_mean_cs = (a3_r1.cs + a3_r2.cs) / 2.0;
        double a3_aniso = std::abs(a3_r1.cs - a3_r2.cs) / a3_mean_cs * 100.0;

        std::cout << "  - " << setw(18) << a3_r1.name << " : c_s = " << setprecision(5) << a3_r1.cs
                  << " | alpha = " << setprecision(5) << a3_r1.alpha
                  << " | R^2 = " << setprecision(5) << a3_r1.r_squared
                  << " | cycles = " << setprecision(2) << a3_r1.cycles
                  << " | 3-cycle: " << (a3_r1.three_cycle_pass ? "PASS" : "FAIL") << "\n";
        std::cout << "  - " << setw(18) << a3_r2.name << " : c_s = " << setprecision(5) << a3_r2.cs
                  << " | alpha = " << setprecision(5) << a3_r2.alpha
                  << " | R^2 = " << setprecision(5) << a3_r2.r_squared
                  << " | cycles = " << setprecision(2) << a3_r2.cycles
                  << " | 3-cycle: " << (a3_r2.three_cycle_pass ? "PASS" : "FAIL") << "\n";
        std::cout << "  ===> Arm 3 Matched-|k| Anisotropy: " << setprecision(2) << a3_aniso << "%\n";

        // 2. Arm 5 Equivariant Inter-Shell
        std::cout << "\n--- Arm 5 (Exact Oh-Equivariant Inter-Shell Kernel) ---\n";
        auto a5_r1 = run_matched_k_probe(L, 3, 0, 0, "Axis_3_0_0", true, arm5_edge_gates, arm5_d, arm3_gates, v, ticks, ts_csv);
        auto a5_r2 = run_matched_k_probe(L, 2, 2, 1, "Oblique_2_2_1", true, arm5_edge_gates, arm5_d, arm3_gates, v, ticks, ts_csv);
        all_results.push_back(a5_r1);
        all_results.push_back(a5_r2);

        double a5_mean_cs = (a5_r1.cs + a5_r2.cs) / 2.0;
        double a5_aniso = std::abs(a5_r1.cs - a5_r2.cs) / a5_mean_cs * 100.0;

        std::cout << "  - " << setw(18) << a5_r1.name << " : c_s = " << setprecision(5) << a5_r1.cs
                  << " | alpha = " << setprecision(5) << a5_r1.alpha
                  << " | R^2 = " << setprecision(5) << a5_r1.r_squared
                  << " | cycles = " << setprecision(2) << a5_r1.cycles
                  << " | 3-cycle: " << (a5_r1.three_cycle_pass ? "PASS" : "FAIL") << "\n";
        std::cout << "  - " << setw(18) << a5_r2.name << " : c_s = " << setprecision(5) << a5_r2.cs
                  << " | alpha = " << setprecision(5) << a5_r2.alpha
                  << " | R^2 = " << setprecision(5) << a5_r2.r_squared
                  << " | cycles = " << setprecision(2) << a5_r2.cycles
                  << " | 3-cycle: " << (a5_r2.three_cycle_pass ? "PASS" : "FAIL") << "\n";
        std::cout << "  ===> Arm 5 Matched-|k| Anisotropy: " << setprecision(2) << a5_aniso << "%\n";

        if (a3_r1.r_squared < 0.70 || a3_r2.r_squared < 0.70) all_checks_passed = false;
        if (a5_r1.r_squared < 0.65 || a5_r2.r_squared < 0.65) all_checks_passed = false;
    }

    for (const auto& r : all_results) {
        csv << r.kernel << "," << r.L << "," << r.name << ","
            << r.kx << "," << r.ky << "," << r.kz << ","
            << r.k_mag << "," << r.omega << "," << r.cs << ","
            << r.alpha << "," << r.r_squared << "," << r.cycles << ","
            << (r.three_cycle_pass ? "1" : "0") << "\n";
    }
    csv.close();
    ts_csv.close();

    // ------------------------------------------------------------------------
    // Host 8 300 GB Physical Substrate Live Verification
    // ------------------------------------------------------------------------
    std::cout << "\n=======================================================================\n";
    std::cout << " HOST8 300 GB PHYSICAL SUBSTRATE BENCHMARK (SECTORS 000, 187, 374)\n";
    std::cout << "=======================================================================\n";

    string host8_dir = "D:/ASOLARIA-HOST8-ROOMS-300G";
    std::ofstream host8_csv("host8_matched_k_results.csv");
    host8_csv << "sector,L,cells,trits,sha256_hash,auth_match,arm5_dE,arm5_loschmidt_mismatches,status\n";

    const map<int, string> auth_hashes_l24 = {
        {0,   "2dfe7d757ecb012475806b86fcdefc602680d042db836ffbcb2bfe924f2a18ee"},
        {187, "6752ab9e2603cc5042f7c7a41c3ddc48ae5571fd0aeb945d0a3f63b1a948a652"},
        {374, "963805be806e8a290aab8dee48b0d214d03015c84e02bf844f0de87e2b37c10a"}
    };
    const map<int, string> auth_hashes_l48 = {
        {0,   "9da76c8b3151fa372ff2ef378cb5018490f7460581035e3268b3afd4a85e6e0f"},
        {187, "b3bfb382de3eb473cd43328abb50e31c4e5222d734e8d39d8e80b47e6f19ad07"},
        {374, "11969dcf23ae7cb32103d40e8c12b4f7a87eec2b614c02ea2e9675bc80e2bae4"}
    };

    bool host8_available = std::ifstream(host8_dir + "/sector-000.rooms").is_open();

    if (host8_available) {
        std::cout << "Physical 300 GB Host8 array mounted at " << host8_dir << ".\n";
        for (int sec : {0, 187, 374}) {
            std::ostringstream ss;
            ss << host8_dir << "/sector-" << setw(3) << std::setfill('0') << sec << ".rooms";
            string path = ss.str();

            for (int L_h : {24, 48}) {
                int V_h = L_h * L_h * L_h;
                vector<u8> state;
                string block_sha256;
                bool ok = load_host8_rooms(path, 0, V_h, state, block_sha256);
                assert(ok);

                string expected_sha = (L_h == 24) ? auth_hashes_l24.at(sec) : auth_hashes_l48.at(sec);
                bool hash_match = (block_sha256 == expected_sha);

                std::cout << "  - Sector " << setw(3) << std::setfill('0') << sec
                          << " (L=" << L_h << "^3, " << (V_h * K) << " trits) : SHA-256 = " << block_sha256.substr(0, 16) << "... "
                          << (hash_match ? "[AUTHENTICATED]" : "[MISMATCH]") << "\n";
                if (!hash_match) all_checks_passed = false;

                // Test Arm 5 Energy Conservation
                vector<u8> test_state = state;
                vector<u8> scratch(V_h * K, 0);
                auto compute_energy = [&](const vector<u8>& s) {
                    double E = 0.0;
                    for (int c = 0; c < V_h; ++c) {
                        for (int ki = 0; ki < K; ++ki) E += s[c * K + ki] * v[ki].e;
                    }
                    return E / V_h;
                };
                double E0 = compute_energy(test_state);
                for (int t = 0; t < 24; ++t) {
                    step_lattice_arm5(test_state, scratch, arm5_edge_gates, arm5_d, v, L_h);
                }
                double E_final = compute_energy(test_state);
                double dE = std::abs(E_final - E0);
                bool cons_ok = (dE < 1e-12);
                std::cout << "    * Arm 5 Energy Conservation: dE = " << std::scientific << dE
                          << (cons_ok ? " [PASS]" : " [FAIL]") << "\n";
                if (!cons_ok) all_checks_passed = false;

                // Test Arm 5 Loschmidt Reversibility on Host8
                vector<u8> rev_state = state;
                for (int t = 0; t < 8; ++t) {
                    step_lattice_arm5(rev_state, scratch, arm5_edge_gates, arm5_d, v, L_h);
                }
                for (int t = 0; t < 8; ++t) {
                    step_lattice_arm5_backward(rev_state, scratch, arm5_edge_gates, arm5_d, v, L_h);
                }
                int mismatches = 0;
                for (size_t i = 0; i < state.size(); ++i) {
                    if (rev_state[i] != state[i]) mismatches++;
                }
                bool rev_ok = (mismatches == 0);
                std::cout << "    * Arm 5 Loschmidt Reversibility: " << mismatches << " / " << (V_h * K)
                          << " mismatches " << (rev_ok ? "[100% BIT-FOR-BIT PASS]" : "[FAIL]") << "\n";
                if (!rev_ok) all_checks_passed = false;

                host8_csv << sec << "," << L_h << "," << V_h << "," << (V_h * K) << ","
                          << block_sha256 << "," << (hash_match ? "1" : "0") << ","
                          << dE << "," << mismatches << ","
                          << (hash_match && cons_ok && rev_ok ? "VERIFIED" : "FAILED") << "\n";
            }
        }
    } else {
        std::cout << "[SKIP] Host8 drive not mounted at " << host8_dir << ".\n";
    }
    host8_csv.close();

    std::cout << "\n=======================================================================\n";
    std::cout << " Benchmark Results Exported:\n";
    std::cout << "  - arm5_matched_k_dispersion_results.csv (Summary metrics)\n";
    std::cout << "  - arm5_matched_k_timeseries.csv         (Raw signed Fourier signals)\n";
    std::cout << "  - host8_matched_k_results.csv           (Host 8 physical substrate logs)\n";
    std::cout << " Master Verification Status: " << (all_checks_passed ? "PASS" : "FAIL") << "\n";
    std::cout << "=======================================================================\n";

    assert(all_checks_passed);
    return all_checks_passed ? 0 : 1;
}
