#include <cstring>
#include <cstdio>
#include <iostream>
#include <cstdlib>
#include <vector>

struct Model {
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    unsigned int vertexCount = 0;
    unsigned int indexCount = 0;
    unsigned int dimensions = 4;
};

static bool isEmptyOrComment(const char* line) {
    return line[0] == '\0' || line[0] == '\n' || line[0] == '\r' ||
           (line[0] == '/' && line[1] == '/');
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

    // First pass: read dims, count vertices and indices
    while (fgets(line, sizeof(line), file)) {
        if (isEmptyOrComment(line)) continue;

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

    // Second pass: read data
    rewind(file);
    section = false;
    unsigned int vi = 0, ii = 0;
    dimsRead = false;

    while (fgets(line, sizeof(line), file)) {
        if (isEmptyOrComment(line)) continue;

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
