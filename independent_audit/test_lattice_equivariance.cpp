#include <iostream>
#include <vector>
#include <array>
#include <map>
#include <set>
#include <algorithm>
#include <cstdint>
#include <random>
#include <cassert>

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

static inline int cell_index(int x, int y, int z, int L) {
    return (x * L + y) * L + z;
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

static void step_lattice_equivariant(vector<u8>& state, vector<u8>& next_state,
                                     const vector<Gate>& commuting_gates,
                                     const array<Vel, K>& v, int L) {
    int V = L * L * L;
    // 1. Spatially uniform, commuting collision step
    for (int cell = 0; cell < V; ++cell) {
        u8* ptr = &state[cell * K];
        for (const auto& g : commuting_gates) {
            apply_gate(ptr, g);
        }
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
    state.swap(next_state);
}

int main() {
    auto v = make_vels();
    auto proper_rots = make_full_Oh();

    // Commuting Disjoint-Orbit Gates:
    // Size-3 orbit on edges: [1,7,19,25], [3,5,21,23], [9,11,15,17]
    // These 3 gates are mutually disjoint and form a closed orbit under Oh!
    vector<Gate> equiv_gates = {
        build_gate(1, 7, 19, 25, v),
        build_gate(3, 5, 21, 23, v),
        build_gate(9, 11, 15, 17, v)
    };

    std::cout << "=======================================================================\n";
    std::cout << " ASOLARIA WHOLE-LATTICE DYNAMIC ROTATIONAL EQUIVARIANCE BENCHMARK\n";
    std::cout << " Probing F(R s) == R(F s) across 24 proper cubic rotations\n";
    std::cout << "=======================================================================\n";

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
                        s0[c + 1] = 2; s0[c + 25] = 0;
                    } else if ((x + y + z) % 3 == 1) {
                        s0[c + 3] = 2; s0[c + 23] = 0;
                    } else {
                        s0[c + 9] = 2; s0[c + 17] = 0;
                    }
                }
            }
        }

        u64 total_mismatches = 0;
        int test_idx = 0;

        for (const auto& M : proper_rots) {
            // Path 1: F(R s0)
            vector<u8> Rs0 = rotate_full_lattice(s0, L, M, v);
            vector<u8> F_Rs0 = Rs0, n1(total_trits, 0);
            step_lattice_equivariant(F_Rs0, n1, equiv_gates, v, L);

            // Path 2: R(F s0)
            vector<u8> F_s0 = s0, n2(total_trits, 0);
            step_lattice_equivariant(F_s0, n2, equiv_gates, v, L);
            vector<u8> R_Fs0 = rotate_full_lattice(F_s0, L, M, v);

            u64 mismatches = 0;
            for (size_t i = 0; i < s0.size(); ++i) {
                if (F_Rs0[i] != R_Fs0[i]) mismatches++;
            }
            total_mismatches += mismatches;
            test_idx++;
        }

        std::cout << "Lattice " << L << "x" << L << "x" << L << " (" << total_trits << " trits):\n";
        std::cout << "  - Tested Rotations : " << test_idx << " proper cubic rotations\n";
        std::cout << "  - Total Mismatches : " << total_mismatches << " / " << (total_trits * test_idx) << "\n";
        std::cout << "  - Equivariance     : " << (total_mismatches == 0 ? "PASS (0 mismatches, 100% Exact F(Rs) == R(Fs))" : "FAIL") << "\n";
        if (total_mismatches != 0) return 1;
    }

    return 0;
}
