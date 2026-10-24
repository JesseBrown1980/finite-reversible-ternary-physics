#include <iostream>
#include <vector>
#include <array>
#include <map>
#include <set>
#include <algorithm>
#include <cstdint>
#include <random>
#include <cassert>
#include "face_lut.h"

using std::array; using std::vector;
using u8 = std::uint8_t;
using u64 = std::uint64_t;
constexpr int K = 27;

struct Vel { int x, y, z, e; };

static array<Vel, K> make_vels() {
    array<Vel, K> v{};
    int idx = 0;
    for (int x = -1; x <= 1; ++x)
        for (int y = -1; y <= 1; ++y)
            for (int z = -1; z <= 1; ++z)
                v[idx++] = {x, y, z, x*x + y*y + z*z};
    return v;
}

struct Gate {
    int a, b, c, d;
    array<u8, 81> lut;
};

static array<int, 4> decode_mask(int mask) {
    array<int, 4> dec{};
    for (int i = 0; i < 4; ++i) { dec[i] = mask % 3; mask /= 3; }
    return dec;
}

static int encode_mask(int a, int b, int c, int d) {
    return a + 3 * b + 9 * c + 27 * d;
}

static array<int, 5> get_invariants(const array<int, 4>& counts,
                                    const array<int, 4>& q,
                                    const array<Vel, K>& v) {
    int mass = 0, px = 0, py = 0, pz = 0, e = 0;
    for (int i = 0; i < 4; ++i) {
        int n = counts[i];
        mass += n; px += n * v[q[i]].x; py += n * v[q[i]].y; pz += n * v[q[i]].z; e += n * v[q[i]].e;
    }
    return {mass, px, py, pz, e};
}

static Gate build_gate(int a, int b, int c, int d, const array<Vel, K>& v) {
    array<int, 4> q = {a, b, c, d};
    Gate g{a, b, c, d, {}};
    std::map<array<int, 5>, vector<int>> classes;
    for (int mask = 0; mask < 81; ++mask) {
        g.lut[mask] = static_cast<u8>(mask);
        classes[get_invariants(decode_mask(mask), q, v)].push_back(mask);
    }
    for (const auto& c_entry : classes) {
        vector<int> codes;
        for (int code : c_entry.second) if (code != 40) codes.push_back(code);
        for (size_t j = 0; j + 1 < codes.size(); j += 2) {
            int c1 = codes[j]; int c2 = codes[j + 1];
            g.lut[c1] = static_cast<u8>(c2); g.lut[c2] = static_cast<u8>(c1);
        }
    }
    return g;
}

static void apply_gate(u8* s, const Gate& g) {
    int in_mask = encode_mask(s[g.a], s[g.b], s[g.c], s[g.d]);
    int out_mask = g.lut[in_mask];
    auto dec = decode_mask(out_mask);
    s[g.a] = static_cast<u8>(dec[0]);
    s[g.b] = static_cast<u8>(dec[1]);
    s[g.c] = static_cast<u8>(dec[2]);
    s[g.d] = static_cast<u8>(dec[3]);
}

static inline void apply_face_lut(u8* s) {
    // Face channels in D3Q27 indexing:
    // +x: 22, -x: 4, +y: 16, -y: 10, +z: 14, -z: 12
    int code = s[22] + 3 * (s[4] + 3 * (s[16] + 3 * (s[10] + 3 * (s[14] + 3 * s[12]))));
    uint16_t out = FACE_LUT[code];
    s[22] = out % 3; out /= 3;
    s[4]  = out % 3; out /= 3;
    s[16] = out % 3; out /= 3;
    s[10] = out % 3; out /= 3;
    s[14] = out % 3; out /= 3;
    s[12] = out;
}

static void apply_multishell_collision(u8* s, const vector<Gate>& edge_gates) {
    // 1. Commuting Edge gates
    for (const auto& g : edge_gates) {
        apply_gate(s, g);
    }
    // 2. Exact Equivariant Face LUT
    apply_face_lut(s);
}

struct Mat3 { int m[3][3]; };
static vector<Mat3> make_full_Oh() {
    vector<Mat3> ops;
    int p[3] = {0, 1, 2};
    do {
        for (int sx : {-1, 1})
            for (int sy : {-1, 1})
                for (int sz : {-1, 1}) {
                    Mat3 M{};
                    M.m[0][p[0]] = sx; M.m[1][p[1]] = sy; M.m[2][p[2]] = sz;
                    ops.push_back(M);
                }
    } while (std::next_permutation(p, p + 3));
    return ops;
}

static int rotate_vel(int k, const Mat3& M, const array<Vel, K>& v) {
    int x = v[k].x, y = v[k].y, z = v[k].z;
    int rx = M.m[0][0]*x + M.m[0][1]*y + M.m[0][2]*z;
    int ry = M.m[1][0]*x + M.m[1][1]*y + M.m[1][2]*z;
    int rz = M.m[2][0]*x + M.m[2][1]*y + M.m[2][2]*z;
    for (int i = 0; i < K; ++i) if (v[i].x == rx && v[i].y == ry && v[i].z == rz) return i;
    return -1;
}

static array<u8, K> rotate_state(const array<u8, K>& s, const Mat3& M, const array<Vel, K>& v) {
    array<u8, K> r{};
    for (int k = 0; k < K; ++k) {
        int rk = rotate_vel(k, M, v);
        r[rk] = s[k];
    }
    return r;
}

static inline int cell_index(int x, int y, int z, int L) {
    return (x * L + y) * L + z;
}

static vector<u8> rotate_full_lattice(const vector<u8>& state, int L, const Mat3& M, const array<Vel, K>& v) {
    int V = L * L * L;
    vector<u8> rot(V * K, 0);
    array<int, K> rmap{};
    for (int k = 0; k < K; ++k) rmap[k] = rotate_vel(k, M, v);

    for (int x = 0; x < L; ++x) {
        for (int y = 0; y < L; ++y) {
            for (int z = 0; z < L; ++z) {
                int src_cell = cell_index(x, y, z, L);
                int rx = (M.m[0][0]*x + M.m[0][1]*y + M.m[0][2]*z) % L; if (rx < 0) rx += L;
                int ry = (M.m[1][0]*x + M.m[1][1]*y + M.m[1][2]*z) % L; if (ry < 0) ry += L;
                int rz = (M.m[2][0]*x + M.m[2][1]*y + M.m[2][2]*z) % L; if (rz < 0) rz += L;
                int dst_cell = cell_index(rx, ry, rz, L);
                for (int k = 0; k < K; ++k) {
                    rot[dst_cell * K + rmap[k]] = state[src_cell * K + k];
                }
            }
        }
    }
    return rot;
}

static void step_lattice_multishell(vector<u8>& state, vector<u8>& next_state,
                                   const vector<Gate>& edge_gates,
                                   const array<Vel, K>& v, int L) {
    int V = L * L * L;
    // 1. Multi-shell equivariant collision step
    for (int cell = 0; cell < V; ++cell) {
        apply_multishell_collision(&state[cell * K], edge_gates);
    }

    // 2. Exact periodic integer streaming
    for (int x = 0; x < L; ++x) {
        for (int y = 0; y < L; ++y) {
            for (int z = 0; z < L; ++z) {
                int src = cell_index(x, y, z, L);
                for (int k = 0; k < K; ++k) {
                    int nx = (x + v[k].x + L) % L;
                    int ny = (y + v[k].y + L) % L;
                    int nz = (z + v[k].z + L) % L;
                    int dst = cell_index(nx, ny, nz, L);
                    next_state[dst * K + k] = state[src * K + k];
                }
            }
        }
    }
    state = next_state;
}

int main() {
    std::cout << "====================================================================\n";
    std::cout << "  Multi-Shell Equivariant Involutive Collision Kernel Verification  \n";
    std::cout << "====================================================================\n\n";

    auto v = make_vels();
    auto Oh = make_full_Oh();

    // The 3 edge gates
    vector<Gate> edge_gates = {
        build_gate(1, 7, 19, 25, v),  // Gate 1: xy edge plane
        build_gate(3, 5, 21, 23, v),  // Gate 2: xz edge plane
        build_gate(9, 11, 15, 17, v)   // Gate 3: yz edge plane
    };

    // -------------------------------------------------------------------------
    // Experiment 1: Single-Cell Exact Equivariance across ALL 48 Oh operations
    // -------------------------------------------------------------------------
    std::cout << "[Experiment 1] Single-Cell Equivariance & Involution across 48 Oh ops...\n";
    std::mt19937_64 rng(42ULL);
    std::uniform_int_distribution<int> trit_dist(0, 2);

    int num_random_cells = 5000;
    int local_mismatches = 0;
    int involution_failures = 0;
    int conservation_failures = 0;

    for (int trial = 0; trial < num_random_cells; ++trial) {
        array<u8, K> s{};
        int m0 = 0, px0 = 0, py0 = 0, pz0 = 0, e0 = 0;
        for (int k = 0; k < K; ++k) {
            s[k] = static_cast<u8>(trit_dist(rng));
            m0 += s[k];
            px0 += s[k] * v[k].x; py0 += s[k] * v[k].y; pz0 += s[k] * v[k].z;
            e0 += s[k] * v[k].e;
        }

        // Test conservation & involution
        array<u8, K> s_col = s;
        apply_multishell_collision(s_col.data(), edge_gates);

        int m1 = 0, px1 = 0, py1 = 0, pz1 = 0, e1 = 0;
        for (int k = 0; k < K; ++k) {
            m1 += s_col[k];
            px1 += s_col[k] * v[k].x; py1 += s_col[k] * v[k].y; pz1 += s_col[k] * v[k].z;
            e1 += s_col[k] * v[k].e;
        }
        if (m0 != m1 || px0 != px1 || py0 != py1 || pz0 != pz1 || e0 != e1) {
            conservation_failures++;
        }

        array<u8, K> s_rev = s_col;
        apply_multishell_collision(s_rev.data(), edge_gates);
        if (s_rev != s) involution_failures++;

        // Test C(R s) == R(C s) for all 48 operations
        for (const auto& M : Oh) {
            auto Rs = rotate_state(s, M, v);
            apply_multishell_collision(Rs.data(), edge_gates);
            auto R_Cs = rotate_state(s_col, M, v);
            if (Rs != R_Cs) local_mismatches++;
        }
    }

    std::cout << "  - Random States Tested : " << num_random_cells << "\n";
    std::cout << "  - Conservation Failures: " << conservation_failures << "\n";
    std::cout << "  - Involution Failures  : " << involution_failures << "\n";
    std::cout << "  - Equivariance Checks  : " << (num_random_cells * 48) << " operations\n";
    std::cout << "  - Equivariance Mismatch: " << local_mismatches << "\n";
    assert(conservation_failures == 0);
    assert(involution_failures == 0);
    assert(local_mismatches == 0);

    // -------------------------------------------------------------------------
    // Experiment 2: Active Collision Tests on Face & Thermal Perturbations
    // -------------------------------------------------------------------------
    std::cout << "\n[Experiment 2] Active Collision Cell Count on Physical Perturbations (L=24)...\n";
    int L = 24;
    int V = L * L * L;

    // A: Face-Channel Acoustic Disturbance
    vector<u8> state_face(V * K, 1);
    for (int x = 0; x < L; ++x) {
        for (int y = 0; y < L; ++y) {
            for (int z = 0; z < L; ++z) {
                int cell = cell_index(x, y, z, L);
                // Perturb head-on face channels (+x:22, -x:4)
                if ((x % 4) == 0) {
                    state_face[cell * K + 22] = 2; // +x
                    state_face[cell * K + 4]  = 2; // -x
                }
            }
        }
    }
    int face_active_cells = 0;
    for (int cell = 0; cell < V; ++cell) {
        array<u8, K> orig{};
        for (int k = 0; k < K; ++k) orig[k] = state_face[cell * K + k];
        apply_multishell_collision(&state_face[cell * K], edge_gates);
        bool changed = false;
        for (int k = 0; k < K; ++k) if (state_face[cell * K + k] != orig[k]) { changed = true; break; }
        if (changed) face_active_cells++;
    }
    std::cout << "  - Face-Channel Perturbation: " << face_active_cells << " / " << V 
              << " active collision cells (" << (100.0 * face_active_cells / V) << "%)\n";

    // B: Hot-Cold Thermal Slab (Thermal Contrast)
    vector<u8> state_thermal(V * K, 1);
    for (int x = 0; x < L; ++x) {
        for (int y = 0; y < L; ++y) {
            for (int z = 0; z < L; ++z) {
                int cell = cell_index(x, y, z, L);
                if (x < L / 2) {
                    // Hot slab: excess face + edge energy
                    state_thermal[cell * K + 22] = 2; state_thermal[cell * K + 4] = 2;
                    state_thermal[cell * K + 1]  = 2; state_thermal[cell * K + 7] = 2;
                } else {
                    // Cold slab
                    state_thermal[cell * K + 22] = 0; state_thermal[cell * K + 4] = 0;
                    state_thermal[cell * K + 1]  = 0; state_thermal[cell * K + 7] = 0;
                }
            }
        }
    }
    int thermal_active_cells = 0;
    for (int cell = 0; cell < V; ++cell) {
        array<u8, K> orig{};
        for (int k = 0; k < K; ++k) orig[k] = state_thermal[cell * K + k];
        apply_multishell_collision(&state_thermal[cell * K], edge_gates);
        bool changed = false;
        for (int k = 0; k < K; ++k) if (state_thermal[cell * K + k] != orig[k]) { changed = true; break; }
        if (changed) thermal_active_cells++;
    }
    std::cout << "  - Thermal Slab Perturbation: " << thermal_active_cells << " / " << V 
              << " active collision cells (" << (100.0 * thermal_active_cells / V) << "%)\n";

    // -------------------------------------------------------------------------
    // Experiment 3: Full 3D Lattice Equivariance across ALL 48 Oh operations
    // -------------------------------------------------------------------------
    std::cout << "\n[Experiment 3] Full 3D Lattice Dynamic Equivariance F(Rs) == R(Fs)...\n";
    int lat_L = 8;
    int lat_V = lat_L * lat_L * lat_L;
    int total_trits = lat_V * K;

    vector<u8> s_base(total_trits);
    for (int i = 0; i < total_trits; ++i) s_base[i] = static_cast<u8>(trit_dist(rng));

    int lat_mismatches = 0;
    int op_idx = 0;
    vector<u8> scratch(total_trits);

    for (const auto& M : Oh) {
        op_idx++;
        // Path A: Rotate full lattice, then advance 2 steps
        vector<u8> state_A = rotate_full_lattice(s_base, lat_L, M, v);
        for (int step = 0; step < 2; ++step) {
            step_lattice_multishell(state_A, scratch, edge_gates, v, lat_L);
        }

        // Path B: Advance 2 steps on unrotated lattice, then rotate full lattice
        vector<u8> state_B = s_base;
        for (int step = 0; step < 2; ++step) {
            step_lattice_multishell(state_B, scratch, edge_gates, v, lat_L);
        }
        vector<u8> R_state_B = rotate_full_lattice(state_B, lat_L, M, v);

        // Compare
        int mismatches = 0;
        for (int i = 0; i < total_trits; ++i) {
            if (state_A[i] != R_state_B[i]) mismatches++;
        }
        lat_mismatches += mismatches;
    }

    std::cout << "  - Tested Lattice Size : " << lat_L << "^3 (" << total_trits << " trits)\n";
    std::cout << "  - Tested Oh Operations: " << op_idx << " / 48\n";
    std::cout << "  - Total Trit Checks   : " << (total_trits * op_idx) << "\n";
    std::cout << "  - Total Mismatches    : " << lat_mismatches << "\n";
    std::cout << "  - Multi-Shell Result  : " << (lat_mismatches == 0 ? "PASS (100% Exact Equivariance)" : "FAIL") << "\n\n";

    if (local_mismatches == 0 && involution_failures == 0 && conservation_failures == 0 &&
        face_active_cells > 0 && thermal_active_cells > 0 && lat_mismatches == 0) {
        std::cout << ">>> ALL MULTI-SHELL EQUIVARIANCE AND TRANSPORT TESTS PASSED WITH ZERO ERRORS! <<<\n";
        return 0;
    } else {
        std::cerr << ">>> MULTI-SHELL VERIFICATION FAILED! <<<\n";
        return 1;
    }
}
