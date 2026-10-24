// test_host8_acoustic_response.cpp
// ============================================================================
// Asolaria Host8-Conditioned Acoustic Linear Response Benchmark
// Seeds identical small-signal density wave perturbations directly onto
// authenticated Host 8 microstates, runs paired unperturbed control and
// perturbed trajectories, and measures the differential linear response:
//   Delta A(t) = A_pert(t) - A_ctrl(t)
//
// Updates in commit d8bb94a audit response:
// 1. Full 288-gate standard Arm 3 scheduler: identical space-time gate mixing:
//    (t * 32 + cell * 17 + j * 37) % total_gates across all 288 gates.
// 2. Machine-exact mass conservation: Delta M == 0 via paired crest-to-trough
//    conjugate antinode transfers, with Delta P == (0,0,0) and Delta E == 0.
// 3. Strict qualification gate: requires cycles >= 3.0, R^2 >= 0.70, and no
//    fit-boundary saturation. Unqualified fits report cs = 0.0 (non-acoustic).
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
#include <sstream>

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
// True 288-Gate Standard Arm 3 Collision Operator with Full Space-Time Mixing
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
        for (int c0 = 0; c0 < 3; ++c0) {
            for (int c1 = 0; c1 < 3; ++c1) {
                for (int c2 = 0; c2 < 3; ++c2) {
                    for (int c3 = 0; c3 < 3; ++c3) {
                        int code = c0 + 3 * (c1 + 3 * (c2 + 3 * c3));
                        int m = c0 + c1 + c2 + c3;
                        int px = c0 * v[q[0]].x + c1 * v[q[1]].x + c2 * v[q[2]].x + c3 * v[q[3]].x;
                        int py = c0 * v[q[0]].y + c1 * v[q[1]].y + c2 * v[q[2]].y + c3 * v[q[3]].y;
                        int pz = c0 * v[q[0]].z + c1 * v[q[1]].z + c2 * v[q[2]].z + c3 * v[q[3]].z;
                        int e  = c0 * v[q[0]].e + c1 * v[q[1]].e + c2 * v[q[2]].e + c3 * v[q[3]].e;
                        classes[{m, px, py, pz, e}].push_back(code);
                    }
                }
            }
        }
        for (int i = 0; i < 81; ++i) g.lut[i] = static_cast<u8>(i);
        for (const auto& kv_cls : classes) {
            const auto& members = kv_cls.second;
            if (members.size() >= 2) {
                for (size_t i = 0; i + 1 < members.size(); i += 2) {
                    g.lut[members[i]]     = static_cast<u8>(members[i + 1]);
                    g.lut[members[i + 1]] = static_cast<u8>(members[i]);
                }
            }
        }
        gates.push_back(g);
    }
    return gates;
}

static void step_lattice_arm3(vector<u8>& state, vector<u8>& scratch,
                              const vector<Arm3Gate>& gates,
                              const array<asolaria::Vel, K>& v, int L, int t) {
    int num_cells = L * L * L;
    size_t total_c = gates.size();
    constexpr int gates_per_cell = 32;
    for (int cell = 0; cell < num_cells; ++cell) {
        int base = cell * K;
        for (int j = 0; j < gates_per_cell; ++j) {
            size_t which = (static_cast<size_t>(t) * gates_per_cell +
                            static_cast<size_t>(cell) * 17 +
                            static_cast<size_t>(j) * 37) % total_c;
            const auto& gate = gates[which];
            int in_code = state[base + gate.a] + 3 * (state[base + gate.b] + 3 * (state[base + gate.c] + 3 * state[base + gate.d]));
            int out_code = gate.lut[in_code];
            state[base + gate.a] = static_cast<u8>(out_code % 3); out_code /= 3;
            state[base + gate.b] = static_cast<u8>(out_code % 3); out_code /= 3;
            state[base + gate.c] = static_cast<u8>(out_code % 3); out_code /= 3;
            state[base + gate.d] = static_cast<u8>(out_code % 3);
        }
    }
    for (int x = 0; x < L; ++x) {
        for (int y = 0; y < L; ++y) {
            for (int z = 0; z < L; ++z) {
                int src_cell = cell_index(x, y, z, L);
                for (int ki = 0; ki < K; ++ki) {
                    int dst_x = (x + v[ki].x + L) % L;
                    int dst_y = (y + v[ki].y + L) % L;
                    int dst_z = (z + v[ki].z + L) % L;
                    int dst_cell = cell_index(dst_x, dst_y, dst_z, L);
                    scratch[dst_cell * K + ki] = state[src_cell * K + ki];
                }
            }
        }
    }
    state = scratch;
}

static void step_lattice_arm5(vector<u8>& state, vector<u8>& scratch,
                              const vector<asolaria::Gate>& edge_gates,
                              const asolaria::InterShellData& d,
                              const array<asolaria::Vel, K>& v, int L) {
    int num_cells = L * L * L;
    for (int c = 0; c < num_cells; ++c) {
        asolaria::apply_full_symmetric_collision(&state[c * K], edge_gates, d);
    }
    for (int x = 0; x < L; ++x) {
        for (int y = 0; y < L; ++y) {
            for (int z = 0; z < L; ++z) {
                int src_cell = cell_index(x, y, z, L);
                for (int ki = 0; ki < K; ++ki) {
                    int dst_x = (x + v[ki].x + L) % L;
                    int dst_y = (y + v[ki].y + L) % L;
                    int dst_z = (z + v[ki].z + L) % L;
                    int dst_cell = cell_index(dst_x, dst_y, dst_z, L);
                    scratch[dst_cell * K + ki] = state[src_cell * K + ki];
                }
            }
        }
    }
    state = scratch;
}

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

static double compute_mode_amplitude(const vector<u8>& state, int L, int kx, int ky, int kz) {
    const double pi = std::acos(-1.0);
    double amp = 0.0;
    int num_cells = L * L * L;
    for (int x = 0; x < L; ++x) {
        for (int y = 0; y < L; ++y) {
            for (int z = 0; z < L; ++z) {
                int cell = cell_index(x, y, z, L);
                int rho = 0;
                for (int ki = 0; ki < K; ++ki) rho += state[cell * K + ki];
                double phase = 2.0 * pi * (kx * x + ky * y + kz * z) / static_cast<double>(L);
                amp += static_cast<double>(rho) * std::cos(phase);
            }
        }
    }
    return amp / static_cast<double>(num_cells);
}

struct ResponseResult {
    string kernel;
    string sector;
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

static ResponseResult run_response_probe(
    const string& kernel_name,
    const string& sector_name,
    const vector<u8>& base_state,
    int L, int kx, int ky, int kz, const string& mode_name,
    bool use_arm5,
    const vector<asolaria::Gate>& arm5_edge_gates,
    const asolaria::InterShellData& arm5_d,
    const vector<Arm3Gate>& arm3_gates,
    const array<asolaria::Vel, K>& v,
    int total_ticks,
    std::ofstream& ts_csv)
{
    const double pi = std::acos(-1.0);
    int num_cells = L * L * L;
    double k_mag = std::sqrt(static_cast<double>(kx * kx + ky * ky + kz * kz)) * (2.0 * pi / L);

    // 1. Control Run (unperturbed)
    vector<u8> ctrl_state = base_state;
    vector<u8> scratch(num_cells * K, 0);
    vector<double> ctrl_amp(total_ticks + 1, 0.0);

    for (int t = 0; t <= total_ticks; ++t) {
        ctrl_amp[t] = compute_mode_amplitude(ctrl_state, L, kx, ky, kz);
        if (t < total_ticks) {
            if (use_arm5) step_lattice_arm5(ctrl_state, scratch, arm5_edge_gates, arm5_d, v, L);
            else          step_lattice_arm3(ctrl_state, scratch, arm3_gates, v, L, t);
        }
    }

    // 2. Perturbed Run: Apply EXACT mass-conserving conjugate antinode perturbation
    // Transfers 1 trit from trough to crest on the rest channel (ki = 13, v = (0,0,0))
    // Guarantees machine-exact: Delta M == 0, Delta P == (0,0,0), Delta E == 0.
    vector<u8> pert_state = base_state;
    const int rest_idx = 13;
    int dx = (kx != 0) ? (L / (2 * kx)) : 0;
    int dy = (ky != 0) ? (L / (2 * ky)) : 0;
    int dz = 0;
    // For Oblique (2,2,1) on L=24: k*d = (2*0 + 2*0 + 1*12) * 2pi/24 = pi -> d = (0,0,12)
    if (kx == 2 && ky == 2 && kz == 1) {
        dx = 0; dy = 0; dz = 12;
    }

    int transfers = 0;
    for (int x = 0; x < L; ++x) {
        for (int y = 0; y < L; ++y) {
            for (int z = 0; z < L; ++z) {
                int cell = cell_index(x, y, z, L);
                double phase = 2.0 * pi * (kx * x + ky * y + kz * z) / static_cast<double>(L);
                double c = std::cos(phase);
                if (c > 0.3) {
                    int x2 = (x + dx + L) % L;
                    int y2 = (y + dy + L) % L;
                    int z2 = (z + dz + L) % L;
                    int cell2 = cell_index(x2, y2, z2, L);
                    u8& v1 = pert_state[cell * K + rest_idx];
                    u8& v2 = pert_state[cell2 * K + rest_idx];
                    if (v1 < 2 && v2 > 0) {
                        v1 += 1;
                        v2 -= 1;
                        transfers++;
                    }
                }
            }
        }
    }
    assert(transfers > 0);

    vector<double> diff_amp(total_ticks + 1, 0.0);
    vector<double> times(total_ticks + 1, 0.0);
    for (int t = 0; t <= total_ticks; ++t) {
        times[t] = static_cast<double>(t);
        double pert_a = compute_mode_amplitude(pert_state, L, kx, ky, kz);
        diff_amp[t] = pert_a - ctrl_amp[t];
        ts_csv << kernel_name << "," << sector_name << "," << L << "," << mode_name << "," << t << ","
               << fixed << setprecision(6) << diff_amp[t] << "," << pert_a << "," << ctrl_amp[t] << "\n";
        if (t < total_ticks) {
            if (use_arm5) step_lattice_arm5(pert_state, scratch, arm5_edge_gates, arm5_d, v, L);
            else          step_lattice_arm3(pert_state, scratch, arm3_gates, v, L, t);
        }
    }

    // 3. QR Variable-Projection Damped Harmonic Fit
    const auto fit = acoustic::fit(times, diff_amp);

    bool at_boundary = (std::abs(fit.omega - pi / (2.0 * total_ticks)) < 1e-4 ||
                        std::abs(fit.omega - 0.99 * pi) < 1e-4 ||
                        std::abs(fit.alpha - 20.0 / total_ticks) < 1e-4);
    double cycles = fit.cycles;
    bool pass_qualification = (fit.valid && cycles >= 3.0 && fit.r_squared >= 0.70 && !at_boundary);
    double cs = pass_qualification ? (fit.omega / k_mag) : 0.0;

    return ResponseResult{
        kernel_name, sector_name, L, mode_name,
        kx, ky, kz, k_mag, fit.omega, cs, fit.alpha,
        fit.r_squared, cycles, pass_qualification
    };
}

int main(int argc, char* argv[]) {
    auto v = asolaria::make_vels();
    auto arm3_gates = make_arm3_gates(v);
    auto arm5_edge_gates = asolaria::make_edge_gates(v);
    auto arm5_d = asolaria::init_intershell_data(v);

    std::cout << "=======================================================================\n";
    std::cout << " ASOLARIA HOST8-CONDITIONED LINEAR RESPONSE BENCHMARK\n";
    std::cout << " Directly Probes Differential Acoustic Response on Authenticated Microstates\n";
    std::cout << " Standard Arm 3 (288-gate space-time mixing) vs Equivariant Arm 5\n";
    std::cout << " Delta M == 0 Machine-Exact Conserved Conjugate-Antinode Transfer\n";
    std::cout << " Delta A(t) = A_pert(t) - A_ctrl(t) [Zero Thermal Noise Floor]\n";
    std::cout << "=======================================================================\n";

    string host8_dir = "D:/ASOLARIA-HOST8-ROOMS-300G";
    bool host8_required = false;
    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];
        if (arg == "--host8-required") {
            host8_required = true;
        } else if (arg == "--host8-dir" && i + 1 < argc) {
            host8_dir = argv[++i];
        }
    }

    bool host8_available = std::ifstream(host8_dir + "/sector-000.rooms").is_open();
    if (!host8_available && std::ifstream("data/host8_samples/sector-000.rooms").is_open()) {
        host8_dir = "data/host8_samples";
        host8_available = true;
        std::cout << "Using authenticated sample slice directory: " << host8_dir << "\n";
    }

    if (!host8_available) {
        if (host8_required) {
            std::cerr << "[FAIL] Host8 volume required but unavailable!\n";
            return 2;
        }
        std::cout << "[SKIP] Host8 volume not available.\n";
        return 0;
    }

    const map<string, string> auth_hashes_l24 = {
        {"000", "2dfe7d757ecb012475806b86fcdefc602680d042db836ffbcb2bfe924f2a18ee"},
        {"187", "6752ab9e2603cc5042f7c7a41c3ddc48ae5571fd0aeb945d0a3f63b1a948a652"},
        {"374", "963805be806e8a290aab8dee48b0d214d03015c84e02bf844f0de87e2b37c10a"}
    };

    std::ofstream res_csv("host8_linear_response_results.csv");
    res_csv << "kernel,sector,L,name,kx,ky,kz,k_mag,fitted_omega,measured_cs,damping_alpha,r_squared,cycles,three_cycle_pass\n";

    std::ofstream ts_csv("host8_linear_response_timeseries.csv");
    ts_csv << "kernel,sector,L,name,tick,diff_amplitude,pert_amplitude,ctrl_amplitude\n";

    vector<ResponseResult> all_results;
    int L = 24;
    int V = L * L * L;
    int ticks = 48;

    for (const string sec : {"000", "187", "374"}) {
        string path = host8_dir + "/sector-" + sec + ".rooms";
        vector<u8> state;
        string block_sha256;
        bool ok = load_host8_rooms(path, 0, V, state, block_sha256);
        assert(ok);
        string expected = auth_hashes_l24.at(sec);
        bool match = (block_sha256 == expected);
        std::cout << "\n>>> Sector " << sec << " (L=" << L << "^3): SHA-256 = " << block_sha256.substr(0, 16) << "... "
                  << (match ? "[AUTHENTICATED]" : "[MISMATCH]") << " <<<\n";
        assert(match);

        // 1. Arm 3 Axis & Oblique (288-gate space-time scheduler)
        std::cout << "--- Standard Arm 3 (288-gate space-time mixer) ---\n";
        auto a3_ax = run_response_probe("Standard Arm 3", sec, state, L, 3, 0, 0, "Axis_3_0_0",
                                        false, arm5_edge_gates, arm5_d, arm3_gates, v, ticks, ts_csv);
        auto a3_ob = run_response_probe("Standard Arm 3", sec, state, L, 2, 2, 1, "Oblique_2_2_1",
                                        false, arm5_edge_gates, arm5_d, arm3_gates, v, ticks, ts_csv);
        all_results.push_back(a3_ax);
        all_results.push_back(a3_ob);
        std::cout << "  - Axis (3,0,0)   : cs=" << fixed << setprecision(5) << a3_ax.cs
                  << " | omega=" << a3_ax.omega << " | alpha=" << a3_ax.alpha
                  << " | R^2=" << a3_ax.r_squared << " | cycles=" << a3_ax.cycles
                  << " | Qualified=" << (a3_ax.three_cycle_pass ? "YES" : "NO (Non-Acoustic)") << "\n";
        std::cout << "  - Oblique (2,2,1): cs=" << fixed << setprecision(5) << a3_ob.cs
                  << " | omega=" << a3_ob.omega << " | alpha=" << a3_ob.alpha
                  << " | R^2=" << a3_ob.r_squared << " | cycles=" << a3_ob.cycles
                  << " | Qualified=" << (a3_ob.three_cycle_pass ? "YES" : "NO (Non-Acoustic)") << "\n";

        // 2. Arm 5 Axis & Oblique (Oh-Equivariant Inter-Shell)
        std::cout << "--- Equivariant Arm 5 (Oh Inter-Shell) ---\n";
        auto a5_ax = run_response_probe("Equivariant Arm 5", sec, state, L, 3, 0, 0, "Axis_3_0_0",
                                        true, arm5_edge_gates, arm5_d, arm3_gates, v, ticks, ts_csv);
        auto a5_ob = run_response_probe("Equivariant Arm 5", sec, state, L, 2, 2, 1, "Oblique_2_2_1",
                                        true, arm5_edge_gates, arm5_d, arm3_gates, v, ticks, ts_csv);
        all_results.push_back(a5_ax);
        all_results.push_back(a5_ob);
        std::cout << "  - Axis (3,0,0)   : cs=" << fixed << setprecision(5) << a5_ax.cs
                  << " | omega=" << a5_ax.omega << " | alpha=" << a5_ax.alpha
                  << " | R^2=" << a5_ax.r_squared << " | cycles=" << a5_ax.cycles
                  << " | Qualified=" << (a5_ax.three_cycle_pass ? "YES" : "NO (Non-Acoustic)") << "\n";
        std::cout << "  - Oblique (2,2,1): cs=" << fixed << setprecision(5) << a5_ob.cs
                  << " | omega=" << a5_ob.omega << " | alpha=" << a5_ob.alpha
                  << " | R^2=" << a5_ob.r_squared << " | cycles=" << a5_ob.cycles
                  << " | Qualified=" << (a5_ob.three_cycle_pass ? "YES" : "NO (Non-Acoustic)") << "\n";
    }

    for (const auto& r : all_results) {
        res_csv << r.kernel << "," << r.sector << "," << r.L << "," << r.name << ","
                << r.kx << "," << r.ky << "," << r.kz << ","
                << r.k_mag << "," << r.omega << "," << r.cs << ","
                << r.alpha << "," << r.r_squared << "," << r.cycles << ","
                << (r.three_cycle_pass ? "1" : "0") << "\n";
    }
    res_csv.close();
    ts_csv.close();

    std::cout << "\n=======================================================================\n";
    std::cout << " Host8 Linear Response Results Exported:\n";
    std::cout << "  - host8_linear_response_results.csv\n";
    std::cout << "  - host8_linear_response_timeseries.csv\n";
    std::cout << "=======================================================================\n";
    return 0;
}
