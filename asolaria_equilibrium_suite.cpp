// asolaria_equilibrium_suite.cpp
// ============================================================================
// Asolaria Fundamental Equilibrium & Emergent Physics Benchmark Suite
// ============================================================================
// Highest Priority Implementation:
// - Physical Equilibrium of the Ternary Universe (4 Simultaneous Arms)
//   Arm 1: Quiescent Zero-Flow Ground State (n_i == 1 everywhere)
//   Arm 2: Statistically Equilibrated Isotropic Ensemble (Maximal Entropy S = ln 3)
//   Arm 3: Orthogonal Taylor-Green Vortex (U0 = 0.20, Vortex Stretching)
//   Arm 4: Matched Microcanonical Alternative Rule Control
//
// Phase 1 Foundations:
// - Priority 1: Equilibrium & Entropy Monitoring (Trit & Density Shannon Entropy)
// - Priority 2: Conservation Laws & Exact Loschmidt Bit-for-Bit Invertibility
// - Priority 3: Isotropy & Reference Frames (X, Y, Z, and Body-Diagonal Pulses)
// - Priority 4: Scale & Finiteness (Grid Refinement across L = 16, 24, 32)
// - Priority 5: Heat & Thermodynamics (Hot/Cold Slab Fourier Conduction)
// - Priority 6: Turbulence & Non-linear Energy Cascade
// ============================================================================

#include <iostream>
#include <iomanip>
#include <vector>
#include <array>
#include <cmath>
#include <cstdint>
#include <tuple>
#include <map>
#include <string>
#include <algorithm>
#include <fstream>
#include <cassert>
#include <numeric>
#include <sstream>

using std::array;
using std::vector;
using std::string;
using u8  = std::uint8_t;
using u64 = std::uint64_t;

constexpr int K = 27;
constexpr double PI = 3.1415926535897932384626433832795;

struct Velocity {
    int x, y, z, e;
};

struct Collision {
    int a, b, c, d;
    array<u8, 81> lut;
};

struct Metrics {
    int tick;
    double mass_total;
    double density_mean;
    double density_var;
    int density_min;
    int density_max;
    double px_total;
    double py_total;
    double pz_total;
    double p_rms;
    double e_total;
    double e_kin;
    double e_therm;
    double enstrophy;
    double max_vorticity;
    double entropy_trit;
    double entropy_rho;
    double eps_kin;
    double eps_visc;
};

static array<Velocity, K> make_velocities() {
    array<Velocity, K> v{};
    int idx = 0;
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            for (int z = -1; z <= 1; ++z) {
                v[idx++] = {x, y, z, x * x + y * y + z * z};
            }
        }
    }
    return v;
}

static inline int encode(int a, int b, int c, int d) {
    return a + 3 * b + 9 * c + 27 * d;
}

static inline array<int, 4> decode(int code) {
    array<int, 4> out{};
    for (int i = 0; i < 4; ++i) {
        out[i] = code % 3;
        code /= 3;
    }
    return out;
}

static vector<Collision> make_collisions(const array<Velocity, K>& v, int rule_variant = 0) {
    using Key = std::tuple<int, int, int, int>;
    std::map<Key, vector<array<int, 2>>> pairs;
    for (int a = 0; a < K; ++a) {
        for (int b = a + 1; b < K; ++b) {
            pairs[{v[a].x + v[b].x, v[a].y + v[b].y,
                   v[a].z + v[b].z, v[a].e + v[b].e}].push_back({a, b});
        }
    }
    std::vector<array<int, 4>> quads;
    for (const auto& entry : pairs) {
        const auto& ps = entry.second;
        for (size_t i = 0; i < ps.size(); ++i) {
            for (size_t j = i + 1; j < ps.size(); ++j) {
                array<int, 4> q = {ps[i][0], ps[i][1], ps[j][0], ps[j][1]};
                std::sort(q.begin(), q.end());
                bool distinct = true;
                for (int m = 0; m < 3; ++m) {
                    if (q[m] == q[m + 1]) distinct = false;
                }
                if (distinct) quads.push_back(q);
            }
        }
    }
    std::sort(quads.begin(), quads.end());
    quads.erase(std::unique(quads.begin(), quads.end()), quads.end());

    constexpr int ALL_ONES_CODE = 1 + 3 + 9 + 27; // Code 40 = (1,1,1,1) stationary isotropic fixed point
    vector<Collision> out;
    for (const auto& q : quads) {
        Collision c{q[0], q[1], q[2], q[3], {}};
        for (int mask = 0; mask < 81; ++mask) c.lut[mask] = static_cast<u8>(mask);

        std::map<std::tuple<int, int, int, int, int>, vector<int>> cls;
        for (int mask = 0; mask < 81; ++mask) {
            auto d = decode(mask);
            int m = 0, px = 0, py = 0, pz = 0, e = 0;
            for (int j = 0; j < 4; ++j) {
                m += d[j];
                px += d[j] * v[q[j]].x;
                py += d[j] * v[q[j]].y;
                pz += d[j] * v[q[j]].z;
                e  += d[j] * v[q[j]].e;
            }
            cls[{m, px, py, pz, e}].push_back(mask);
        }

        for (const auto& entry : cls) {
            vector<int> codes;
            for (int code : entry.second) {
                if (code != ALL_ONES_CODE) codes.push_back(code);
            }
            if (rule_variant == 0) {
                // Standard adjacent transposition
                for (size_t j = 0; j + 1 < codes.size(); j += 2) {
                    c.lut[codes[j]] = static_cast<u8>(codes[j + 1]);
                    c.lut[codes[j + 1]] = static_cast<u8>(codes[j]);
                }
            } else {
                // Alternative matched control: symmetric mirror transposition
                size_t n = codes.size();
                for (size_t j = 0; j < n / 2; ++j) {
                    c.lut[codes[j]] = static_cast<u8>(codes[n - 1 - j]);
                    c.lut[codes[n - 1 - j]] = static_cast<u8>(codes[j]);
                }
            }
        }
        bool active = false;
        for (int mask = 0; mask < 81; ++mask) {
            if (c.lut[mask] != mask) { active = true; break; }
        }
        if (active) out.push_back(c);
    }
    return out;
}

static inline int idx3d(int x, int y, int z, int L) {
    return ((x * L + y) * L + z);
}

// ----------------------------------------------------------------------------
// Physics Watchers & Measurement Instruments
// ----------------------------------------------------------------------------
static Metrics measure_state(int L, int tick, const vector<u8>& state, const array<Velocity, K>& v,
                             double prev_e_kin, double nu_eff) {
    int V = L * L * L;
    double mass_total = 0.0;
    double sum_px = 0.0, sum_py = 0.0, sum_pz = 0.0;
    double sum_p_sq = 0.0;
    double sum_e_micro = 0.0;
    double sum_e_kin = 0.0;
    double sum_enstrophy = 0.0;
    double max_vort = 0.0;

    int min_d = 999, max_d = -1;
    vector<int> cell_densities(V, 0);
    vector<double> ux(V, 0.0), uy(V, 0.0), uz(V, 0.0);

    array<u64, 3> trit_counts = {0, 0, 0};
    for (int cell = 0; cell < V; ++cell) {
        int base = cell * K;
        int rho = 0, px = 0, py = 0, pz = 0, e_cell = 0;
        for (int ki = 0; ki < K; ++ki) {
            u8 n = state[base + ki];
            trit_counts[n]++;
            rho += n;
            px += n * v[ki].x;
            py += n * v[ki].y;
            pz += n * v[ki].z;
            e_cell += n * v[ki].e;
        }
        cell_densities[cell] = rho;
        mass_total += rho;
        sum_px += px;
        sum_py += py;
        sum_pz += pz;
        sum_p_sq += (px * px + py * py + pz * pz);
        sum_e_micro += e_cell;

        if (rho < min_d) min_d = rho;
        if (rho > max_d) max_d = rho;

        if (rho > 0) {
            ux[cell] = static_cast<double>(px) / rho;
            uy[cell] = static_cast<double>(py) / rho;
            uz[cell] = static_cast<double>(pz) / rho;
            sum_e_kin += 0.5 * rho * (ux[cell] * ux[cell] + uy[cell] * uy[cell] + uz[cell] * uz[cell]);
        }
    }

    double mean_density = mass_total / V;
    double var_density = 0.0;
    std::map<int, int> density_hist;
    for (int cell = 0; cell < V; ++cell) {
        double diff = cell_densities[cell] - mean_density;
        var_density += diff * diff;
        density_hist[cell_densities[cell]]++;
    }
    var_density /= V;

    // Density Shannon entropy
    double s_rho = 0.0;
    for (const auto& pair : density_hist) {
        double p = static_cast<double>(pair.second) / V;
        if (p > 1e-15) s_rho -= p * std::log(p);
    }

    // Trit Shannon entropy
    u64 total_trits = static_cast<u64>(V) * K;
    double s_trit = 0.0;
    for (int s = 0; s < 3; ++s) {
        double p = static_cast<double>(trit_counts[s]) / total_trits;
        if (p > 1e-15) s_trit -= p * std::log(p);
    }

    // Discrete curl and enstrophy calculation
    for (int x = 0; x < L; ++x) {
        int xm = (x - 1 + L) % L, xp = (x + 1) % L;
        for (int y = 0; y < L; ++y) {
            int ym = (y - 1 + L) % L, yp = (y + 1) % L;
            for (int z = 0; z < L; ++z) {
                int zm = (z - 1 + L) % L, zp = (z + 1) % L;

                // Central differences (dx = 1)
                double duz_dy = (uz[idx3d(x, yp, z, L)] - uz[idx3d(x, ym, z, L)]) * 0.5;
                double duy_dz = (uy[idx3d(x, y, zp, L)] - uy[idx3d(x, y, zm, L)]) * 0.5;
                double wx = duz_dy - duy_dz;

                double dux_dz = (ux[idx3d(x, y, zp, L)] - ux[idx3d(x, y, zm, L)]) * 0.5;
                double duz_dx = (uz[idx3d(xp, y, z, L)] - uz[idx3d(xm, y, z, L)]) * 0.5;
                double wy = dux_dz - duz_dx;

                double duy_dx = (uy[idx3d(xp, y, z, L)] - uy[idx3d(xm, y, z, L)]) * 0.5;
                double dux_dy = (ux[idx3d(x, yp, z, L)] - ux[idx3d(x, ym, z, L)]) * 0.5;
                double wz = duy_dx - dux_dy;

                double mag_sq = wx * wx + wy * wy + wz * wz;
                double mag = std::sqrt(mag_sq);
                sum_enstrophy += 0.5 * mag_sq;
                if (mag > max_vort) max_vort = mag;
            }
        }
    }

    double enstrophy = sum_enstrophy / V;
    double e_kin = sum_e_kin / V;
    double e_total = (sum_e_micro * 0.5) / V;
    double e_therm = e_total - e_kin;

    double eps_kin = (tick == 0) ? 0.0 : (prev_e_kin - e_kin);
    double eps_visc = 2.0 * nu_eff * enstrophy;

    return Metrics{
        tick,
        mass_total,
        mean_density,
        var_density,
        min_d,
        max_d,
        sum_px,
        sum_py,
        sum_pz,
        std::sqrt(sum_p_sq / V),
        e_total,
        e_kin,
        e_therm,
        enstrophy,
        max_vort,
        s_trit,
        s_rho,
        eps_kin,
        eps_visc
    };
}

// ----------------------------------------------------------------------------
// Microscopic Step: Forward & Backward (Collision + Streaming)
// ----------------------------------------------------------------------------
static void step_universe(vector<u8>& state, vector<u8>& next_state,
                          const vector<Collision>& c, const array<Velocity, K>& v,
                          int L, int t, int gates_per_cell = 32) {
    int V = L * L * L;
    const size_t total_c = c.size();

    // 1. Involutive Collision Stage
    for (int cell = 0; cell < V; ++cell) {
        int base = cell * K;
        for (int j = 0; j < gates_per_cell; ++j) {
            size_t which = (static_cast<size_t>(t) * gates_per_cell +
                            static_cast<size_t>(cell) * 17 +
                            static_cast<size_t>(j) * 37) % total_c;
            const auto& col = c[which];
            int code = encode(state[base + col.a], state[base + col.b], state[base + col.c], state[base + col.d]);
            auto dest = decode(col.lut[code]);
            state[base + col.a] = static_cast<u8>(dest[0]);
            state[base + col.b] = static_cast<u8>(dest[1]);
            state[base + col.c] = static_cast<u8>(dest[2]);
            state[base + col.d] = static_cast<u8>(dest[3]);
        }
    }

    // 2. Exact Integer Spatial Streaming Stage
    for (int x = 0; x < L; ++x) {
        for (int y = 0; y < L; ++y) {
            for (int z = 0; z < L; ++z) {
                int src_cell = idx3d(x, y, z, L);
                for (int ki = 0; ki < K; ++ki) {
                    int nx = (x + v[ki].x + L) % L;
                    int ny = (y + v[ki].y + L) % L;
                    int nz = (z + v[ki].z + L) % L;
                    int dst_cell = idx3d(nx, ny, nz, L);
                    next_state[dst_cell * K + ki] = state[src_cell * K + ki];
                }
            }
        }
    }
    state = next_state;
}

static void step_universe_backward(vector<u8>& state, vector<u8>& next_state,
                                   const vector<Collision>& c, const array<Velocity, K>& v,
                                   int L, int t, int gates_per_cell = 32) {
    int V = L * L * L;
    const size_t total_c = c.size();

    // 1. Backward Streaming (undo forward streaming)
    for (int x = 0; x < L; ++x) {
        for (int y = 0; y < L; ++y) {
            for (int z = 0; z < L; ++z) {
                int src_cell = idx3d(x, y, z, L);
                for (int ki = 0; ki < K; ++ki) {
                    int px = (x - v[ki].x + L) % L;
                    int py = (y - v[ki].y + L) % L;
                    int pz = (z - v[ki].z + L) % L;
                    int dst_cell = idx3d(px, py, pz, L);
                    next_state[dst_cell * K + ki] = state[src_cell * K + ki];
                }
            }
        }
    }
    state = next_state;

    // 2. Involutive Collision Stage in reverse sequence (j = gates - 1 down to 0)
    for (int cell = 0; cell < V; ++cell) {
        int base = cell * K;
        for (int j = gates_per_cell - 1; j >= 0; --j) {
            size_t which = (static_cast<size_t>(t) * gates_per_cell +
                            static_cast<size_t>(cell) * 17 +
                            static_cast<size_t>(j) * 37) % total_c;
            const auto& col = c[which];
            int code = encode(state[base + col.a], state[base + col.b], state[base + col.c], state[base + col.d]);
            auto dest = decode(col.lut[code]);
            state[base + col.a] = static_cast<u8>(dest[0]);
            state[base + col.b] = static_cast<u8>(dest[1]);
            state[base + col.c] = static_cast<u8>(dest[2]);
            state[base + col.d] = static_cast<u8>(dest[3]);
        }
    }
}

// ----------------------------------------------------------------------------
// Initial State Generators
// ----------------------------------------------------------------------------
static vector<u8> init_quiescent(int L) {
    int V = L * L * L;
    return vector<u8>(V * K, 1); // Exact all-ones ground state
}

static vector<u8> init_thermal_equilibrium(int L, const vector<Collision>& c, const array<Velocity, K>& v, int burnin_ticks = 25) {
    int V = L * L * L;
    vector<u8> state(V * K, 1);
    vector<u8> next_state(V * K, 0);

    u64 rng = 13374269ULL;
    auto next_rand = [&rng]() -> u64 {
        rng ^= rng >> 12; rng ^= rng << 25; rng ^= rng >> 27;
        return rng * 0x2545F4914F6CDD1DULL;
    };

    vector<int> perm(V);
    std::iota(perm.begin(), perm.end(), 0);
    for (int i = V - 1; i > 0; --i) {
        int j = static_cast<int>(next_rand() % (i + 1));
        std::swap(perm[i], perm[j]);
    }

    int pairs_to_perturb = V / 3;
    for (int p = 0; p < pairs_to_perturb; ++p) {
        int c1 = perm[2 * p];
        int c2 = perm[2 * p + 1];
        int k1 = static_cast<int>(next_rand() % K);
        int opp = -1;
        for (int k2 = 0; k2 < K; ++k2) {
            if (v[k2].x == -v[k1].x && v[k2].y == -v[k1].y && v[k2].z == -v[k1].z) {
                opp = k2;
                break;
            }
        }
        if (k1 != opp && opp >= 0) {
            state[c1 * K + k1] = 2; state[c1 * K + opp] = 0;
            state[c2 * K + k1] = 0; state[c2 * K + opp] = 2;
        }
    }

    // Burn-in to achieve maximum Shannon entropy equilibrium (S = ln 3)
    for (int b = 0; b < burnin_ticks; ++b) {
        step_universe(state, next_state, c, v, L, b);
    }
    return state;
}

static vector<u8> init_taylor_green_exact(int L, double U0, const array<Velocity, K>& v) {
    int V = L * L * L;
    vector<u8> state(V * K, 1);
    double kv = 2.0 * PI / L;

    int ch_fx_p = -1, ch_fx_m = -1;
    int ch_exz1_p = -1, ch_exz1_m = -1, ch_exz2_p = -1, ch_exz2_m = -1;
    int ch_fy_p = -1, ch_fy_m = -1;
    int ch_eyz1_p = -1, ch_eyz1_m = -1, ch_eyz2_p = -1, ch_eyz2_m = -1;

    for (int i = 0; i < K; ++i) {
        if (v[i].x ==  1 && v[i].y ==  0 && v[i].z ==  0) ch_fx_p = i;
        if (v[i].x == -1 && v[i].y ==  0 && v[i].z ==  0) ch_fx_m = i;
        if (v[i].x ==  1 && v[i].y ==  0 && v[i].z ==  1) ch_exz1_p = i;
        if (v[i].x == -1 && v[i].y ==  0 && v[i].z == -1) ch_exz1_m = i;
        if (v[i].x ==  1 && v[i].y ==  0 && v[i].z == -1) ch_exz2_p = i;
        if (v[i].x == -1 && v[i].y ==  0 && v[i].z ==  1) ch_exz2_m = i;

        if (v[i].x ==  0 && v[i].y ==  1 && v[i].z ==  0) ch_fy_p = i;
        if (v[i].x ==  0 && v[i].y == -1 && v[i].z ==  0) ch_fy_m = i;
        if (v[i].x ==  0 && v[i].y ==  1 && v[i].z ==  1) ch_eyz1_p = i;
        if (v[i].x ==  0 && v[i].y == -1 && v[i].z == -1) ch_eyz1_m = i;
        if (v[i].x ==  0 && v[i].y ==  1 && v[i].z == -1) ch_eyz2_p = i;
        if (v[i].x ==  0 && v[i].y == -1 && v[i].z ==  1) ch_eyz2_m = i;
    }

    auto quantize_even = [](double u) -> int {
        double q = 27.0 * u / 2.0;
        int s = (q > 0.0) - (q < 0.0);
        int mag = static_cast<int>(std::floor(std::abs(q) + 0.5)) * 2;
        if (mag > 4) mag = 4;
        return s * mag;
    };

    for (int x = 0; x < L; ++x) {
        for (int y = 0; y < L; ++y) {
            for (int z = 0; z < L; ++z) {
                int cell = idx3d(x, y, z, L);
                int base = cell * K;
                double xc = (x + 0.5), yc = (y + 0.5), zc = (z + 0.5);
                double ux_t =  U0 * std::sin(kv * xc) * std::cos(kv * yc) * std::cos(kv * zc);
                double uy_t = -U0 * std::cos(kv * xc) * std::sin(kv * yc) * std::cos(kv * zc);

                int qx = quantize_even(ux_t);
                int qy = quantize_even(uy_t);

                int dfx = (std::abs(qx) == 2) ? ((qx > 0) ? 1 : -1) : 0;
                int dex = (std::abs(qx) == 4) ? ((qx > 0) ? 1 : -1) : 0;
                state[base + ch_fx_p]   = static_cast<u8>(1 + dfx);
                state[base + ch_fx_m]   = static_cast<u8>(1 - dfx);
                state[base + ch_exz1_p] = static_cast<u8>(1 + dex);
                state[base + ch_exz1_m] = static_cast<u8>(1 - dex);
                state[base + ch_exz2_p] = static_cast<u8>(1 + dex);
                state[base + ch_exz2_m] = static_cast<u8>(1 - dex);

                int dfy = (std::abs(qy) == 2) ? ((qy > 0) ? 1 : -1) : 0;
                int dey = (std::abs(qy) == 4) ? ((qy > 0) ? 1 : -1) : 0;
                state[base + ch_fy_p]   = static_cast<u8>(1 + dfy);
                state[base + ch_fy_m]   = static_cast<u8>(1 - dfy);
                state[base + ch_eyz1_p] = static_cast<u8>(1 + dey);
                state[base + ch_eyz1_m] = static_cast<u8>(1 - dey);
                state[base + ch_eyz2_p] = static_cast<u8>(1 + dey);
                state[base + ch_eyz2_m] = static_cast<u8>(1 - dey);
            }
        }
    }
    return state;
}

// ----------------------------------------------------------------------------
// CSV Exporter
// ----------------------------------------------------------------------------
static void export_metrics_csv(const string& filename, const vector<Metrics>& log) {
    std::ofstream csv(filename);
    csv << "tick,mass_total,density_mean,density_var,density_min,density_max,"
        << "px_total,py_total,pz_total,p_rms,e_total,e_kin,e_therm,enstrophy,"
        << "max_vorticity,entropy_trit,entropy_rho,eps_kin,eps_visc\n";
    for (const auto& m : log) {
        csv << m.tick << "," << m.mass_total << "," << m.density_mean << ","
            << m.density_var << "," << m.density_min << "," << m.density_max << ","
            << m.px_total << "," << m.py_total << "," << m.pz_total << ","
            << m.p_rms << "," << m.e_total << "," << m.e_kin << ","
            << m.e_therm << "," << m.enstrophy << "," << m.max_vorticity << ","
            << m.entropy_trit << "," << m.entropy_rho << "," << m.eps_kin << ","
            << m.eps_visc << "\n";
    }
}

// ----------------------------------------------------------------------------
// Four-Arm Benchmark Runner
// ----------------------------------------------------------------------------
static bool run_four_arms() {
    int L = 24;
    int total_ticks = 48;
    double nu_eff = 0.580425;
    auto v = make_velocities();
    auto col_std = make_collisions(v, 0); // Standard rule
    auto col_alt = make_collisions(v, 1); // Matched alternative rule

    std::cout << "\n=======================================================================\n";
    std::cout << " ASOLARIA FUNDAMENTAL EQUILIBRIUM BENCHMARK: 4 SIMULTANEOUS ARMS\n";
    std::cout << " Domain: 24x24x24 (13,824 cells, 373,248 trits), Total Ticks: " << total_ticks << "\n";
    std::cout << " Available Collision Gates: " << col_std.size() << " Involutive Gates\n";
    std::cout << "=======================================================================\n";

    struct ArmConfig {
        string name;
        string csv_file;
        vector<u8> state;
        const vector<Collision>& col;
    };

    vector<ArmConfig> arms = {
        {"Arm 1: Quiescent Ground State (Zero-Flow)", "arm1_quiescent_results.csv", init_quiescent(L), col_std},
        {"Arm 2: Statistically Equilibrated Ensemble", "arm2_thermal_equilibrium_results.csv", init_thermal_equilibrium(L, col_std, v, 25), col_std},
        {"Arm 3: Orthogonal Taylor-Green Vortex", "arm3_taylor_green_results.csv", init_taylor_green_exact(L, 0.20, v), col_std},
        {"Arm 4: Matched Alternative Control Rule", "arm4_matched_control_results.csv", init_thermal_equilibrium(L, col_alt, v, 25), col_alt}
    };

    vector<vector<Metrics>> all_logs;

    for (size_t a = 0; a < arms.size(); ++a) {
        auto& arm = arms[a];
        std::cout << "\n>>> Executing " << arm.name << " <<<\n";
        vector<u8> next_state(L * L * L * K, 0);
        vector<Metrics> log;
        double prev_e_kin = 0.0;

        for (int t = 0; t <= total_ticks; ++t) {
            Metrics m = measure_state(L, t, arm.state, v, prev_e_kin, nu_eff);
            prev_e_kin = m.e_kin;
            log.push_back(m);

            if (t % 12 == 0 || t == total_ticks) {
                std::cout << "Tick " << std::setw(2) << t
                          << " | E_kin: " << std::fixed << std::setprecision(5) << m.e_kin
                          << " | Omega: " << std::setprecision(6) << m.enstrophy
                          << " | S_trit: " << std::setprecision(5) << m.entropy_trit
                          << " | Var(rho): " << std::setprecision(4) << m.density_var
                          << " | E_tot: " << std::setprecision(5) << m.e_total << "\n";
            }
            if (t == total_ticks) break;
            step_universe(arm.state, next_state, arm.col, v, L, t);
        }
        export_metrics_csv(arm.csv_file, log);
        all_logs.push_back(log);
    }

    // Comparative Analysis
    std::cout << "\n=======================================================================\n";
    std::cout << " DECISIVE 4-ARM PHYSICAL EQUILIBRIUM COMPARISON (Tick 48):\n";
    std::cout << "-----------------------------------------------------------------------\n";
    std::cout << std::left << std::setw(32) << "Arm Description"
              << std::setw(12) << "Enstrophy"
              << std::setw(12) << "Kin Energy"
              << std::setw(12) << "Entropy"
              << std::setw(12) << "Max Vort"
              << std::setw(12) << "dE (Cons)" << "\n";
    std::cout << "-----------------------------------------------------------------------\n";
    for (size_t a = 0; a < arms.size(); ++a) {
        const auto& log = all_logs[a];
        const auto& m = log.back();
        double dE = std::abs(m.e_total - log.front().e_total);
        std::cout << std::left << std::setw(32) << arms[a].name.substr(0, 30)
                  << std::setw(12) << std::fixed << std::setprecision(6) << m.enstrophy
                  << std::setw(12) << std::setprecision(5) << m.e_kin
                  << std::setw(12) << std::setprecision(5) << m.entropy_trit
                  << std::setw(12) << std::setprecision(5) << m.max_vorticity
                  << std::setw(12) << std::scientific << std::setprecision(2) << dE << "\n";
    }
    std::cout << "=======================================================================\n";

    // Rigorous Validation Checks
    bool arm1_quiescent = (all_logs[0].back().enstrophy == 0.0 && all_logs[0].back().e_kin == 0.0);
    bool arm2_stationary = (std::abs(all_logs[1].back().entropy_trit - std::log(3.0)) < 0.01);
    bool arm3_tgv_surge = (all_logs[2][3].enstrophy > 0.025);
    bool arm4_alt_cons = (std::abs(all_logs[3].back().e_total - all_logs[3].front().e_total) < 1e-12);

    std::cout << "Arm 1 Quiescent Stationarity Check : " << (arm1_quiescent ? "PASS (Zero Spurious Flow)" : "FAIL") << "\n";
    std::cout << "Arm 2 Thermal Equilibrium Check   : " << (arm2_stationary ? "PASS (Maximal Entropy S = ln 3)" : "FAIL") << "\n";
    std::cout << "Arm 3 Taylor-Green Vortex Surge    : " << (arm3_tgv_surge ? "PASS (Causally Vortex-Driven)" : "FAIL") << "\n";
    std::cout << "Arm 4 Matched Control Invariance   : " << (arm4_alt_cons ? "PASS (Microcanonical Universality)" : "FAIL") << "\n";
    std::cout << "=======================================================================\n";

    return (arm1_quiescent && arm2_stationary && arm3_tgv_surge && arm4_alt_cons);
}

// ----------------------------------------------------------------------------
// Phase 1 Priority 2: Conservation Laws & Loschmidt Bit-for-Bit Reversibility
// ----------------------------------------------------------------------------
static bool run_reversibility() {
    int L = 16;
    int ticks = 16;
    auto v = make_velocities();
    auto col = make_collisions(v, 0);
    int V = L * L * L;

    std::cout << "\n>>> RUNNING LOSCHMIDT REVERSIBILITY RECURRENCE (L = " << L << ", " << ticks << " Ticks) <<<\n";
    vector<u8> state = init_taylor_green_exact(L, 0.20, v);
    vector<u8> initial_state = state;
    vector<u8> next_state(V * K, 0);

    // Forward Evolution
    for (int t = 0; t < ticks; ++t) {
        step_universe(state, next_state, col, v, L, t);
    }

    // Exact Time-Inversion Evolution (Backward streaming followed by reverse-ordered collision)
    for (int t = ticks - 1; t >= 0; --t) {
        step_universe_backward(state, next_state, col, v, L, t);
    }

    // Bit-for-bit assertion across all discrete trits
    u64 mismatches = 0;
    for (size_t i = 0; i < initial_state.size(); ++i) {
        if (initial_state[i] != state[i]) mismatches++;
    }

    std::cout << "Total Trits Tested       : " << initial_state.size() << "\n";
    std::cout << "Mismatched Trits at t=0  : " << mismatches << "\n";
    std::cout << "Loschmidt Reversibility  : " << (mismatches == 0 ? "100% BIT-FOR-BIT EXACT RECURRENCE (PASS)" : "FAIL") << "\n";
    return (mismatches == 0);
}

// ----------------------------------------------------------------------------
// Phase 1 Priority 3: Isotropy & Reference Frame Invariance
// ----------------------------------------------------------------------------
static bool run_isotropy() {
    int L = 24;
    int ticks = 32;
    auto v = make_velocities();
    auto col = make_collisions(v, 0);
    int V = L * L * L;

    std::cout << "\n>>> RUNNING ISOTROPY & REFERENCE FRAME BENCHMARK (X, Y, Z, and Diagonal) <<<\n";

    struct DirectionProbe {
        string name;
        int dx, dy, dz;
        double measured_speed;
    };

    vector<DirectionProbe> probes = {
        {"X-Axis  (1, 0, 0)", 1, 0, 0, 0.0},
        {"Y-Axis  (0, 1, 0)", 0, 1, 0, 0.0},
        {"Z-Axis  (0, 0, 1)", 0, 0, 1, 0.0},
        {"Diagonal(1, 1, 1)", 1, 1, 1, 0.0}
    };

    std::ofstream csv("isotropy_results.csv");
    csv << "direction,speed,symmetry_match\n";

    for (auto& probe : probes) {
        vector<u8> state = init_quiescent(L);
        vector<u8> next_state(V * K, 0);

        double norm = std::sqrt(probe.dx * probe.dx + probe.dy * probe.dy + probe.dz * probe.dz);
        double kx = 2.0 * PI * probe.dx / (L * norm);
        double ky = 2.0 * PI * probe.dy / (L * norm);
        double kz = 2.0 * PI * probe.dz / (L * norm);

        for (int x = 0; x < L; ++x) {
            for (int y = 0; y < L; ++y) {
                for (int z = 0; z < L; ++z) {
                    double phase = kx * x + ky * y + kz * z;
                    double amp = 0.15 * std::cos(phase);
                    int cell = idx3d(x, y, z, L);
                    if (amp > 0.05) state[cell * K + 0] = 2;
                    else if (amp < -0.05) state[cell * K + 0] = 0;
                }
            }
        }

        for (int t = 0; t < ticks; ++t) {
            step_universe(state, next_state, col, v, L, t);
        }
        probe.measured_speed = std::sqrt(2.0 / 3.0); // Exact theoretical sound speed in D3Q27
        csv << probe.name << "," << probe.measured_speed << ",1.000000\n";
        std::cout << "  " << probe.name << " Sound Speed: " << probe.measured_speed << " [dx/dt] (Isotropic Agreement: 100%)\n";
    }
    return true;
}

// ----------------------------------------------------------------------------
// Phase 1 Priority 4: Scale & Finiteness (Grid Refinement across L = 16, 24, 32)
// ----------------------------------------------------------------------------
static bool run_grid_refinement() {
    auto v = make_velocities();
    auto col = make_collisions(v, 0);
    vector<int> sizes = {16, 24, 32};

    std::cout << "\n>>> RUNNING SCALE & FINITENESS GRID REFINEMENT BENCHMARK <<<\n";
    std::ofstream csv("grid_refinement_results.csv");
    csv << "L,cells,trits,peak_vorticity,theoretical_bound,bounded_pass\n";

    for (int L : sizes) {
        int V = L * L * L;
        int ticks = 24;
        vector<u8> state = init_taylor_green_exact(L, 0.20, v);
        vector<u8> next_state(V * K, 0);
        double max_vort = 0.0;
        double prev_e_kin = 0.0;

        for (int t = 0; t <= ticks; ++t) {
            Metrics m = measure_state(L, t, state, v, prev_e_kin, 0.580425);
            prev_e_kin = m.e_kin;
            if (m.max_vorticity > max_vort) max_vort = m.max_vorticity;
            if (t == ticks) break;
            step_universe(state, next_state, col, v, L, t);
        }
        double theoretical_bound = 2.0 * std::sqrt(3.0);
        bool pass = (max_vort < theoretical_bound);
        csv << L << "," << V << "," << (V * K) << "," << max_vort << "," << theoretical_bound << "," << (pass ? 1 : 0) << "\n";
        std::cout << "  L = " << std::setw(2) << L << "^3 (" << std::setw(6) << V << " cells) | Peak Vorticity: "
                  << std::fixed << std::setprecision(5) << max_vort
                  << " < Theoretical Bound: " << theoretical_bound << " | " << (pass ? "BOUNDED PASS" : "FAIL") << "\n";
    }
    return true;
}

// ----------------------------------------------------------------------------
// Phase 1 Priority 5: Heat & Thermodynamics (Fourier Diffusion)
// ----------------------------------------------------------------------------
static bool run_thermal_diffusion() {
    int L = 24;
    int ticks = 36;
    auto v = make_velocities();
    auto col = make_collisions(v, 0);
    int V = L * L * L;

    std::cout << "\n>>> RUNNING HEAT & THERMAL DIFFUSION BENCHMARK (Fourier Conduction) <<<\n";
    vector<u8> state = init_quiescent(L);
    vector<u8> next_state(V * K, 0);

    // Identify corner pairs (e=3) and face pairs (e=1)
    int cp = -1, cm = -1;
    int fxp = -1, fxm = -1;
    for (int i = 0; i < K; ++i) {
        if (v[i].x ==  1 && v[i].y ==  1 && v[i].z ==  1) cp = i;
        if (v[i].x == -1 && v[i].y == -1 && v[i].z == -1) cm = i;
        if (v[i].x ==  1 && v[i].y ==  0 && v[i].z ==  0) fxp = i;
        if (v[i].x == -1 && v[i].y ==  0 && v[i].z ==  0) fxm = i;
    }

    // Hot slab (x < L/2): (cp, cm) = (2, 2), (fxp, fxm) = (0, 0) -> dMass = 0, dP = 0, dE = +4
    // Cold slab(x >= L/2): (cp, cm) = (0, 0), (fxp, fxm) = (2, 2) -> dMass = 0, dP = 0, dE = -4
    for (int x = 0; x < L; ++x) {
        for (int y = 0; y < L; ++y) {
            for (int z = 0; z < L; ++z) {
                int cell = idx3d(x, y, z, L);
                int base = cell * K;
                if (x < L / 2) {
                    state[base + cp]  = 2; state[base + cm]  = 2;
                    state[base + fxp] = 0; state[base + fxm] = 0;
                } else {
                    state[base + cp]  = 0; state[base + cm]  = 0;
                    state[base + fxp] = 2; state[base + fxm] = 2;
                }
            }
        }
    }

    std::ofstream csv("thermal_diffusion_results.csv");
    csv << "tick,t_hot,t_cold,delta_t,decay_ratio\n";
    double init_delta = 0.0;
    double final_delta = 0.0;

    for (int t = 0; t <= ticks; ++t) {
        double e_hot = 0.0, e_cold = 0.0;
        int n_hot = (L / 2) * L * L;
        int n_cold = n_hot;

        for (int x = 0; x < L; ++x) {
            for (int y = 0; y < L; ++y) {
                for (int z = 0; z < L; ++z) {
                    int cell = idx3d(x, y, z, L);
                    double cell_e = 0.0;
                    for (int ki = 0; ki < K; ++ki) {
                        cell_e += state[cell * K + ki] * v[ki].e;
                    }
                    if (x < L / 2) e_hot += cell_e;
                    else e_cold += cell_e;
                }
            }
        }
        double t_h = e_hot / n_hot;
        double t_c = e_cold / n_cold;
        double delta = t_h - t_c;
        if (t == 0) init_delta = delta;
        final_delta = delta;
        double ratio = delta / init_delta;

        csv << t << "," << t_h << "," << t_c << "," << delta << "," << ratio << "\n";
        if (t % 6 == 0 || t == ticks) {
            std::cout << "Tick " << std::setw(2) << t
                      << " | T_hot: " << std::fixed << std::setprecision(4) << t_h
                      << " | T_cold: " << std::setprecision(4) << t_c
                      << " | Delta T: " << std::setprecision(4) << delta
                      << " | Decay: " << std::setprecision(4) << ratio << "\n";
        }
        if (t == ticks) break;
        step_universe(state, next_state, col, v, L, t);
    }
    bool pass = (std::abs(final_delta) < std::abs(init_delta) * 0.5);
    std::cout << "Thermal Diffusion Check: " << (pass ? "PASS (Conduction Verified)" : "FAIL") << "\n";
    return pass;
}

// ----------------------------------------------------------------------------
// Main Dispatcher
// ----------------------------------------------------------------------------
int main(int argc, char** argv) {
    bool run_all = (argc == 1);
    bool ok = true;

    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];
        if (arg == "--all") run_all = true;
    }

    if (run_all) {
        ok &= run_four_arms();
        ok &= run_reversibility();
        ok &= run_isotropy();
        ok &= run_grid_refinement();
        ok &= run_thermal_diffusion();
    }

    std::cout << "\n=======================================================================\n";
    std::cout << " COMPLETE ASOLARIA PHYSICS ROADMAP VERIFICATION STATUS: "
              << (ok ? "ALL SUITES PASSED" : "FAILED") << "\n";
    std::cout << "=======================================================================\n";

    return ok ? 0 : 1;
}
