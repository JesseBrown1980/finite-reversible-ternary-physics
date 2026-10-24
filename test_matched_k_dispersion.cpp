#include "acoustic_damped_fit.h"
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

using std::array;
using std::vector;
using std::string;
using u8  = std::uint8_t;
using u64 = std::uint64_t;

constexpr int K = 27;

struct Velocity {
    int x, y, z;
    int energy;
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

struct Gate {
    int a, b, c, d;
    array<u8, 81> lut;
};

static array<int, 4> decode_mask(int mask) {
    array<int, 4> dec{};
    for (int i = 0; i < 4; ++i) {
        dec[i] = mask % 3;
        mask /= 3;
    }
    return dec;
}

static int encode_mask(int a, int b, int c, int d) {
    return a + 3 * b + 9 * c + 27 * d;
}

static array<int, 5> get_invariants(const array<int, 4>& counts,
                                    const array<int, 4>& q,
                                    const array<Velocity, K>& v) {
    int mass = 0, px = 0, py = 0, pz = 0, e = 0;
    for (int i = 0; i < 4; ++i) {
        int n = counts[i];
        mass += n;
        px += n * v[q[i]].x;
        py += n * v[q[i]].y;
        pz += n * v[q[i]].z;
        e  += n * v[q[i]].energy;
    }
    return {mass, px, py, pz, e};
}

static vector<Gate> make_collisions(const array<Velocity, K>& v) {
    using PairKey = std::tuple<int, int, int, int>;
    std::map<PairKey, vector<array<int, 2>>> pair_map;
    for (int i = 0; i < K; ++i) {
        for (int j = i + 1; j < K; ++j) {
            auto a = v[i], b = v[j];
            pair_map[{a.x + b.x, a.y + b.y, a.z + b.z, a.energy + b.energy}].push_back({i, j});
        }
    }

    std::map<array<int, 4>, bool> unique_quads;
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

    vector<Gate> gates;
    for (const auto& kv : unique_quads) {
        array<int, 4> q = kv.first;
        Gate g{q[0], q[1], q[2], q[3], {}};
        std::map<array<int, 5>, vector<int>> classes;
        for (int mask = 0; mask < 81; ++mask) {
            g.lut[mask] = static_cast<u8>(mask);
            classes[get_invariants(decode_mask(mask), q, v)].push_back(mask);
        }
        for (const auto& c_entry : classes) {
            vector<int> codes;
            for (int code : c_entry.second) {
                if (code != 40) codes.push_back(code);
            }
            for (size_t j = 0; j + 1 < codes.size(); j += 2) {
                int c1 = codes[j];
                int c2 = codes[j + 1];
                g.lut[c1] = static_cast<u8>(c2);
                g.lut[c2] = static_cast<u8>(c1);
            }
        }
        bool active = false;
        for (int mask = 0; mask < 81; ++mask) {
            if (g.lut[mask] != mask) { active = true; break; }
        }
        if (active) gates.push_back(g);
    }
    return gates;
}

static inline int cell_index(int x, int y, int z, int L) {
    return (x * L + y) * L + z;
}

static void step_universe(vector<u8>& state, vector<u8>& next_state,
                          const vector<Gate>& gates, const array<Velocity, K>& v, int L, int t) {
    int num_cells = L * L * L;
    for (int cell = 0; cell < num_cells; ++cell) {
        for (int j = 0; j < 32; ++j) {
            size_t gate_idx = (static_cast<size_t>(t) * 32 + static_cast<size_t>(cell) * 17 + static_cast<size_t>(j) * 37) % gates.size();
            const auto& g = gates[gate_idx];
            int base = cell * K;
            int in_mask = encode_mask(state[base + g.a], state[base + g.b], state[base + g.c], state[base + g.d]);
            int out_mask = g.lut[in_mask];
            auto out_counts = decode_mask(out_mask);
            state[base + g.a] = static_cast<u8>(out_counts[0]);
            state[base + g.b] = static_cast<u8>(out_counts[1]);
            state[base + g.c] = static_cast<u8>(out_counts[2]);
            state[base + g.d] = static_cast<u8>(out_counts[3]);
        }
    }

    for (int x = 0; x < L; ++x) {
        for (int y = 0; y < L; ++y) {
            for (int z = 0; z < L; ++z) {
                int src_cell = cell_index(x, y, z, L);
                for (int k = 0; k < K; ++k) {
                    int nx = (x + v[k].x + L) % L;
                    int ny = (y + v[k].y + L) % L;
                    int nz = (z + v[k].z + L) % L;
                    int dst_cell = cell_index(nx, ny, nz, L);
                    next_state[dst_cell * K + k] = state[src_cell * K + k];
                }
            }
        }
    }
    state.swap(next_state);
}

struct MatchedKResult {
    int L;
    int kx, ky, kz;
    double k_mag;
    double measured_cs;
    double damping;
    double r_squared;
    string name;
};

static MatchedKResult run_matched_k_probe(int L, int kx, int ky, int kz, const string& name,
                                         const vector<Gate>& gates, const array<Velocity, K>& v) {
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

                // Modulate rest vs face particles to seed pure density wave without initial momentum
                int base = c * K;
                if (cval > 0.4) {
                    state[base + 13] = 0; // remove rest particle
                    state[base + 22] = 2; // add to opposing face pair (preserving zero net momentum)
                    state[base + 4]  = 2;
                } else if (cval < -0.4) {
                    state[base + 13] = 2; // add rest particle
                    state[base + 22] = 0; // remove opposing face pair
                    state[base + 4]  = 0;
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
        t_series.push_back(t);
        amp_series.push_back(amp);
        if (t == ticks) break;
        step_universe(state, next_state, gates, v, L, t);
    }

    const auto fit = acoustic::fit(t_series, amp_series);
    double cs = fit.omega / k_mag;
    return {L, kx, ky, kz, k_mag, cs, fit.alpha, fit.r_squared, name};
}

int main() {
    auto v = make_velocities();
    auto gates = make_collisions(v);

    std::cout << "=======================================================================\n";
    std::cout << " ASOLARIA MATCHED-|k| ACOUSTIC DISPERSION BENCHMARK\n";
    std::cout << " Directly comparing equal-magnitude wavevectors:\n";
    std::cout << " k1 = (3, 0, 0) vs k2 = (2, 2, 1)  [Both have |k|^2 = 9*(2*pi/L)^2]\n";
    std::cout << " ZERO wavenumber dispersion difference | Isolates true directional anisotropy\n";
    std::cout << "=======================================================================\n";

    std::ofstream csv("matched_k_dispersion_results.csv");
    csv << "L,kx,ky,kz,k_mag,measured_cs,damping,r_squared,name\n";

    for (int L : {24, 48}) {
        std::cout << "\n>>> Domain L = " << L << "x" << L << "x" << L << " <<<\n";
        auto r1 = run_matched_k_probe(L, 3, 0, 0, "Axis (3, 0, 0)", gates, v);
        auto r2 = run_matched_k_probe(L, 2, 2, 1, "Oblique (2, 2, 1)", gates, v);

        double mean_cs = (r1.measured_cs + r2.measured_cs) / 2.0;
        double delta_cs = std::abs(r1.measured_cs - r2.measured_cs);
        double aniso_pct = (delta_cs / mean_cs) * 100.0;

        std::cout << "  - " << r1.name << " : |k| = " << std::fixed << std::setprecision(4) << r1.k_mag
                  << " | c_s = " << std::setprecision(5) << r1.measured_cs
                  << " | alpha = " << std::setprecision(5) << r1.damping
                  << " | R^2 = " << std::setprecision(5) << r1.r_squared << "\n";

        std::cout << "  - " << r2.name << " : |k| = " << std::fixed << std::setprecision(4) << r2.k_mag
                  << " | c_s = " << std::setprecision(5) << r2.measured_cs
                  << " | alpha = " << std::setprecision(5) << r2.damping
                  << " | R^2 = " << std::setprecision(5) << r2.r_squared << "\n";

        std::cout << "  ===> Matched-|k| Directional Anisotropy: " << std::setprecision(2) << aniso_pct << "%\n";

        csv << r1.L << "," << r1.kx << "," << r1.ky << "," << r1.kz << "," << r1.k_mag << ","
            << r1.measured_cs << "," << r1.damping << "," << r1.r_squared << "," << r1.name << "\n";
        csv << r2.L << "," << r2.kx << "," << r2.ky << "," << r2.kz << "," << r2.k_mag << ","
            << r2.measured_cs << "," << r2.damping << "," << r2.r_squared << "," << r2.name << "\n";
    }

    std::cout << "\n=======================================================================\n";
    std::cout << " MATCHED-|k| DATA EXPORTED TO matched_k_dispersion_results.csv\n";
    std::cout << "=======================================================================\n";
    return 0;
}
