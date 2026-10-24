#include <iostream>
#include <vector>
#include <array>
#include <map>
#include <set>
#include <algorithm>
#include <cstdint>
#include <random>

using std::array; using std::vector;
using u8 = std::uint8_t;
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

struct Mat3 { int m[3][3]; };
static vector<Mat3> make_Oh() {
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
    array<u8, K> rot{};
    for (int k = 0; k < K; ++k) {
        int rk = rotate_vel(k, M, v);
        rot[rk] = s[k];
    }
    return rot;
}

static void apply_gate(array<u8, K>& s, const Gate& g) {
    int in_mask = encode_mask(s[g.a], s[g.b], s[g.c], s[g.d]);
    int out_mask = g.lut[in_mask];
    auto dec = decode_mask(out_mask);
    s[g.a] = static_cast<u8>(dec[0]);
    s[g.b] = static_cast<u8>(dec[1]);
    s[g.c] = static_cast<u8>(dec[2]);
    s[g.d] = static_cast<u8>(dec[3]);
}

int main() {
    auto v = make_vels();
    auto Oh = make_Oh();

    // The 3 mutually commuting, disjoint gates of the size-3 orbit:
    // Gate 1 (xy edge plane): [1, 7, 19, 25]
    // Gate 2 (xz edge plane): [3, 5, 21, 23]
    // Gate 3 (yz edge plane): [9, 11, 15, 17]
    Gate g1 = build_gate(1, 7, 19, 25, v);
    Gate g2 = build_gate(3, 5, 21, 23, v);
    Gate g3 = build_gate(9, 11, 15, 17, v);

    auto C_equiv = [&](array<u8, K>& s) {
        apply_gate(s, g1);
        apply_gate(s, g2);
        apply_gate(s, g3);
    };

    std::cout << "Testing Commuting Disjoint-Orbit Operator C_equiv across ALL 48 Oh operations...\n";

    std::mt19937_64 rng(1337);
    int total_tests = 1000;
    int total_mismatches = 0;
    int active_collisions = 0;

    for (int t = 0; t < total_tests; ++t) {
        array<u8, K> s{};
        for (int k = 0; k < K; ++k) s[k] = rng() % 3;

        array<u8, K> s_after = s;
        C_equiv(s_after);
        if (s_after != s) active_collisions++;

        for (const auto& M : Oh) {
            // Path 1: C(R s)
            array<u8, K> Rs = rotate_state(s, M, v);
            array<u8, K> C_Rs = Rs;
            C_equiv(C_Rs);

            // Path 2: R(C s)
            array<u8, K> Cs = s;
            C_equiv(Cs);
            array<u8, K> R_Cs = rotate_state(Cs, M, v);

            if (C_Rs != R_Cs) {
                total_mismatches++;
            }
        }
    }

    std::cout << "Tests run: " << total_tests << " random states x 48 Oh rotations = "
              << total_tests * 48 << " evaluations\n";
    std::cout << "Active collisions occurred in: " << active_collisions << " / " << total_tests << " states\n";
    std::cout << "Total Equivariance Mismatches: " << total_mismatches << "\n";
    if (total_mismatches == 0) {
        std::cout << ">>> 100% EXACT ROTATIONAL EQUIVARIANCE CONFIRMED: C(Rs) == R(Cs) across ALL 48 Oh operations! <<<\n";
    }
    return (total_mismatches == 0) ? 0 : 1;
}
