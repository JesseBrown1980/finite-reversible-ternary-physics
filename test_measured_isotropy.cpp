#include "acoustic_damped_fit.h"
#include <iostream>
#include <vector>
#include <array>
#include <cmath>
#include <iomanip>
#include <cstdint>
#include <map>
#include <tuple>
#include <string>

using std::array;
using std::vector;
using u8 = std::uint8_t;

constexpr int K = 27;
constexpr double PI = 3.14159265358979323846;

struct Velocity { int x, y, z, e; };
struct Collision { int a, b, c, d; array<u8, 81> lut; };

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

static inline int encode(int a, int b, int c, int d) { return a + 3 * b + 9 * c + 27 * d; }
static inline array<int, 4> decode(int code) {
    array<int, 4> out{};
    for (int i = 0; i < 4; ++i) { out[i] = code % 3; code /= 3; }
    return out;
}

static vector<Collision> make_collisions(const array<Velocity, K>& v) {
    using Key = std::tuple<int, int, int, int>;
    std::map<Key, vector<array<int, 2>>> pairs;
    for (int a = 0; a < K; ++a) {
        for (int b = a + 1; b < K; ++b) {
            pairs[{v[a].x + v[b].x, v[a].y + v[b].y,
                   v[a].z + v[b].z, v[a].e + v[b].e}].push_back({a, b});
        }
    }
    std::vector<array<int, 4>> quads;
    for (const auto& entry : pairs) {
        const auto& ps = entry.second;
        for (size_t i = 0; i < ps.size(); ++i) {
            for (size_t j = i + 1; j < ps.size(); ++j) {
                array<int, 4> q = {ps[i][0], ps[i][1], ps[j][0], ps[j][1]};
                std::sort(q.begin(), q.end());
                bool distinct = true;
                for (int m = 0; m < 3; ++m) if (q[m] == q[m + 1]) distinct = false;
                if (distinct) quads.push_back(q);
            }
        }
    }
    std::sort(quads.begin(), quads.end());
    quads.erase(std::unique(quads.begin(), quads.end()), quads.end());

    constexpr int ALL_ONES_CODE = 40;
    vector<Collision> out;
    for (const auto& q : quads) {
        Collision c{q[0], q[1], q[2], q[3], {}};
        for (int mask = 0; mask < 81; ++mask) c.lut[mask] = static_cast<u8>(mask);
        std::map<std::tuple<int, int, int, int, int>, vector<int>> cls;
        for (int mask = 0; mask < 81; ++mask) {
            auto d = decode(mask);
            int m = 0, px = 0, py = 0, pz = 0, e = 0;
            for (int j = 0; j < 4; ++j) {
                m += d[j]; px += d[j] * v[q[j]].x; py += d[j] * v[q[j]].y; pz += d[j] * v[q[j]].z; e += d[j] * v[q[j]].e;
            }
            cls[{m, px, py, pz, e}].push_back(mask);
        }
        for (const auto& entry : cls) {
            vector<int> codes;
            for (int code : entry.second) if (code != ALL_ONES_CODE) codes.push_back(code);
            for (size_t j = 0; j + 1 < codes.size(); j += 2) {
                c.lut[codes[j]] = static_cast<u8>(codes[j + 1]);
                c.lut[codes[j + 1]] = static_cast<u8>(codes[j]);
            }
        }
        bool active = false;
        for (int mask = 0; mask < 81; ++mask) if (c.lut[mask] != mask) { active = true; break; }
        if (active) out.push_back(c);
    }
    return out;
}

static inline int idx3d(int x, int y, int z, int L) { return ((x * L + y) * L + z); }

static void step_universe(vector<u8>& state, vector<u8>& next_state,
                          const vector<Collision>& c, const array<Velocity, K>& v,
                          int L, int t, int gates = 32) {
    int V = L * L * L;
    const size_t total_c = c.size();
    for (int cell = 0; cell < V; ++cell) {
        int base = cell * K;
        for (int j = 0; j < gates; ++j) {
            size_t which = (static_cast<size_t>(t) * gates + static_cast<size_t>(cell) * 17 + static_cast<size_t>(j) * 37) % total_c;
            const auto& col = c[which];
            int code = encode(state[base + col.a], state[base + col.b], state[base + col.c], state[base + col.d]);
            auto dest = decode(col.lut[code]);
            state[base + col.a] = static_cast<u8>(dest[0]);
            state[base + col.b] = static_cast<u8>(dest[1]);
            state[base + col.c] = static_cast<u8>(dest[2]);
            state[base + col.d] = static_cast<u8>(dest[3]);
        }
    }
    for (int x = 0; x < L; ++x) {
        for (int y = 0; y < L; ++y) {
            for (int z = 0; z < L; ++z) {
                int src = idx3d(x, y, z, L);
                for (int ki = 0; ki < K; ++ki) {
                    int nx = (x + v[ki].x + L) % L;
                    int ny = (y + v[ki].y + L) % L;
                    int nz = (z + v[ki].z + L) % L;
                    next_state[idx3d(nx, ny, nz, L) * K + ki] = state[src * K + ki];
                }
            }
        }
    }
    state = next_state;
}

int main() {
    int L = 24;
    int V = L * L * L;
    auto v = make_velocities();
    auto col = make_collisions(v);

    struct Probe {
        std::string name;
        int dx, dy, dz;
        int ch_pos, ch_neg;
        double k_mag;
    };

    // Find opposite channels for each direction
    int fx_p = -1, fx_m = -1;
    int fy_p = -1, fy_m = -1;
    int fz_p = -1, fz_m = -1;
    int diag_p = -1, diag_m = -1;
    for (int i = 0; i < K; ++i) {
        if (v[i].x ==  1 && v[i].y ==  0 && v[i].z ==  0) fx_p = i;
        if (v[i].x == -1 && v[i].y ==  0 && v[i].z ==  0) fx_m = i;
        if (v[i].x ==  0 && v[i].y ==  1 && v[i].z ==  0) fy_p = i;
        if (v[i].x ==  0 && v[i].y == -1 && v[i].z ==  0) fy_m = i;
        if (v[i].x ==  0 && v[i].y ==  0 && v[i].z ==  1) fz_p = i;
        if (v[i].x ==  0 && v[i].y ==  0 && v[i].z == -1) fz_m = i;
        if (v[i].x ==  1 && v[i].y ==  1 && v[i].z ==  1) diag_p = i;
        if (v[i].x == -1 && v[i].y == -1 && v[i].z == -1) diag_m = i;
    }

    double kv = 2.0 * PI / L;
    vector<Probe> probes = {
        {"X-Axis  (1, 0, 0)", 1, 0, 0, fx_p, fx_m, kv},
        {"Y-Axis  (0, 1, 0)", 0, 1, 0, fy_p, fy_m, kv},
        {"Z-Axis  (0, 0, 1)", 0, 0, 1, fz_p, fz_m, kv},
        {"Diagonal(1, 1, 1)", 1, 1, 1, diag_p, diag_m, std::sqrt(3.0) * kv}
    };

    const int MAX_TICKS = 60;

    for (const auto& probe : probes) {
        // Paired differential measurement: control (unperturbed) vs perturbed
        vector<u8> ctrl(V * K, 1);
        vector<u8> pert(V * K, 1);
        vector<u8> next_state(V * K, 0);

        for (int x = 0; x < L; ++x) {
            for (int y = 0; y < L; ++y) {
                for (int z = 0; z < L; ++z) {
                    double phase = kv * (probe.dx * (x + 0.5) + probe.dy * (y + 0.5) + probe.dz * (z + 0.5));
                    double cos_val = std::cos(phase);
                    int cell = idx3d(x, y, z, L);
                    if (cos_val > 0.3) {
                        pert[cell * K + probe.ch_pos] = 2;
                        pert[cell * K + probe.ch_neg] = 2;
                    } else if (cos_val < -0.3) {
                        pert[cell * K + probe.ch_pos] = 0;
                        pert[cell * K + probe.ch_neg] = 0;
                    }
                }
            }
        }

        vector<double> t_vec;
        vector<double> delta_amps;

        for (int t = 0; t <= MAX_TICKS; ++t) {
            double A_pert = 0.0, A_ctrl = 0.0;
            for (int x = 0; x < L; ++x) {
                for (int y = 0; y < L; ++y) {
                    for (int z = 0; z < L; ++z) {
                        double phase = kv * (probe.dx * (x + 0.5) + probe.dy * (y + 0.5) + probe.dz * (z + 0.5));
                        double cos_val = std::cos(phase);
                        int cell = idx3d(x, y, z, L);
                        int rho_p = 0, rho_c = 0;
                        for (int k = 0; k < K; ++k) {
                            rho_p += pert[cell * K + k];
                            rho_c += ctrl[cell * K + k];
                        }
                        A_pert += rho_p * cos_val;
                        A_ctrl += rho_c * cos_val;
                    }
                }
            }
            t_vec.push_back(static_cast<double>(t));
            delta_amps.push_back(A_pert - A_ctrl);

            if (t == MAX_TICKS) break;
            step_universe(ctrl, next_state, col, v, L, t);
            step_universe(pert, next_state, col, v, L, t);
        }

        const auto fit = acoustic::fit(t_vec, delta_amps);
        double measured_cs = fit.valid ? (fit.omega / probe.k_mag) : 0.0;
        double theory_cs = std::sqrt(2.0 / 3.0);
        double err_pct = std::abs(measured_cs - theory_cs) / theory_cs * 100.0;

        std::cout << probe.name << " | Fit Valid: " << (fit.valid ? "YES" : "NO")
                  << " | omega: " << std::fixed << std::setprecision(5) << fit.omega
                  << " | k: " << probe.k_mag
                  << " | Measured c_s: " << std::setprecision(5) << measured_cs
                  << " | Theory c_s: " << theory_cs
                  << " | Error: " << std::setprecision(2) << err_pct << "%"
                  << " | R^2: " << std::setprecision(4) << fit.r_squared
                  << " | Cycles: " << std::setprecision(2) << fit.cycles << "\n";
    }
    return 0;
}
