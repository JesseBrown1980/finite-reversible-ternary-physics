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

static vector<Gate> make_collisions(const array<Velocity, K>& v, int variant = 0) {
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
    size_t q_idx = 0;
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
            bool perform = (variant == 0) || ((c_entry.first[0] + static_cast<long long>(q_idx)) % 2 == 0);
            if (perform) {
                for (size_t j = 0; j + 1 < codes.size(); j += 2) {
                    int c1 = codes[j];
                    int c2 = codes[j + 1];
                    g.lut[c1] = static_cast<u8>(c2);
                    g.lut[c2] = static_cast<u8>(c1);
                }
            }
        }
        bool active = false;
        for (int mask = 0; mask < 81; ++mask) {
            if (g.lut[mask] != mask) { active = true; break; }
        }
        if (active) gates.push_back(g);
        q_idx++;
    }
    return gates;
}

static inline int cell_index(int x, int y, int z, int L) {
    return (x * L + y) * L + z;
}

// Full forward simulation step
static void step_universe(vector<u8>& state, vector<u8>& next_state,
                          const vector<Gate>& gates, const array<Velocity, K>& v, int L, int t,
                          bool collision_only = false, bool streaming_only = false) {
    int num_cells = L * L * L;
    if (!streaming_only) {
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
    }

    if (!collision_only) {
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
}

// 90-degree rotation around z-axis: (x, y, z) -> (-y, x, z)
// Corresponding velocity rotation: R(vx, vy, vz) = (-vy, vx, vz)
static array<int, K> make_velocity_rot_map(const array<Velocity, K>& v) {
    array<int, K> rmap{};
    for (int i = 0; i < K; ++i) {
        int rx = -v[i].y;
        int ry =  v[i].x;
        int rz =  v[i].z;
        for (int j = 0; j < K; ++j) {
            if (v[j].x == rx && v[j].y == ry && v[j].z == rz) {
                rmap[i] = j;
                break;
            }
        }
    }
    return rmap;
}

static vector<u8> rotate_state_z90(const vector<u8>& state, int L, const array<int, K>& rmap) {
    int V = L * L * L;
    vector<u8> rot(V * K, 0);
    for (int x = 0; x < L; ++x) {
        for (int y = 0; y < L; ++y) {
            for (int z = 0; z < L; ++z) {
                int src_cell = cell_index(x, y, z, L);
                int rx = (-y + L) % L;
                int ry = (x + L) % L;
                int rz = z;
                int dst_cell = cell_index(rx, ry, rz, L);
                for (int k = 0; k < K; ++k) {
                    int rk = rmap[k];
                    rot[dst_cell * K + rk] = state[src_cell * K + k];
                }
            }
        }
    }
    return rot;
}

int main() {
    auto v = make_velocities();
    auto gates_std = make_collisions(v, 0);
    auto gates_alt = make_collisions(v, 1);
    auto rmap = make_velocity_rot_map(v);

    std::cout << "=======================================================================\n";
    std::cout << " ASOLARIA MICROSCOPIC ROTATIONAL EQUIVARIANCE TEST: F(R s) vs R(F s)\n";
    std::cout << "=======================================================================\n";

    // 1. Analytical rational moments check
    double sum_cx2 = 0, sum_cx4 = 0, sum_cx2cy2 = 0;
    for (int i = 0; i < K; ++i) {
        sum_cx2 += v[i].x * v[i].x;
        sum_cx4 += std::pow(v[i].x, 4);
        sum_cx2cy2 += (v[i].x * v[i].x) * (v[i].y * v[i].y);
    }
    std::cout << "\n>>> Exact Rational Velocity Moments (Unweighted D3Q27) <<<\n";
    std::cout << "  <c_x^2>     = " << sum_cx2 << " / 27 = " << (sum_cx2 / 27.0) << " (Exact: 2/3)\n";
    std::cout << "  <c_x^4>     = " << sum_cx4 << " / 27 = " << (sum_cx4 / 27.0) << " (Exact: 2/3)\n";
    std::cout << "  <c_x^2 c_y^2> = " << sum_cx2cy2 << " / 27 = " << (sum_cx2cy2 / 27.0) << " (Exact: 4/9)\n";
    std::cout << "  Fourth-Order Isotropy requires: <c_x^4> = 3 * <c_x^2 c_y^2>\n";
    std::cout << "  Calculated: 2/3 = " << (2.0/3.0) << " vs 3 * (4/9) = 4/3 = " << (4.0/3.0) << "\n";
    std::cout << "  ===> Microscopic Fourth-Order Cubic Deficit: Delta_aniso = 2/3 - 4/3 = -2/3 != 0 (CONFIRMED)\n";

    // 2. Dynamic Equivariance Test across grids L = 6, 8, 12
    for (int L : {6, 8, 12}) {
        int V = L * L * L;
        int total_trits = V * K;

        // Construct legal non-trivial state (Taylor-Green initialization)
        vector<u8> s0(total_trits, 1);
        for (int x = 0; x < L; ++x) {
            for (int y = 0; y < L; ++y) {
                for (int z = 0; z < L; ++z) {
                    int c = cell_index(x, y, z, L) * K;
                    if ((x + y + z) % 3 == 0) {
                        s0[c + 22] = 2; s0[c + 4] = 0;
                    } else if ((x + y + z) % 3 == 1) {
                        s0[c + 16] = 2; s0[c + 10] = 0;
                    }
                }
            }
        }

        // Test Streaming Only
        {
            vector<u8> s_stream = s0, n_stream(total_trits, 0);
            step_universe(s_stream, n_stream, gates_std, v, L, 0, false, true); // streaming only
            vector<u8> R_Fs = rotate_state_z90(s_stream, L, rmap);

            vector<u8> Rs = rotate_state_z90(s0, L, rmap);
            vector<u8> F_Rs = Rs, n_rot(total_trits, 0);
            step_universe(F_Rs, n_rot, gates_std, v, L, 0, false, true);

            u64 stream_mismatches = 0;
            for (size_t i = 0; i < total_trits; ++i) {
                if (R_Fs[i] != F_Rs[i]) stream_mismatches++;
            }
            std::cout << "\n>>> Grid " << L << "x" << L << "x" << L << " (" << total_trits << " trits) <<<\n";
            std::cout << "  - Pure Streaming Equivariance: " << (stream_mismatches == 0 ? "PASS (0 mismatches)" : "FAIL") << "\n";
        }

        // Test Standard Collision + Streaming
        {
            vector<u8> s_std = s0, n_std(total_trits, 0);
            step_universe(s_std, n_std, gates_std, v, L, 0);
            vector<u8> R_Fs = rotate_state_z90(s_std, L, rmap);

            vector<u8> Rs = rotate_state_z90(s0, L, rmap);
            vector<u8> F_Rs = Rs, n_rot(total_trits, 0);
            step_universe(F_Rs, n_rot, gates_std, v, L, 0);

            u64 mismatches = 0;
            for (size_t i = 0; i < total_trits; ++i) {
                if (R_Fs[i] != F_Rs[i]) mismatches++;
            }
            std::cout << "  - Standard Update Mismatches : " << mismatches << " / " << total_trits << "\n";
        }

        // Test Alternative Collision + Streaming
        {
            vector<u8> s_alt = s0, n_alt(total_trits, 0);
            step_universe(s_alt, n_alt, gates_alt, v, L, 0);
            vector<u8> R_Fs = rotate_state_z90(s_alt, L, rmap);

            vector<u8> Rs = rotate_state_z90(s0, L, rmap);
            vector<u8> F_Rs = Rs, n_rot(total_trits, 0);
            step_universe(F_Rs, n_rot, gates_alt, v, L, 0);

            u64 mismatches = 0;
            for (size_t i = 0; i < total_trits; ++i) {
                if (R_Fs[i] != F_Rs[i]) mismatches++;
            }
            std::cout << "  - Alternative Update Mismatches: " << mismatches << " / " << total_trits << "\n";
        }
    }
    return 0;
}
