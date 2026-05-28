#include "ducky/io.hpp"
#include <cstdio>
#include <iostream>
#include <vector>
#include <ctime>

namespace dky {

void writeTGA(const char* path, int w, int h, unsigned char* data) {
    FILE* f = fopen(path, "wb");
    if (!f) return;
    unsigned char header[18] = {0};
    header[2] = 2;
    header[12] = w & 0xFF;
    header[13] = (w >> 8) & 0xFF;
    header[14] = h & 0xFF;
    header[15] = (h >> 8) & 0xFF;
    header[16] = 24;
    fwrite(header, 1, 18, f);
    for (int y = h - 1; y >= 0; y--)
        fwrite(data + y * w * 3, 1, w * 3, f);
    fclose(f);
}

void saveState(const char* path, const TransformND& t) {
    FILE* f = fopen(path, "w");
    if (!f) { std::cerr << "Failed to save state\n"; return; }
    fprintf(f, "%u\n", t.dims);
    for (float a : t.angles) fprintf(f, "%.8f ", a);
    fprintf(f, "\n");
    for (float tr : t.translation) fprintf(f, "%.8f ", tr);
    fprintf(f, "\n");
    fclose(f);
}

bool loadState(const char* path, TransformND& t) {
    FILE* f = fopen(path, "r");
    if (!f) { std::cerr << "Failed to load state\n"; return false; }
    unsigned int dims;
    if (fscanf(f, "%u", &dims) != 1 || dims != t.dims) {
        fclose(f);
        std::cerr << "State file dimension mismatch\n";
        return false;
    }
    std::vector<float> newAngles(t.planeCount());
    std::vector<float> newTranslations(t.dims);
    bool ok = true;
    for (int i = 0; i < t.planeCount(); i++) {
        if (fscanf(f, "%f", &newAngles[i]) != 1) { ok = false; break; }
    }
    if (ok) {
        for (unsigned int i = 0; i < t.dims; i++) {
            if (fscanf(f, "%f", &newTranslations[i]) != 1) { ok = false; break; }
        }
    }
    fclose(f);
    if (!ok) {
        std::cerr << "Failed to read full state from " << path << std::endl;
        return false;
    }
    t.angles = newAngles;
    t.translation = newTranslations;
    return true;
}

} // namespace dky
