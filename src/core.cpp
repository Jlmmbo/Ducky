#include "ducky/core.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <set>
#include <map>

#ifdef _WIN32
#include <malloc.h>
#else
#include <alloca.h>
#endif

namespace dky {

int TransformND::planeCount() const {
    return dims * (dims - 1) / 2;
}

int TransformND::planeIndex(int i, int j) const {
    int idx = 0;
    for (int a = 0; a < i; a++)
        idx += dims - a - 1;
    return idx + (j - i - 1);
}

Model LoadModel(const char* path) {
    Model model;

    FILE* file = fopen(path, "r");
    if (!file) {
        std::cerr << "Failed to open model file: " << path << std::endl;
        return model;
    }

    char line[1024];
    unsigned int dims = 4;
    bool section = false;
    unsigned int vertCount = 0, idxCount = 0;
    bool dimsRead = false;

    while (fgets(line, sizeof(line), file)) {
        if (line[0] == '\0' || line[0] == '\n' || line[0] == '\r' ||
            (line[0] == '/' && line[1] == '/')) continue;

        if (!dimsRead) {
            unsigned int d;
            if (sscanf(line, " dims %u", &d) >= 1 || sscanf(line, "dims %u", &d) >= 1 ||
                sscanf(line, " DIMS %u", &d) >= 1 || sscanf(line, "DIMS %u", &d) >= 1 ||
                sscanf(line, " Dims %u", &d) >= 1 || sscanf(line, "Dims %u", &d) >= 1) {
                if (d >= 3) dims = d;
                dimsRead = true;
                continue;
            }
            dimsRead = true;
        }

        if (strncmp(line, "face", 4) == 0) { section = true; continue; }
        if (!section) vertCount++;
        else idxCount += 3;
    }

    model.dimensions = dims;
    int fpv = dims + 3;
    model.vertexCount = vertCount;
    model.indexCount = idxCount;
    model.vertices.resize(vertCount * fpv);
    model.indices.resize(idxCount);

    rewind(file);
    section = false;
    unsigned int vi = 0, ii = 0;
    dimsRead = false;

    while (fgets(line, sizeof(line), file)) {
        if (line[0] == '\0' || line[0] == '\n' || line[0] == '\r' ||
            (line[0] == '/' && line[1] == '/')) continue;

        if (!dimsRead) {
            unsigned int d;
            if (sscanf(line, " dims %u", &d) >= 1 || sscanf(line, "dims %u", &d) >= 1 ||
                sscanf(line, " DIMS %u", &d) >= 1 || sscanf(line, "DIMS %u", &d) >= 1 ||
                sscanf(line, " Dims %u", &d) >= 1 || sscanf(line, "Dims %u", &d) >= 1) {
                dimsRead = true;
                continue;
            }
            dimsRead = true;
        }

        if (strncmp(line, "face", 4) == 0) { section = true; continue; }

        if (!section) {
            std::vector<float> vals(fpv);
            int parsed = 0;
            const char* ptr = line;
            while (parsed < fpv) {
                char* end;
                vals[parsed] = strtof(ptr, &end);
                if (end == ptr) break;
                ptr = end;
                parsed++;
            }
            if (parsed == fpv) {
                for (int i = 0; i < fpv; i++)
                    model.vertices[vi * fpv + i] = vals[i];
                vi++;
            }
        } else {
            unsigned int i0, i1, i2;
            if (sscanf(line, "%u %u %u", &i0, &i1, &i2) == 3) {
                if (i0 >= model.vertexCount || i1 >= model.vertexCount || i2 >= model.vertexCount) {
                    std::cerr << "Index out of bounds in " << path << ": "
                              << i0 << " " << i1 << " " << i2
                              << " (vertexCount=" << model.vertexCount << ")" << std::endl;
                    continue;
                }
                model.indices[ii] = i0;
                model.indices[ii + 1] = i1;
                model.indices[ii + 2] = i2;
                ii += 3;
            }
        }
    }

    fclose(file);
    return model;
}

void hslToRgb(float h, float s, float l, float& r, float& g, float& b) {
    auto hueToRgb = [](float p, float q, float t) {
        if (t < 0.0f) t += 1.0f;
        if (t > 1.0f) t -= 1.0f;
        if (t < 1.0f/6.0f) return p + (q - p) * 6.0f * t;
        if (t < 1.0f/2.0f) return q;
        if (t < 2.0f/3.0f) return p + (q - p) * (2.0f/3.0f - t) * 6.0f;
        return p;
    };
    if (s == 0.0f) {
        r = g = b = l;
    } else {
        float q = l < 0.5f ? l * (1.0f + s) : l + s - l * s;
        float p = 2.0f * l - q;
        r = hueToRgb(p, q, h + 1.0f/3.0f);
        g = hueToRgb(p, q, h);
        b = hueToRgb(p, q, h - 1.0f/3.0f);
    }
}

void rotatePlane(float& a, float& b, float angle) {
    float c = cosf(angle), s = sinf(angle);
    float na = a * c - b * s;
    float nb = a * s + b * c;
    a = na;
    b = nb;
}

void applyRotation(float* pos, const TransformND& t) {
    for (unsigned int i = 0; i < t.dims; i++) {
        for (unsigned int j = i + 1; j < t.dims; j++) {
            float angle = t.angles[t.planeIndex(i, j)];
            if (fabsf(angle) > 0.0001f)
                rotatePlane(pos[i], pos[j], angle);
        }
    }
}

void projectPerspective(const float* in, float* out, int dims, float focalLength) {
    float* tmp = (float*)alloca(dims * sizeof(float));
    for (int i = 0; i < dims; i++) tmp[i] = in[i];

    for (int d = dims - 1; d >= 3; d--) {
        float dist = (float)d * focalLength;
        float depth = dist - tmp[d];
        float scale = depth > 0.001f ? dist / depth : 10.0f;
        for (int c = 0; c < d; c++)
            tmp[c] *= scale;
    }

    out[0] = tmp[0];
    out[1] = tmp[1];
    out[2] = tmp[2];
}

void projectOrthographic(const float* in, float* out, int dims) {
    out[0] = in[0];
    out[1] = in[1];
    out[2] = dims > 2 ? in[2] : 0.0f;
}

void projectStereographic(const float* in, float* out, int dims, float focalLength) {
    float tmp[128];
    for (int i = 0; i < dims; i++) tmp[i] = in[i];

    for (int d = dims - 1; d >= 4; d--) {
        float dist = (float)d * focalLength;
        float depth = dist - tmp[d];
        float s = depth > 0.001f ? dist / depth : 10.0f;
        for (int c = 0; c < d; c++)
            tmp[c] *= s;
    }

    float x = tmp[0], y = tmp[1], z = tmp[2], w = dims > 3 ? tmp[3] : 0.0f;
    float radius = sqrtf(x*x + y*y + z*z + w*w);
    float denom = radius - w;
    if (fabsf(denom) < 0.001f) denom = 0.001f;
    float s = radius / denom;
    out[0] = x * s;
    out[1] = y * s;
    out[2] = z * s;
}

std::vector<Edge> generateEdges(const float* vertices, unsigned int vertexCount,
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

void assignFaceColors(Model& model, int colorScheme) {
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

} // namespace dky
