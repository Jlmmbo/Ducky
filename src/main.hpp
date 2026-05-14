#include <cstring>
#include <iostream>

struct Model {
    float* vertices;           // x, y, z, w, u, v per vertex
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

    char line[256];
    int section = 0; // 0=vertices, 1=faces

    while (fgets(line, sizeof(line), file)) {
        if (line[0] == '/' && line[1] == '/') continue;
        if (line[0] == '\n' || line[0] == '\r' || line[0] == '\0') continue;

        if (line[0] == 'f' && line[1] == 'a') {
            section = 1;
            continue;
        }

        if (section == 0) {
            float x, y, z, w, u, v;
            if (sscanf(line, "%f %f %f %f %f %f", &x, &y, &z, &w, &u, &v) == 6) {
                float* newverts = new float[(model.vertexCount + 1) * 6];
                if (model.vertices) {
                    memcpy(newverts, model.vertices, model.vertexCount * 6 * sizeof(float));
                    delete[] model.vertices;
                }
                model.vertices = newverts;
                int offset = model.vertexCount * 6;
                model.vertices[offset] = x;
                model.vertices[offset + 1] = y;
                model.vertices[offset + 2] = z;
                model.vertices[offset + 3] = w;
                model.vertices[offset + 4] = u;
                model.vertices[offset + 5] = v;
                model.vertexCount++;
            }
        } else if (section == 1) {
            unsigned int i0, i1, i2;
            if (sscanf(line, "%u %u %u", &i0, &i1, &i2) == 3) {
                unsigned int* newindices = new unsigned int[model.indexCount + 3];
                if (model.indices) {
                    memcpy(newindices, model.indices, model.indexCount * sizeof(unsigned int));
                    delete[] model.indices;
                }
                model.indices = newindices;
                model.indices[model.indexCount] = i0;
                model.indices[model.indexCount + 1] = i1;
                model.indices[model.indexCount + 2] = i2;
                model.indexCount += 3;
            }
        }
    }

    fclose(file);
    return model;
}
