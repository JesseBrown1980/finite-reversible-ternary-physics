// equivariant_intershell_kernel.h
// ============================================================================
// Asolaria 100% Oh-Equivariant, Reversible Inter-Shell Collision Kernel
// Combines:
// 1. Intra-shell 3 commuting edge gates (Orbit 17)
// 2. Intra-shell 7 Oh-isomorphic face orbit pairs (FACE_LUT)
// 3. Inter-shell Rest-Edge-Face coupling (Orbit 15: Rest + Edge <-> 2 Face)
// 4. Inter-shell Corner-Face-Edge coupling (Orbit 1: Corner + Face <-> 2 Edge)
// ============================================================================

#pragma once

#include "face_lut.h"
#include <vector>
#include <array>
#include <map>
#include <cstdint>
#include <algorithm>

namespace asolaria {

using std::array;
using std::vector;
using std::map;
using u8 = std::uint8_t;
constexpr int K = 27;

struct Vel { int x, y, z, e; };

inline array<Vel, K> make_vels() {
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

inline array<int, 4> decode_mask(int mask) {
    array<int, 4> dec{};
    for (int i = 0; i < 4; ++i) { dec[i] = mask % 3; mask /= 3; }
    return dec;
}

inline int encode_mask(int a, int b, int c, int d) {
    return a + 3 * b + 9 * c + 27 * d;
}

inline array<int, 5> get_invariants(const array<int, 4>& counts,
                                    const array<int, 4>& q,
                                    const array<Vel, K>& v) {
    int mass = 0, px = 0, py = 0, pz = 0, e = 0;
    for (int i = 0; i < 4; ++i) {
        int n = counts[i];
        mass += n; px += n * v[q[i]].x; py += n * v[q[i]].y; pz += n * v[q[i]].z; e += n * v[q[i]].e;
    }
    return {mass, px, py, pz, e};
}

inline Gate build_gate(int a, int b, int c, int d, const array<Vel, K>& v) {
    array<int, 4> q = {a, b, c, d};
    Gate g{a, b, c, d, {}};
    map<array<int, 5>, vector<int>> classes;
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

inline vector<Gate> make_edge_gates(const array<Vel, K>& v) {
    return {
        build_gate(1, 7, 19, 25, v),  // xy edge plane
        build_gate(3, 5, 21, 23, v),  // xz edge plane
        build_gate(9, 11, 15, 17, v)   // yz edge plane
    };
}

inline void apply_gate(u8* s, const Gate& g) {
    int in_mask = encode_mask(s[g.a], s[g.b], s[g.c], s[g.d]);
    int out_mask = g.lut[in_mask];
    auto dec = decode_mask(out_mask);
    s[g.a] = static_cast<u8>(dec[0]);
    s[g.b] = static_cast<u8>(dec[1]);
    s[g.c] = static_cast<u8>(dec[2]);
    s[g.d] = static_cast<u8>(dec[3]);
}

inline void apply_face_lut(u8* s) {
    int code = s[22] + 3 * (s[4] + 3 * (s[16] + 3 * (s[10] + 3 * (s[14] + 3 * s[12]))));
    uint16_t out = FACE_LUT[code];
    s[22] = out % 3; out /= 3;
    s[4]  = out % 3; out /= 3;
    s[16] = out % 3; out /= 3;
    s[10] = out % 3; out /= 3;
    s[14] = out % 3; out /= 3;
    s[12] = out;
}

struct Quad { int c, f, e1, e2; };

struct InterShellData {
    int rest_idx;
    vector<int> edge_indices;
    map<int, array<int, 2>> edge_face_map;
    vector<Quad> corner_quads;
};

inline InterShellData init_intershell_data(const array<Vel, K>& v) {
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

inline void op_rest_edge_face(u8* s, const InterShellData& d) {
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

inline void op_corner_face_edge(u8* s, const InterShellData& d) {
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

inline void apply_full_symmetric_collision(u8* s, const vector<Gate>& edge_gates, const InterShellData& d) {
    for (const auto& g : edge_gates) apply_gate(s, g);
    apply_face_lut(s);
    op_rest_edge_face(s, d);
    op_corner_face_edge(s, d);
}

inline void apply_full_symmetric_collision_backward(u8* s, const vector<Gate>& edge_gates, const InterShellData& d) {
    op_corner_face_edge(s, d);
    op_rest_edge_face(s, d);
    apply_face_lut(s);
    for (const auto& g : edge_gates) apply_gate(s, g);
}

} // namespace asolaria
