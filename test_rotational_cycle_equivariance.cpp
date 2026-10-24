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

// 24 proper rotations of the cube (det = +1)
struct Mat3 { int m[3][3]; };

static vector<Mat3> make_proper_rotations() {
    vector<Mat3> ops;
    int p[3] = {0, 1, 2};
    do {
        for (int sx : {-1, 1}) {
            for (int sy : {-1, 1}) {
                for (int sz : {-1, 1}) {
                    int det = (p[0]==0 ? (p[1]==1 ? 1 : -1) : (p[0]==1 ? (p[1]==2 ? 1 : -1) : (p[1]==0 ? 1 : -1)));
                    det *= (sx * sy * sz);
                    if (det == 1) { // Proper rotation
                        Mat3 M{};
                        M.m[0][p[0]] = sx;
                        M.m[1][p[1]] = sy;
                        M.m[2][p[2]] = sz;
                        ops.push_back(M);
                    }
                }
            }
        }
    } while (std::next_permutation(p, p + 3));
    return ops;
}

static int rotate_vel(int k, const Mat3& M, const array<Vel, K>& v) {
    int x = v[k].x, y = v[k].y, z = v[k].z;
    int rx = M.m[0][0]*x + M.m[0][1]*y + M.m[0][2]*z;
    int ry = M.m[1][0]*x + M.m[1][1]*y + M.m[1][2]*z;
    int rz = M.m[2][0]*x + M.m[2][1]*y + M.m[2][2]*z;
    for (int i = 0; i < K; ++i) {
        if (v[i].x == rx && v[i].y == ry && v[i].z == rz) return i;
    }
    return -1;
}

int main() {
    auto v = make_vels();
    auto rots = make_proper_rotations();
    std::cout << "Proper cubic rotation group size: " << rots.size() << " operations\n";
    return 0;
}
