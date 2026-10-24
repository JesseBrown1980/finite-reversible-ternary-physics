#include <iostream>
#include <vector>
#include <array>
#include <map>
#include <set>
#include <algorithm>
#include <cstdint>
#include <random>
#include <cassert>
#include <iomanip>
#include <fstream>
#include "face_lut.h"

using std::array; using std::vector; using std::map;
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
    int code = s[22] + 3 * (s[4] + 3 * (s[16] + 3 * (s[10] + 3 * (s[14] + 3 * s[12]))));
    uint16_t out = FACE_LUT[code];
    s[22] = out % 3; out /= 3;
    s[4]  = out % 3; out /= 3;
    s[16] = out % 3; out /= 3;
    s[10] = out % 3; out /= 3;
    s[14] = out % 3; out /= 3;
    s[12] = out;
}

// ----------------------------------------------------------------------------
// Inter-Shell Cross-Coupling Operators
// ----------------------------------------------------------------------------
struct Quad { int c, f, e1, e2; };

struct InterShellData {
    int rest_idx;
    vector<int> edge_indices;
    map<int, array<int, 2>> edge_face_map;
    vector<Quad> corner_quads;
};

static InterShellData init_intershell_data(const array<Vel, K>& v) {
    InterShellData d;
    d.rest_idx = 13;
    for (int i = 0; i < K; ++i) {
        if (v[i].e == 2) d.edge_indices.push_back(i);
    }
    for (int e : d.edge_indices) {
        for (int f1 = 0; f1 < K; ++f1) {
            if (v[f1].e == 1) {
                int rem_x = v[e].x - v[f1].x;
                int rem_y = v[e].y - v[f1].y;
                int rem_z = v[e].z - v[f1].z;
                for (int f2 = f1 + 1; f2 < K; ++f2) {
                    if (v[f2].e == 1 && v[f2].x == rem_x && v[f2].y == rem_y && v[f2].z == rem_z) {
                        d.edge_face_map[e] = {f1, f2};
                        break;
                    }
                }
            }
        }
    }

    // Corner quads: Corner (3) + Face (1) <-> Edge (2) + Edge (2)
    for (int c = 0; c < K; ++c) {
        if (v[c].e == 3) {
            for (int f = 0; f < K; ++f) {
                if (v[f].e == 1) {
                    int tx = v[c].x + v[f].x;
                    int ty = v[c].y + v[f].y;
                    int tz = v[c].z + v[f].z;
                    for (int e1 = 0; e1 < K; ++e1) {
                        if (v[e1].e == 2) {
                            int rem_x = tx - v[e1].x;
                            int rem_y = ty - v[e1].y;
                            int rem_z = tz - v[e1].z;
                            for (int e2 = e1 + 1; e2 < K; ++e2) {
                                if (v[e2].e == 2 && v[e2].x == rem_x && v[e2].y == rem_y && v[e2].z == rem_z) {
                                    d.corner_quads.push_back({c, f, e1, e2});
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return d;
}

static void op_rest_edge_face(u8* s, const InterShellData& d) {
    int r = s[d.rest_idx];
    if (r == 2) {
        int cand_e = -1;
        int cand_count = 0;
        for (int e : d.edge_indices) {
            const auto& p = d.edge_face_map.at(e);
            if (s[e] == 2 && s[p[0]] == 0 && s[p[1]] == 0) {
                cand_e = e;
                cand_count++;
            }
        }
        if (cand_count == 1) {
            const auto& p = d.edge_face_map.at(cand_e);
            array<u8, K> sc;
            for (int k = 0; k < K; ++k) sc[k] = s[k];
            sc[d.rest_idx] = 1; sc[cand_e] = 1; sc[p[0]] = 1; sc[p[1]] = 1;
            int bwd_count = 0; int bwd_e = -1;
            for (int eb : d.edge_indices) {
                const auto& pb = d.edge_face_map.at(eb);
                if (sc[eb] == 1 && sc[pb[0]] == 1 && sc[pb[1]] == 1) {
                    bwd_e = eb;
                    bwd_count++;
                }
            }
            if (bwd_count == 1 && bwd_e == cand_e) {
                s[d.rest_idx] = 1; s[cand_e] = 1; s[p[0]] = 1; s[p[1]] = 1;
            }
        }
    } else if (r == 1) {
        int cand_bwd = -1;
        int bwd_count = 0;
        for (int eb : d.edge_indices) {
            const auto& pb = d.edge_face_map.at(eb);
            if (s[eb] == 1 && s[pb[0]] == 1 && s[pb[1]] == 1) {
                cand_bwd = eb;
                bwd_count++;
            }
        }
        if (bwd_count == 1) {
            const auto& pb = d.edge_face_map.at(cand_bwd);
            array<u8, K> sc;
            for (int k = 0; k < K; ++k) sc[k] = s[k];
            sc[d.rest_idx] = 2; sc[cand_bwd] = 2; sc[pb[0]] = 0; sc[pb[1]] = 0;
            int fwd_count = 0; int fwd_e = -1;
            for (int ef : d.edge_indices) {
                const auto& pf = d.edge_face_map.at(ef);
                if (sc[ef] == 2 && sc[pf[0]] == 0 && sc[pf[1]] == 0) {
                    fwd_e = ef;
                    fwd_count++;
                }
            }
            if (fwd_count == 1 && fwd_e == cand_bwd) {
                s[d.rest_idx] = 2; s[cand_bwd] = 2; s[pb[0]] = 0; s[pb[1]] = 0;
            }
        }
    }
}

static void op_corner_face_edge(u8* s, const InterShellData& d) {
    int cand_fwd = -1;
    int fwd_count = 0;
    for (size_t qi = 0; qi < d.corner_quads.size(); ++qi) {
        const auto& q = d.corner_quads[qi];
        if (s[q.c] == 2 && s[q.f] == 2 && s[q.e1] == 0 && s[q.e2] == 0) {
            cand_fwd = static_cast<int>(qi);
            fwd_count++;
        }
    }
    if (fwd_count == 1) {
        const auto& q = d.corner_quads[cand_fwd];
        array<u8, K> sc;
        for (int k = 0; k < K; ++k) sc[k] = s[k];
        sc[q.c] = 1; sc[q.f] = 1; sc[q.e1] = 1; sc[q.e2] = 1;
        int bwd_count = 0; int bwd_qi = -1;
        for (size_t qi = 0; qi < d.corner_quads.size(); ++qi) {
            const auto& q2 = d.corner_quads[qi];
            if (sc[q2.c] == 1 && sc[q2.f] == 1 && sc[q2.e1] == 1 && sc[q2.e2] == 1) {
                bwd_qi = static_cast<int>(qi);
                bwd_count++;
            }
        }
        if (bwd_count == 1 && bwd_qi == cand_fwd) {
            s[q.c] = 1; s[q.f] = 1; s[q.e1] = 1; s[q.e2] = 1;
            return;
        }
    }

    int cand_bwd = -1;
    int bwd_count = 0;
    for (size_t qi = 0; qi < d.corner_quads.size(); ++qi) {
        const auto& q = d.corner_quads[qi];
        if (s[q.c] == 1 && s[q.f] == 1 && s[q.e1] == 1 && s[q.e2] == 1) {
            cand_bwd = static_cast<int>(qi);
            bwd_count++;
        }
    }
    if (bwd_count == 1) {
        const auto& q = d.corner_quads[cand_bwd];
        array<u8, K> sc;
        for (int k = 0; k < K; ++k) sc[k] = s[k];
        sc[q.c] = 2; sc[q.f] = 2; sc[q.e1] = 0; sc[q.e2] = 0;
        int fwd_count2 = 0; int fwd_qi = -1;
        for (size_t qi = 0; qi < d.corner_quads.size(); ++qi) {
            const auto& q2 = d.corner_quads[qi];
            if (sc[q2.c] == 2 && sc[q2.f] == 2 && sc[q2.e1] == 0 && sc[q2.e2] == 0) {
                fwd_qi = static_cast<int>(qi);
                fwd_count2++;
            }
        }
        if (fwd_count2 == 1 && fwd_qi == cand_bwd) {
            s[q.c] = 2; s[q.f] = 2; s[q.e1] = 0; s[q.e2] = 0;
            return;
        }
    }
}

static void apply_full_symmetric_collision(u8* s, const vector<Gate>& edge_gates, const InterShellData& d) {
    // 1. Intra-shell collision stage (Edge + Face)
    for (const auto& g : edge_gates) apply_gate(s, g);
    apply_face_lut(s);

    // 2. Inter-shell cross-coupling stage (Rest-Edge-Face & Corner-Face-Edge)
    op_rest_edge_face(s, d);
    op_corner_face_edge(s, d);
}

static void apply_full_symmetric_collision_backward(u8* s, const vector<Gate>& edge_gates, const InterShellData& d) {
    // Exact inverse: reverse sequence
    op_corner_face_edge(s, d);
    op_rest_edge_face(s, d);
    apply_face_lut(s);
    for (const auto& g : edge_gates) apply_gate(s, g);
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

static void step_lattice_full(vector<u8>& state, vector<u8>& next_state,
                             const vector<Gate>& edge_gates, const InterShellData& d,
                             const array<Vel, K>& v, int L) {
    int V = L * L * L;
    for (int cell = 0; cell < V; ++cell) {
        apply_full_symmetric_collision(&state[cell * K], edge_gates, d);
    }
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
    std::cout << " Full-Symmetric Multi-Shell & Inter-Shell Collision Verification   \n";
    std::cout << "====================================================================\n\n";

    auto v = make_vels();
    auto Oh = make_full_Oh();
    auto d = init_intershell_data(v);

    vector<Gate> edge_gates = {
        build_gate(1, 7, 19, 25, v),
        build_gate(3, 5, 21, 23, v),
        build_gate(9, 11, 15, 17, v)
    };

    // -------------------------------------------------------------------------
    // 1. Single-Cell Equivariance & Involution across ALL 48 Oh operations
    // -------------------------------------------------------------------------
    std::cout << "[Test 1] Single-Cell Equivariance & Involution (5,000 states x 48 Oh ops)...\n";
    std::mt19937_64 rng(88937ULL);
    std::uniform_int_distribution<int> trit_dist(0, 2);

    int num_random = 5000;
    int local_mismatches = 0;
    int involution_failures = 0;
    int conservation_failures = 0;
    int active_collisions = 0;

    for (int trial = 0; trial < num_random; ++trial) {
        array<u8, K> s{};
        int m0 = 0, px0 = 0, py0 = 0, pz0 = 0, e0 = 0;
        for (int k = 0; k < K; ++k) {
            s[k] = static_cast<u8>(trit_dist(rng));
            m0 += s[k]; px0 += s[k] * v[k].x; py0 += s[k] * v[k].y; pz0 += s[k] * v[k].z; e0 += s[k] * v[k].e;
        }

        array<u8, K> s_col = s;
        apply_full_symmetric_collision(s_col.data(), edge_gates, d);
        if (s_col != s) active_collisions++;

        int m1 = 0, px1 = 0, py1 = 0, pz1 = 0, e1 = 0;
        for (int k = 0; k < K; ++k) {
            m1 += s_col[k]; px1 += s_col[k] * v[k].x; py1 += s_col[k] * v[k].y; pz1 += s_col[k] * v[k].z; e1 += s_col[k] * v[k].e;
        }
        if (m0 != m1 || px0 != px1 || py0 != py1 || pz0 != pz1 || e0 != e1) conservation_failures++;

        array<u8, K> s_rev = s_col;
        apply_full_symmetric_collision_backward(s_rev.data(), edge_gates, d);
        if (s_rev != s) involution_failures++;

        for (const auto& M : Oh) {
            auto Rs = rotate_state(s, M, v);
            apply_full_symmetric_collision(Rs.data(), edge_gates, d);
            auto R_Cs = rotate_state(s_col, M, v);
            if (Rs != R_Cs) local_mismatches++;
        }
    }

    std::cout << "  - Random Microstates   : " << num_random << "\n";
    std::cout << "  - Active Collision Rate: " << active_collisions << " / " << num_random 
              << " (" << std::fixed << std::setprecision(2) << (100.0 * active_collisions / num_random) << "%)\n";
    std::cout << "  - Invariant Failures   : " << conservation_failures << "\n";
    std::cout << "  - Involution Failures  : " << involution_failures << "\n";
    std::cout << "  - Oh Equivariance Chks : " << (num_random * 48) << " operations\n";
    std::cout << "  - Equivariance Mismatch: " << local_mismatches << "\n";
    assert(conservation_failures == 0);
    assert(involution_failures == 0);
    assert(local_mismatches == 0);

    // -------------------------------------------------------------------------
    // 2. Inter-Shell Particle & Energy Redistribution (48 Ticks on L=24^3)
    // -------------------------------------------------------------------------
    std::cout << "\n[Test 2] Inter-Shell Redistribution Verification (48 Ticks, L=24)...\n";
    int L = 24;
    int V = L * L * L;
    vector<u8> state(V * K);
    vector<u8> next_state(V * K);
    for (size_t i = 0; i < state.size(); ++i) state[i] = static_cast<u8>(trit_dist(rng));

    auto get_shell_totals = [&](const vector<u8>& st) {
        array<long long, 4> totals{};
        for (int c = 0; c < V; ++c) {
            for (int k = 0; k < K; ++k) {
                totals[v[k].e] += st[c * K + k];
            }
        }
        return totals;
    };

    auto shell_0 = get_shell_totals(state);
    std::cout << "  - Initial Shell Occupancies: Rest=" << shell_0[0]
              << ", Face=" << shell_0[1] << ", Edge=" << shell_0[2] << ", Corner=" << shell_0[3] << "\n";

    for (int t = 0; t < 48; ++t) {
        step_lattice_full(state, next_state, edge_gates, d, v, L);
    }

    auto shell_48 = get_shell_totals(state);
    std::cout << "  - Final Shell (Tick 48)   : Rest=" << shell_48[0]
              << ", Face=" << shell_48[1] << ", Edge=" << shell_48[2] << ", Corner=" << shell_48[3] << "\n";

    long long d_rest   = shell_48[0] - shell_0[0];
    long long d_face   = shell_48[1] - shell_0[1];
    long long d_edge   = shell_48[2] - shell_0[2];
    long long d_corner = shell_48[3] - shell_0[3];
    long long d_mass   = d_rest + d_face + d_edge + d_corner;
    long long d_energy = 0 * d_rest + 1 * d_face + 2 * d_edge + 3 * d_corner;

    std::cout << "  - Inter-Shell Deltas       : Rest=" << d_rest << ", Face=" << d_face 
              << ", Edge=" << d_edge << ", Corner=" << d_corner << "\n";
    std::cout << "  - Total Mass Change        : " << d_mass << " (EXACT CONSERVATION)\n";
    std::cout << "  - Total Energy Change      : " << d_energy << " (EXACT CONSERVATION)\n";
    bool inter_shell_active = (d_rest != 0 || d_face != 0 || d_edge != 0 || d_corner != 0);
    std::cout << "  - Inter-Shell Exchange     : " << (inter_shell_active ? "PASS (GENUINE INTER-SHELL REDISTRIBUTION)" : "FAIL") << "\n";
    assert(d_mass == 0);
    assert(d_energy == 0);
    assert(inter_shell_active);

    // -------------------------------------------------------------------------
    // 3A. Directional Stress Relaxation (Uniform E=54, rho=27, P=(-1,-1,0))
    // -------------------------------------------------------------------------
    std::cout << "\n[Test 3A] Directional Stress Relaxation (Uniform E=54, rho=27)...\n";
    vector<u8> stress_slab(V * K, 1);
    for (int x = 0; x < L; ++x) {
        for (int y = 0; y < L; ++y) {
            for (int z = 0; z < L; ++z) {
                int c = cell_index(x, y, z, L);
                int base = c * K;
                if (x < L / 2) {
                    stress_slab[base + 22] = 2; stress_slab[base + 4] = 2;
                    stress_slab[base + 16] = 0; stress_slab[base + 10] = 0;
                    stress_slab[base + 14] = 0; stress_slab[base + 12] = 0;
                    stress_slab[base + 13] = 2; stress_slab[base + 1]  = 2;
                } else {
                    stress_slab[base + 22] = 0; stress_slab[base + 4] = 0;
                    stress_slab[base + 16] = 1; stress_slab[base + 10] = 1;
                    stress_slab[base + 14] = 1; stress_slab[base + 12] = 1;
                    stress_slab[base + 13] = 2; stress_slab[base + 1]  = 2;
                }
            }
        }
    }
    int stress_active_cells = 0;
    for (int c = 0; c < V; ++c) {
        array<u8, K> orig{};
        for (int k = 0; k < K; ++k) orig[k] = stress_slab[c * K + k];
        apply_full_symmetric_collision(&stress_slab[c * K], edge_gates, d);
        bool changed = false;
        for (int k = 0; k < K; ++k) if (stress_slab[c * K + k] != orig[k]) { changed = true; break; }
        if (changed) stress_active_cells++;
    }
    std::cout << "  - Active Stress Slab Cells : " << stress_active_cells << " / " << V 
              << " (" << (100.0 * stress_active_cells / V) << "%)\n";

    // -------------------------------------------------------------------------
    // 3B. Genuine Constant-Density Thermal Gradient (rho=27, P=(0,0,0), Delta E0 = +4.0)
    // -------------------------------------------------------------------------
    std::cout << "\n[Test 3B] Genuine Constant-Density Thermal Gradient (Delta E = +4.0, P=(0,0,0))...\n";
    vector<u8> realheat(V * K, 1);
    for (int x = 0; x < L; ++x) {
        for (int y = 0; y < L; ++y) {
            for (int z = 0; z < L; ++z) {
                int c = cell_index(x, y, z, L);
                int base = c * K;
                if (x < L / 2) {
                    realheat[base + 22] = 0; realheat[base + 4] = 0; // face -2
                    realheat[base + 1]  = 2; realheat[base + 25] = 2; // edge +2 (Delta E = +2)
                } else {
                    realheat[base + 22] = 2; realheat[base + 4] = 2; // face +2
                    realheat[base + 1]  = 0; realheat[base + 25] = 0; // edge -2 (Delta E = -2)
                }
            }
        }
    }

    std::ofstream cfile("corrected_thermal_timeseries.csv");
    cfile << "tick,mean_energy_hot,mean_energy_cold,delta_energy,mean_density_hot,mean_density_cold,net_px,net_py,net_pz\n";

    for (int t = 0; t <= 48; ++t) {
        long long eh = 0, ec = 0, nh = 0, nc = 0;
        long long px = 0, py = 0, pz = 0;
        for (int x = 0; x < L; ++x) {
            for (int y = 0; y < L; ++y) {
                for (int z = 0; z < L; ++z) {
                    int c = cell_index(x, y, z, L);
                    int mass = 0, cell_e = 0;
                    for (int k = 0; k < K; ++k) {
                        int count = realheat[c * K + k];
                        mass += count;
                        cell_e += count * v[k].e;
                        px += count * v[k].x;
                        py += count * v[k].y;
                        pz += count * v[k].z;
                    }
                    if (x < L / 2) { eh += cell_e; nh += mass; }
                    else           { ec += cell_e; nc += mass; }
                }
            }
        }
        double vol = double(L * L * L / 2);
        double mean_eh = double(eh) / vol;
        double mean_ec = double(ec) / vol;
        double delta_e = double(eh - ec) / vol;
        double mean_nh = double(nh) / vol;
        double mean_nc = double(nc) / vol;

        cfile << t << "," << mean_eh << "," << mean_ec << "," << delta_e << ","
              << mean_nh << "," << mean_nc << "," << px << "," << py << "," << pz << "\n";

        if (t == 0 || t == 1 || t == 6 || t == 12 || t == 24 || t == 48) {
            std::cout << "  - Tick " << std::setw(2) << t << " : E_hot=" << std::fixed << std::setprecision(4) << mean_eh
                      << ", E_cold=" << mean_ec << ", Delta E=" << std::setprecision(4) << delta_e 
                      << ", rho=" << std::setprecision(2) << mean_nh << " vs " << mean_nc
                      << ", net P=(" << px << "," << py << "," << pz << ")\n";
        }
        if (t < 48) {
            step_lattice_full(realheat, next_state, edge_gates, d, v, L);
        }
    }
    std::cout << "  ===> Corrected Thermal Time-Series Exported to corrected_thermal_timeseries.csv\n";

    // -------------------------------------------------------------------------
    // 4. Whole 3D Lattice Dynamic Equivariance F(Rs) == R(Fs)
    // -------------------------------------------------------------------------
    std::cout << "\n[Test 4] Whole 3D Lattice Dynamic Equivariance (L=8^3, 48 Oh ops)...\n";
    int L8 = 8;
    int V8 = L8 * L8 * L8;
    int trits8 = V8 * K;
    vector<u8> s_base(trits8);
    for (int i = 0; i < trits8; ++i) s_base[i] = static_cast<u8>(trit_dist(rng));

    int lat_mismatches = 0;
    vector<u8> scratch8(trits8);

    for (const auto& M : Oh) {
        vector<u8> state_A = rotate_full_lattice(s_base, L8, M, v);
        for (int step = 0; step < 2; ++step) {
            step_lattice_full(state_A, scratch8, edge_gates, d, v, L8);
        }

        vector<u8> state_B = s_base;
        for (int step = 0; step < 2; ++step) {
            step_lattice_full(state_B, scratch8, edge_gates, d, v, L8);
        }
        vector<u8> R_state_B = rotate_full_lattice(state_B, L8, M, v);

        for (int i = 0; i < trits8; ++i) {
            if (state_A[i] != R_state_B[i]) lat_mismatches++;
        }
    }
    std::cout << "  - Lattice Trit Comparisons : " << (trits8 * 48) << "\n";
    std::cout << "  - Dynamic Mismatches       : " << lat_mismatches << "\n";
    std::cout << "  - Whole-Lattice Equivariance: " << (lat_mismatches == 0 ? "PASS (100% Exact Dynamic Equivariance)" : "FAIL") << "\n\n";
    assert(lat_mismatches == 0);

    std::cout << ">>> ALL INTER-SHELL EQUIVARIANCE AND REDISTRIBUTION BENCHMARKS PASSED! <<<\n";
    return 0;
}
