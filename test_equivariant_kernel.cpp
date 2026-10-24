#include <iostream>
#include <vector>
#include <array>
#include <map>
#include <set>
#include <algorithm>
#include <cstdint>

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

static vector<Gate> make_gates(const array<Vel, K>& v) {
    using PairKey = std::tuple<int, int, int, int>;
    std::map<PairKey, vector<array<int, 2>>> pair_map;
    for (int i = 0; i < K; ++i)
        for (int j = i + 1; j < K; ++j) {
            auto a = v[i], b = v[j];
            pair_map[{a.x + b.x, a.y + b.y, a.z + b.z, a.e + b.e}].push_back({i, j});
        }

    std::map<array<int, 4>, bool> unique_quads;
    for (const auto& kv : pair_map) {
        const auto& pairs = kv.second;
        for (size_t i = 0; i < pairs.size(); ++i)
            for (size_t j = i + 1; j < pairs.size(); ++j) {
                auto p1 = pairs[i];
                auto p2 = pairs[j];
                array<int, 4> q = {p1[0], p1[1], p2[0], p2[1]};
                std::sort(q.begin(), q.end());
                if (std::adjacent_find(q.begin(), q.end()) == q.end())
                    unique_quads[q] = true;
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
            for (int code : c_entry.second) if (code != 40) codes.push_back(code);
            for (size_t j = 0; j + 1 < codes.size(); j += 2) {
                int c1 = codes[j]; int c2 = codes[j + 1];
                g.lut[c1] = static_cast<u8>(c2); g.lut[c2] = static_cast<u8>(c1);
            }
        }
        bool active = false;
        for (int mask = 0; mask < 81; ++mask) if (g.lut[mask] != mask) { active = true; break; }
        if (active) gates.push_back(g);
    }
    return gates;
}

int main() {
    auto v = make_vels();
    auto gates = make_gates(v);
    std::cout << "Successfully generated " << gates.size() << " collision gates.\n";
    return 0;
}
