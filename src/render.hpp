#pragma once

#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <cmath>
#include "main.hpp"
#include "transform.hpp"

struct PairHash {
    size_t operator()(const std::pair<int,int>& p) const {
        return (size_t)p.first ^ ((size_t)p.second << 16);
    }
};

struct Edge { int a, b; };

inline std::vector<Edge> generateEdges(const float* vertices, unsigned int vertexCount,
                                        unsigned int dims, int fpv,
                                        const unsigned int* indices, unsigned int indexCount) {
    // Sort-based vertex dedup: O(N log N * dims) instead of O(N^2 * dims)
    std::vector<int> order(vertexCount);
    for (unsigned int i = 0; i < vertexCount; i++) order[i] = (int)i;
    std::sort(order.begin(), order.end(), [&](int a, int b) {
        for (unsigned int d = 0; d < dims; d++) {
            float da = vertices[a * fpv + d];
            float db = vertices[b * fpv + d];
            if (da < db - 0.001f) return true;
            if (da > db + 0.001f) return false;
        }
        return false;
    });

    std::vector<int> canonical(vertexCount);
    std::vector<int> reverseCanonical;
    for (unsigned int i = 0; i < vertexCount; i++) {
        int cur = order[i];
        if (i > 0) {
            int prev = order[i - 1];
            bool same = true;
            for (unsigned int d = 0; d < dims; d++) {
                if (fabsf(vertices[cur * fpv + d] - vertices[prev * fpv + d]) > 0.001f) {
                    same = false;
                    break;
                }
            }
            if (same) {
                canonical[cur] = canonical[prev];
                continue;
            }
        }
        canonical[cur] = (int)reverseCanonical.size();
        reverseCanonical.push_back(cur);
    }
    unsigned int uniqueCount = (unsigned int)reverseCanonical.size();

    // Try coordinate-difference edge detection first
    std::unordered_set<std::pair<int,int>, PairHash> edgeSet;
    for (unsigned int a = 0; a < uniqueCount; a++) {
        int ai = reverseCanonical[a];
        for (unsigned int b = a + 1; b < uniqueCount; b++) {
            int bi = reverseCanonical[b];
            int diff = 0;
            for (unsigned int d = 0; d < dims; d++) {
                if (fabsf(vertices[ai * fpv + d] - vertices[bi * fpv + d]) > 0.001f)
                    diff++;
            }
            if (diff == 1)
                edgeSet.insert({(int)a, (int)b});
        }
    }

    if (!edgeSet.empty()) {
        std::vector<Edge> edges;
        edges.reserve(edgeSet.size());
        for (auto& e : edgeSet)
            edges.push_back({reverseCanonical[e.first], reverseCanonical[e.second]});
        return edges;
    }

    // Fallback: shared triangle-edge detection
    std::unordered_map<std::pair<int,int>, int, PairHash> edgeCounts;
    for (unsigned int t = 0; t < indexCount / 3; t++) {
        int tri[3] = {
            canonical[indices[t * 3]],
            canonical[indices[t * 3 + 1]],
            canonical[indices[t * 3 + 2]]
        };
        for (int k = 0; k < 3; k++) {
            int a = tri[k], b = tri[(k + 1) % 3];
            if (a == b) continue;
            if (a > b) std::swap(a, b);
            edgeCounts[{a, b}]++;
        }
    }

    std::vector<Edge> edges;
    edges.reserve(edgeCounts.size());
    for (auto& ec : edgeCounts)
        edges.push_back({reverseCanonical[ec.first.first], reverseCanonical[ec.first.second]});
    return edges;
}

inline void assignFaceColors(Model& model, int colorScheme) {
    if (model.vertexCount == 0 || model.indexCount == 0) return;
    unsigned int dims = model.dimensions;
    int fpv = dims + 3;
    unsigned int faces = model.indexCount / 3;
    for (unsigned int f = 0; f < faces; f++) {
        float h;
        switch (colorScheme) {
            case 0: h = (float)f / (float)faces; break;
            default: h = 0.6f; break;
        }
        float s = (colorScheme == 1) ? 0.0f : 0.85f;
        float l = 0.45f + ((f / 8) % 3) * 0.2f;
        float r, g, b;
        hslToRgb(h, s, l, r, g, b);
        for (int j = 0; j < 3; j++) {
            int vi = model.indices[f * 3 + j];
            model.vertices[vi * fpv + dims] = r;
            model.vertices[vi * fpv + dims + 1] = g;
            model.vertices[vi * fpv + dims + 2] = b;
        }
    }
}
