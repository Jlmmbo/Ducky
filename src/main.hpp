#include <cstring>
#include <iostream>
#include <cstdlib>

struct Model {
    float* vertices;           // N + 3 values per vertex (N pos + 3 color)
    unsigned int vertexCount;
    unsigned int* indices;
    unsigned int indexCount;
    unsigned int dimensions;   // number of spatial dimensions
};

static bool isEmptyOrComment(const char* line) {
    return line[0] == '\0' || line[0] == '\n' || line[0] == '\r' ||
           (line[0] == '/' && line[1] == '/');
}

Model LoadModel(const char* path) {
    Model model = {nullptr, 0, nullptr, 0, 4};

    FILE* file = fopen(path, "r");
    if (!file) {
        std::cerr << "Failed to open model file\n";
        return model;
    }

    char line[1024];
    unsigned int dims = 4;
    bool section = false; // false = vertices, true = faces
    unsigned int vertCount = 0, idxCount = 0;
    bool dimsRead = false;

    // First pass: read dims, count vertices and indices
    while (fgets(line, sizeof(line), file)) {
        if (isEmptyOrComment(line)) continue;

        if (!dimsRead) {
            unsigned int d;
            if (sscanf(line, " dims %u", &d) >= 1 || sscanf(line, "dims %u", &d) >= 1) {
                if (d >= 3) dims = d;
                dimsRead = true;
                continue;
            }
            dimsRead = true;
        }

        if (line[0] == 'f' && line[1] == 'a') { section = true; continue; }
        if (!section) vertCount++;
        else idxCount += 3;
    }

    model.dimensions = dims;
    int fpv = dims + 3;
    model.vertexCount = vertCount;
    model.indexCount = idxCount;
    model.vertices = new float[vertCount * fpv];
    model.indices = new unsigned int[idxCount];

    // Second pass: read data
    rewind(file);
    section = false;
    unsigned int vi = 0, ii = 0;
    dimsRead = false;

    while (fgets(line, sizeof(line), file)) {
        if (isEmptyOrComment(line)) continue;

        if (!dimsRead) {
            unsigned int d;
            if (sscanf(line, " dims %u", &d) >= 1 || sscanf(line, "dims %u", &d) >= 1) {
                dimsRead = true;
                continue;
            }
            dimsRead = true;
        }

        if (line[0] == 'f' && line[1] == 'a') { section = true; continue; }

        if (!section) {
            float vals[32];
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
