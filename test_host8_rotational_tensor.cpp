#include <iostream>
#include <iomanip>
#include <vector>
#include <array>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <numeric>
#include <string>
#include <algorithm>

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

static bool load_host8_rooms(const string& path, u64 offset_stubs, size_t num_rooms, vector<u8>& state) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    file.seekg(offset_stubs * 8);
    vector<u64> stubs(num_rooms);
    file.read(reinterpret_cast<char*>(stubs.data()), num_rooms * 8);
    if (!file) return false;

    state.resize(num_rooms * K);
    for (size_t cell = 0; cell < num_rooms; ++cell) {
        u64 val = stubs[cell];
        for (int k = 0; k < K; ++k) {
            state[cell * K + k] = static_cast<u8>(val % 3);
            val /= 3;
        }
    }
    return true;
}

struct TensorMoments {
    double m2_xx, m2_yy, m2_zz, m2_xy;
    double m4_xxxx, m4_yyyy, m4_zzzz;
    double m4_xxyy, m4_xxzz, m4_yyzz;
    double dir_axis;
    double dir_face_diag;
    double dir_body_diag;
    double entropy;
    double mean_density;
};

static TensorMoments compute_moments(const vector<u8>& state, size_t num_rooms, const array<Velocity, K>& v) {
    double s2_xx = 0, s2_yy = 0, s2_zz = 0, s2_xy = 0;
    double s4_xxxx = 0, s4_yyyy = 0, s4_zzzz = 0;
    double s4_xxyy = 0, s4_xxzz = 0, s4_yyzz = 0;
    double total_mass = 0;
    array<u64, 3> trit_counts = {0, 0, 0};

    for (size_t cell = 0; cell < num_rooms; ++cell) {
        int base = cell * K;
        for (int k = 0; k < K; ++k) {
            int n = state[base + k];
            trit_counts[n]++;
            total_mass += n;

            double vx = v[k].x;
            double vy = v[k].y;
            double vz = v[k].z;

            s2_xx += n * vx * vx;
            s2_yy += n * vy * vy;
            s2_zz += n * vz * vz;
            s2_xy += n * vx * vy;

            s4_xxxx += n * vx * vx * vx * vx;
            s4_yyyy += n * vy * vy * vy * vy;
            s4_zzzz += n * vz * vz * vz * vz;

            s4_xxyy += n * vx * vx * vy * vy;
            s4_xxzz += n * vx * vx * vz * vz;
            s4_yyzz += n * vy * vy * vz * vz;
        }
    }

    double m2_xx = s2_xx / total_mass;
    double m2_yy = s2_yy / total_mass;
    double m2_zz = s2_zz / total_mass;
    double m2_xy = s2_xy / total_mass;

    double m4_xxxx = s4_xxxx / total_mass;
    double m4_yyyy = s4_yyyy / total_mass;
    double m4_zzzz = s4_zzzz / total_mass;

    double m4_xxyy = s4_xxyy / total_mass;
    double m4_xxzz = s4_xxzz / total_mass;
    double m4_yyzz = s4_yyzz / total_mass;

    // Directional fourth moments:
    // Axis: n = (1, 0, 0) -> < (v . n)^4 > = < v_x^4 >
    double dir_axis = m4_xxxx;
    // Face diagonal: n = (1, 1, 0) / sqrt(2) -> < (v . n)^4 > = (1/4) [ <vx^4> + <vy^4> + 6 <vx^2 vy^2> ]
    double dir_face_diag = 0.25 * (m4_xxxx + m4_yyyy + 6.0 * m4_xxyy);
    // Body diagonal: n = (1, 1, 1) / sqrt(3) -> < (v . n)^4 > = (1/9) [ sum <vx^4> + 6 sum <vx^2 vy^2> ]
    double dir_body_diag = (1.0 / 9.0) * (m4_xxxx + m4_yyyy + m4_zzzz + 6.0 * (m4_xxyy + m4_xxzz + m4_yyzz));

    u64 total_trits = num_rooms * K;
    double entropy = 0.0;
    for (int t = 0; t < 3; ++t) {
        if (trit_counts[t] > 0) {
            double p = static_cast<double>(trit_counts[t]) / total_trits;
            entropy -= p * std::log(p);
        }
    }

    double mean_density = total_mass / num_rooms;
    return {m2_xx, m2_yy, m2_zz, m2_xy, m4_xxxx, m4_yyyy, m4_zzzz, m4_xxyy, m4_xxzz, m4_yyzz,
            dir_axis, dir_face_diag, dir_body_diag, entropy, mean_density};
}

int main() {
    auto v = make_velocities();
    string dir = "D:/ASOLARIA-HOST8-ROOMS-300G";

    std::cout << "=======================================================================\n";
    std::cout << " ASOLARIA 300 GB HOST8 PROCESS ROOM ROTATIONAL TENSOR INVESTIGATION\n";
    std::cout << " Measuring Exact Rational Velocity Moments Across Millions of Rooms\n";
    std::cout << " Volume: " << dir << " (375 Sectors x 800 MB = 300 GB)\n";
    std::cout << "=======================================================================\n";

    vector<int> sectors = {0, 187, 374};
    // Let's sample 100,000 process rooms (2,700,000 trits) per sector
    size_t sample_rooms = 100000;

    for (int sec : sectors) {
        std::ostringstream ss;
        ss << dir << "/sector-" << std::setw(3) << std::setfill('0') << sec << ".rooms";
        string path = ss.str();

        vector<u8> state;
        bool loaded = load_host8_rooms(path, 0, sample_rooms, state);
        if (!loaded) {
            std::cout << "[SKIP] Could not open " << path << "\n";
            continue;
        }

        std::cout << "\n>>> Probing Sector " << std::setw(3) << std::setfill('0') << sec
                  << " (" << sample_rooms << " rooms, " << sample_rooms * K << " trits) <<<\n";

        TensorMoments m = compute_moments(state, sample_rooms, v);

        std::cout << std::fixed << std::setprecision(6);
        std::cout << "  - Mean Density / Room  : " << m.mean_density << " particles\n";
        std::cout << "  - Shannon Trit Entropy : " << m.entropy << " (Theoretical Max: " << std::log(3.0) << ")\n";
        std::cout << "\n  --- Second-Order Moments (Acoustic Metric) ---\n";
        std::cout << "    <c_x^2> = " << m.m2_xx << " | <c_y^2> = " << m.m2_yy << " | <c_z^2> = " << m.m2_zz << "\n";
        std::cout << "    <c_x c_y> = " << m.m2_xy << " (Cross-diagonal shear)\n";
        double m2_mean = (m.m2_xx + m.m2_yy + m.m2_zz) / 3.0;
        double m2_aniso = (std::max({m.m2_xx, m.m2_yy, m.m2_zz}) - std::min({m.m2_xx, m.m2_yy, m.m2_zz})) / m2_mean * 100.0;
        std::cout << "    ===> Second-Order Moment Anisotropy: " << std::setprecision(4) << m2_aniso << "% (Exact Theory: 2/3 = 0.666667)\n";

        std::cout << "\n  --- Fourth-Order Directional Moments ---\n";
        std::cout << "    Axis [1, 0, 0]         < (c . n)^4 > : " << m.dir_axis << " (Theory: 2/3 = 0.666667)\n";
        std::cout << "    Face Diag [1, 1, 0]    < (c . n)^4 > : " << m.dir_face_diag << " (Theory: 1.000000)\n";
        std::cout << "    Body Diag [1, 1, 1]    < (c . n)^4 > : " << m.dir_body_diag << " (Theory: 10/9 = 1.111111)\n";
        std::cout << "    Ratio Body / Axis                     : " << (m.dir_body_diag / m.dir_axis) << " (Theory: 5/3 = 1.666667)\n";

        std::cout << "\n  --- Fourth-Order Isotropy Check: <c_x^4> vs 3 * <c_x^2 c_y^2> ---\n";
        std::cout << "    <c_x^4>             = " << m.m4_xxxx << "\n";
        std::cout << "    3 * <c_x^2 c_y^2>   = " << (3.0 * m.m4_xxyy) << "\n";
        std::cout << "    Cubic Deficit Delta = " << (m.m4_xxxx - 3.0 * m.m4_xxyy) << " (Theory: -2/3 = -0.666667)\n";
    }

    return 0;
}
