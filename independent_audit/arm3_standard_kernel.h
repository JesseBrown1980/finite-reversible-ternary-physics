// arm3_standard_kernel.h
// ============================================================================
// Asolaria Standard Arm 3 Collision Kernel & Space-Time Scheduler
// Single source of truth for the canonical 288-gate standard collision operator.
// Guarantees:
// 1. Exactly 288 distinct 4-channel collision gates.
// 2. Strict exclusion of ALL_ONES_CODE = 40 ((1,1,1,1) fixed point), preserving
//    the isotropic quiescent ground state across all 288 gates.
// 3. Space-time mixing schedule:
//    gate_idx = (t * 32 + cell * 17 + j * 37) % 288.
// ============================================================================

#ifndef ASOLARIA_ARM3_STANDARD_KERNEL_H
#define ASOLARIA_ARM3_STANDARD_KERNEL_H

#include "equivariant_intershell_kernel.h"
#include <vector>
#include <array>
#include <map>
#include <tuple>
#include <algorithm>
#include <cstdint>
#include <cassert>
#include <iostream>

namespace asolaria {

struct Arm3Gate {
    int a, b, c, d;
    std::array<std::uint8_t, 81> lut;
};

constexpr int ALL_ONES_CODE = 40; // Code 40 = 1 + 3*(1 + 3*(1 + 3*1)) = (1,1,1,1)

inline std::vector<Arm3Gate> make_arm3_standard_gates(const std::array<Vel, 27>& v) {
    using PairKey = std::tuple<int, int, int, int>;
    std::map<PairKey, std::vector<std::array<int, 2>>> pair_map;
    for (int i = 0; i < 27; ++i) {
        for (int j = i + 1; j < 27; ++j) {
            auto a = v[i], b = v[j];
            pair_map[{a.x + b.x, a.y + b.y, a.z + b.z, a.e + b.e}].push_back({i, j});
        }
    }
    std::map<std::array<int, 4>, bool> unique_quads;
    for (const auto& kv : pair_map) {
        const auto& pairs = kv.second;
        for (size_t i = 0; i < pairs.size(); ++i) {
            for (size_t j = i + 1; j < pairs.size(); ++j) {
                auto p1 = pairs[i];
                auto p2 = pairs[j];
                std::array<int, 4> q = {p1[0], p1[1], p2[0], p2[1]};
                std::sort(q.begin(), q.end());
                if (std::adjacent_find(q.begin(), q.end()) == q.end()) {
                    unique_quads[q] = true;
                }
            }
        }
    }
    std::vector<Arm3Gate> gates;
    for (const auto& kv : unique_quads) {
        std::array<int, 4> q = kv.first;
        Arm3Gate g{q[0], q[1], q[2], q[3], {}};
        for (int mask = 0; mask < 81; ++mask) g.lut[mask] = static_cast<std::uint8_t>(mask);

        std::map<std::array<int, 5>, std::vector<int>> classes;
        for (int mask = 0; mask < 81; ++mask) {
            classes[get_invariants(decode_mask(mask), q, v)].push_back(mask);
        }

        for (const auto& c_entry : classes) {
            std::vector<int> codes;
            for (int code : c_entry.second) {
                // Code 40 = (1,1,1,1) is strictly preserved as stationary isotropic fixed point
                if (code != ALL_ONES_CODE) codes.push_back(code);
            }
            for (size_t j = 0; j + 1 < codes.size(); j += 2) {
                int c1 = codes[j];
                int c2 = codes[j + 1];
                g.lut[c1] = static_cast<std::uint8_t>(c2);
                g.lut[c2] = static_cast<std::uint8_t>(c1);
            }
        }
        gates.push_back(g);
    }
    assert(gates.size() == 288);
    // Exhaustive sanity check: every gate MUST preserve ALL_ONES_CODE = 40
    for (size_t gi = 0; gi < gates.size(); ++gi) {
        assert(gates[gi].lut[ALL_ONES_CODE] == ALL_ONES_CODE);
    }
    return gates;
}

inline void step_lattice_arm3(std::vector<std::uint8_t>& state,
                              std::vector<std::uint8_t>& scratch,
                              const std::vector<Arm3Gate>& gates,
                              const std::array<Vel, 27>& v, int L, int t) {
    int num_cells = L * L * L;
    size_t total_c = gates.size();
    constexpr int gates_per_cell = 32;

    for (int cell = 0; cell < num_cells; ++cell) {
        int base = cell * 27;
        for (int j = 0; j < gates_per_cell; ++j) {
            size_t which = (static_cast<size_t>(t) * gates_per_cell +
                            static_cast<size_t>(cell) * 17 +
                            static_cast<size_t>(j) * 37) % total_c;
            const auto& gate = gates[which];
            int in_mask = encode_mask(state[base + gate.a], state[base + gate.b], state[base + gate.c], state[base + gate.d]);
            auto dec = decode_mask(gate.lut[in_mask]);
            state[base + gate.a] = static_cast<std::uint8_t>(dec[0]);
            state[base + gate.b] = static_cast<std::uint8_t>(dec[1]);
            state[base + gate.c] = static_cast<std::uint8_t>(dec[2]);
            state[base + gate.d] = static_cast<std::uint8_t>(dec[3]);
        }
    }
    for (int x = 0; x < L; ++x) {
        for (int y = 0; y < L; ++y) {
            for (int z = 0; z < L; ++z) {
                int src = (x * L + y) * L + z;
                for (int ki = 0; ki < 27; ++ki) {
                    int nx = (x + v[ki].x + L) % L;
                    int ny = (y + v[ki].y + L) % L;
                    int nz = (z + v[ki].z + L) % L;
                    int dst = (nx * L + ny) * L + nz;
                    scratch[dst * 27 + ki] = state[src * 27 + ki];
                }
            }
        }
    }
    state = scratch;
}

} // namespace asolaria

#endif // ASOLARIA_ARM3_STANDARD_KERNEL_H
