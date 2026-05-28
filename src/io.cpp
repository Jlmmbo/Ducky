#include "ducky/io.hpp"
#include <cstdio>

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

} // namespace dky
