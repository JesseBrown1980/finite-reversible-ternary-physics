#include <iostream>
#include <vector>
#include <array>
#include <map>
#include <set>
#include <algorithm>

using std::array; using std::vector; using std::string;
struct Vel { int x, y, z, e; };
constexpr int K = 27;

static array<Vel, K> make_vels() {
    array<Vel, K> v{};
    int idx = 0;
    for (int x = -1; x <= 1; ++x)
        for (int y = -1; y <= 1; ++y)
            for (int z = -1; z <= 1; ++z)
                v[idx++] = {x, y, z, x*x + y*y + z*z};
    return v;
}
struct Gate { int a, b, c, d; };

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
        gates.push_back({q[0], q[1], q[2], q[3]});
    }
    return gates;
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

int main() {
    auto v = make_vels();
    auto gates = make_gates(v);
    auto Oh = make_Oh();

    std::vector<bool> visited(gates.size(), false);
    int orbit_id = 0;

    for (size_t i = 0; i < gates.size(); ++i) {
        if (visited[i]) continue;
        std::set<array<int, 4>> orbit_quads;
        for (const auto& M : Oh) {
            array<int, 4> rq = {
                rotate_vel(gates[i].a, M, v),
                rotate_vel(gates[i].b, M, v),
                rotate_vel(gates[i].c, M, v),
                rotate_vel(gates[i].d, M, v)
            };
            std::sort(rq.begin(), rq.end());
            orbit_quads.insert(rq);
        }
        for (size_t j = 0; j < gates.size(); ++j) {
            array<int, 4> q = {gates[j].a, gates[j].b, gates[j].c, gates[j].d};
            std::sort(q.begin(), q.end());
            if (orbit_quads.count(q)) visited[j] = true;
        }

        orbit_id++;
        auto rep = *orbit_quads.begin();
        std::map<int, int> shell_counts;
        for (int ch : rep) shell_counts[v[ch].e]++;

        std::cout << "Orbit " << orbit_id << " (size " << orbit_quads.size() << "): ";
        std::cout << "Energies in quad = [";
        for (int ch : rep) std::cout << v[ch].e << " ";
        std::cout << "] -> Shells: ";
        for (auto kv : shell_counts) {
            string sname = kv.first == 0 ? "rest" : kv.first == 1 ? "face" : kv.first == 2 ? "edge" : "corner";
            std::cout << kv.second << " " << sname << ", ";
        }
        bool cross_shell = (shell_counts.size() > 1);
        std::cout << (cross_shell ? "===> CROSS-SHELL COUPLING" : "single shell") << "\n";
    }
    return 0;
}
