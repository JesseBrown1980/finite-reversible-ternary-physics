#include <iostream>
#include <fstream>
#include <vector>
#include <array>
#include <string>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <map>
#include <tuple>

using u8 = uint8_t;
using u32 = uint32_t;
using u64 = uint64_t;
using std::vector;
using std::array;
using std::string;

namespace asolaria {
class SHA256 {
public:
    static string hash_bytes(const uint8_t* data, size_t len) {
        uint32_t h[8] = {
            0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
            0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
        };
        const uint32_t k[64] = {
            0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
            0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
            0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
            0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
            0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
            0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
            0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
            0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
        };

        size_t new_len = (len + 9 + 63) / 64 * 64;
        vector<uint8_t> p(new_len, 0);
        for (size_t i = 0; i < len; ++i) p[i] = data[i];
        p[len] = 0x80;
        uint64_t bits = static_cast<uint64_t>(len) * 8;
        for (int i = 0; i < 8; ++i) p[new_len - 1 - i] = static_cast<uint8_t>(bits >> (i * 8));

        auto rotr = [](uint32_t x, uint32_t n) { return (x >> n) | (x << (32 - n)); };

        for (size_t chunk = 0; chunk < new_len; chunk += 64) {
            uint32_t w[64];
            for (int i = 0; i < 16; ++i) {
                w[i] = (static_cast<uint32_t>(p[chunk + i * 4]) << 24) |
                       (static_cast<uint32_t>(p[chunk + i * 4 + 1]) << 16) |
                       (static_cast<uint32_t>(p[chunk + i * 4 + 2]) << 8) |
                       (static_cast<uint32_t>(p[chunk + i * 4 + 3]));
            }
            for (int i = 16; i < 64; ++i) {
                uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
                uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
                w[i] = w[i - 16] + s0 + w[i - 7] + s1;
            }
            uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
            uint32_t e = h[4], f = h[5], g = h[6], h_val = h[7];
            for (int i = 0; i < 64; ++i) {
                uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
                uint32_t ch = (e & f) ^ ((~e) & g);
                uint32_t temp1 = h_val + S1 + ch + k[i] + w[i];
                uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
                uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
                uint32_t temp2 = S0 + maj;
                h_val = g; g = f; f = e; e = d + temp1;
                d = c; c = b; b = a; a = temp1 + temp2;
            }
            h[0] += a; h[1] += b; h[2] += c; h[3] += d;
            h[4] += e; h[5] += f; h[6] += g; h[7] += h_val;
        }

        std::ostringstream ss;
        for (int i = 0; i < 8; ++i) ss << std::hex << std::setw(8) << std::setfill('0') << h[i];
        return ss.str();
    }
};
}

constexpr int K = 27;
constexpr int L = 24;
constexpr int V = L * L * L; // 13,824
constexpr int N = V * K; // 373,248
constexpr int GATES_PER_CELL = 32;

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

static inline int encode(int a, int b, int c, int d) {
    return a + 3 * b + 9 * c + 27 * d;
}

static inline array<int, 4> decode(int code) {
    return {code % 3, (code / 3) % 3, (code / 9) % 3, (code / 27) % 3};
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
                for (int m = 0; m < 3; ++m) {
                    if (q[m] == q[m + 1]) distinct = false;
                }
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
                m += d[j];
                px += d[j] * v[q[j]].x;
                py += d[j] * v[q[j]].y;
                pz += d[j] * v[q[j]].z;
                e  += d[j] * v[q[j]].e;
            }
            cls[{m, px, py, pz, e}].push_back(mask);
        }

        for (const auto& entry : cls) {
            vector<int> codes;
            for (int code : entry.second) {
                if (code != ALL_ONES_CODE) codes.push_back(code);
            }
            for (size_t j = 0; j + 1 < codes.size(); j += 2) {
                c.lut[codes[j]] = static_cast<u8>(codes[j + 1]);
                c.lut[codes[j + 1]] = static_cast<u8>(codes[j]);
            }
        }
        bool active = false;
        for (int mask = 0; mask < 81; ++mask) {
            if (c.lut[mask] != mask) { active = true; break; }
        }
        if (active) out.push_back(c);
    }
    return out;
}

static inline int idx3d(int x, int y, int z) {
    return ((x * L + y) * L + z);
}

static void step_universe(vector<u8>& state, vector<u8>& next_state,
                          const vector<Collision>& c, const array<Velocity, K>& v,
                          int t) {
    const size_t total_c = c.size();

    // 1. Involutive Collision Stage
    for (int cell = 0; cell < V; ++cell) {
        int base = cell * K;
        for (int j = 0; j < GATES_PER_CELL; ++j) {
            size_t which = (static_cast<size_t>(t) * GATES_PER_CELL +
                            static_cast<size_t>(cell) * 17 +
                            static_cast<size_t>(j) * 37) % total_c;
            const auto& col = c[which];
            int code = encode(state[base + col.a], state[base + col.b], state[base + col.c], state[base + col.d]);
            auto dest = decode(col.lut[code]);
            state[base + col.a] = static_cast<u8>(dest[0]);
            state[base + col.b] = static_cast<u8>(dest[1]);
            state[base + col.c] = static_cast<u8>(dest[2]);
            state[base + col.d] = static_cast<u8>(dest[3]);
        }
    }

    // 2. Exact Integer Spatial Streaming Stage
    for (int x = 0; x < L; ++x) {
        for (int y = 0; y < L; ++y) {
            for (int z = 0; z < L; ++z) {
                int src_cell = idx3d(x, y, z);
                for (int ki = 0; ki < K; ++ki) {
                    int nx = (x + v[ki].x + L) % L;
                    int ny = (y + v[ki].y + L) % L;
                    int nz = (z + v[ki].z + L) % L;
                    int dst_cell = idx3d(nx, ny, nz);
                    next_state[dst_cell * K + ki] = state[src_cell * K + ki];
                }
            }
        }
    }
    state.swap(next_state);
}

int main(int argc, char** argv) {
    string path = "D:/ASOLARIA-HOST8-ROOMS-300G/sector-000.rooms";
    if (argc > 1) path = argv[1];

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Cannot open " << path << "\n";
        return 1;
    }

    vector<u64> stubs(V);
    file.read(reinterpret_cast<char*>(stubs.data()), V * 8);
    if (file.gcount() != V * 8) {
        std::cerr << "Cannot read " << (V * 8) << " bytes\n";
        return 1;
    }
    file.close();

    string input_sha = asolaria::SHA256::hash_bytes(reinterpret_cast<const uint8_t*>(stubs.data()), V * 8);
    std::cout << "Input Physical Block SHA-256: " << input_sha << "\n";

    auto v = make_velocities();
    auto col = make_collisions(v);

    vector<u8> state(N, 0);
    vector<u8> next_state(N, 0);
    for (int cell = 0; cell < V; ++cell) {
        u64 val = stubs[cell];
        int base = cell * K;
        for (int k = 0; k < K; ++k) {
            state[base + k] = static_cast<u8>(val % 3);
            val /= 3;
        }
    }

    string init_state_sha = asolaria::SHA256::hash_bytes(state.data(), state.size());
    std::cout << "Initial Unpacked State SHA-256 (tick 0): " << init_state_sha << "\n";

    std::ofstream out_csv("host8_cpp_physical_243_ticks.csv");
    out_csv << "tick,state_sha256\n";
    out_csv << "0," << init_state_sha << "\n";

    auto t0 = std::chrono::steady_clock::now();
    for (int t = 0; t < 243; ++t) {
        step_universe(state, next_state, col, v, t);
        string sha = asolaria::SHA256::hash_bytes(state.data(), state.size());
        out_csv << (t + 1) << "," << sha << "\n";
    }
    auto t1 = std::chrono::steady_clock::now();
    double elapsed_s = std::chrono::duration<double>(t1 - t0).count();

    string final_state_sha = asolaria::SHA256::hash_bytes(state.data(), state.size());
    std::cout << "Final State SHA-256 (tick 243): " << final_state_sha << "\n";
    std::cout << "C++ Kernel 243-tick execution time: " << elapsed_s << " s (" 
              << (243.0 / elapsed_s) << " ticks/s, " 
              << (243.0 * V / elapsed_s) << " rooms/s)\n";

    return 0;
}
