#include <cstring>
#include <iostream>

struct Model {
    float* vertices;           // x, y, z, w, u, v, wt per vertex
    unsigned int vertexCount;
    unsigned int* indices;      // face indices
    unsigned int indexCount;
};

Model LoadModel(const char* path) {
    Model model = {nullptr, 0, nullptr, 0};

    FILE* file = fopen(path, "r");
    if (!file) {
        std::cerr << "Failed to open model file\n";
        return model;
    }

    // First pass: count vertices and indices
    char line[256];
    int section = 0;
    unsigned int vertCount = 0, idxCount = 0;
    while (fgets(line, sizeof(line), file)) {
        if (line[0] == '/' && line[1] == '/') continue;
        if (line[0] == '\n' || line[0] == '\r' || line[0] == '\0') continue;
        if (line[0] == 'f' && line[1] == 'a') { section = 1; continue; }
        if (section == 0) vertCount++;
        else idxCount += 3;
    }

    model.vertexCount = vertCount;
    model.indexCount = idxCount;
    model.vertices = new float[vertCount * 7];
    model.indices = new unsigned int[idxCount];

    // Second pass: read data
    rewind(file);
    section = 0;
    unsigned int vi = 0, ii = 0;
    while (fgets(line, sizeof(line), file)) {
        if (line[0] == '/' && line[1] == '/') continue;
        if (line[0] == '\n' || line[0] == '\r' || line[0] == '\0') continue;
        if (line[0] == 'f' && line[1] == 'a') { section = 1; continue; }

        if (section == 0) {
            float x, y, z, w, u, v, wt;
            if (sscanf(line, "%f %f %f %f %f %f %f", &x, &y, &z, &w, &u, &v, &wt) == 7) {
                int offset = vi * 7;
                model.vertices[offset] = x;
                model.vertices[offset + 1] = y;
                model.vertices[offset + 2] = z;
                model.vertices[offset + 3] = w;
                model.vertices[offset + 4] = u;
                model.vertices[offset + 5] = v;
                model.vertices[offset + 6] = wt;
                vi++;
            }
        } else {
            unsigned int i0, i1, i2;
            if (sscanf(line, "%u %u %u", &i0, &i1, &i2) == 3) {
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
