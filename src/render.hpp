#pragma once

#include <vector>
#include <set>
#include <map>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <glad/glad.h>
#include "stb_easy_font.h"
#include "main.hpp"
#include "transform.hpp"

struct Edge { int a, b; };

inline std::vector<Edge> generateEdges(const float* vertices, unsigned int vertexCount,
                                        unsigned int dims, int fpv,
                                        const unsigned int* indices, unsigned int indexCount) {
    std::vector<int> canonical(vertexCount);
    std::vector<int> reverseCanonical;
    for (unsigned int i = 0; i < vertexCount; i++) {
        int found = -1;
        for (size_t j = 0; j < reverseCanonical.size(); j++) {
            int ci = reverseCanonical[j];
            bool same = true;
            for (unsigned int d = 0; d < dims; d++) {
                if (fabsf(vertices[i * fpv + d] - vertices[ci * fpv + d]) > 0.001f) {
                    same = false;
                    break;
                }
            }
            if (same) { found = (int)j; break; }
        }
        if (found < 0) {
            found = (int)reverseCanonical.size();
            reverseCanonical.push_back((int)i);
        }
        canonical[i] = found;
    }
    unsigned int uniqueCount = (unsigned int)reverseCanonical.size();

    std::set<std::pair<int,int>> edgeSet;
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

    std::map<std::pair<int,int>, int> edgeCounts;
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

inline void drawRect(GLuint program, GLuint vao, GLuint vbo,
                     float x, float y, float w, float h,
                     float r, float g, float b, float a,
                     float screenW, float screenH) {
    float verts[8] = {
        x, y, x+w, y, x+w, y+h,
        x, y+h
    };
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);
    glUseProgram(program);
    glUniform2f(glGetUniformLocation(program, "uScreenSize"), screenW, screenH);
    glUniform4f(glGetUniformLocation(program, "uColor"), r, g, b, a);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

inline int drawTextAt(GLuint vao, GLuint vbo, GLuint ebo, GLuint program,
                      float x, float y, const char* text,
                      float screenW, float screenH,
                      const unsigned int* indices, int maxQuads) {
    static std::vector<char> buf(2048);
    int nq = stb_easy_font_print(x, y, (char*)text, nullptr, buf.data(), (int)buf.size());
    if (nq <= 0 || nq > maxQuads) return 0;
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, nq * 64, buf.data());
    glUseProgram(program);
    glUniform2f(glGetUniformLocation(program, "uScreenSize"), screenW, screenH);
    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, nq * 6, GL_UNSIGNED_INT, nullptr);
    return nq;
}

inline void assignFaceColors(Model& model, int colorScheme) {
    if (model.vertexCount == 0 || model.indexCount == 0) return;
    unsigned int dims = model.dimensions;
    int fpv = dims + 3;
    unsigned int faces = model.indexCount / 3;
    const float goldenRatio = 0.618033988749895f;

    for (unsigned int f = 0; f < faces; f++) {
        float h;
        switch (colorScheme) {
            case 1: h = (float)f / (float)faces; break;
            case 2: h = 0.6f; break;
            case 3: h = 0.0f + (float)(f % 10) * 0.05f; break;
            default: h = f * goldenRatio; h = h - floorf(h); break;
        }
        float s = (colorScheme == 2) ? 0.4f : 0.85f;
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
