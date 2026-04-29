struct Vertex {
    float x, y;   // screen position
    float u, v;   // texture coordinates
    char* texture; //mostly unused
};

struct Color {
    uint8_t r, g, b, a;
};

struct Model{
    float* vertices; //x y u v per vertex
    unsigned int vertexCount;
    unsigned int* indices; //face indices
    unsigned int indexCount;
    char** textures;
    unsigned int textureCount;
};

static inline int clampi(int v, int a, int b) {
    return (v < a) ? a : (v > b ? b : v);
}

static inline float edge(float ax, float ay,
                         float bx, float by,
                         float cx, float cy) {
    return (cx - ax) * (by - ay) - (cy - ay) * (bx - ax);
}

void rotatePlane(float &x, float &y, float angle){
    float s = sin(angle);
    float c = cos(angle);
    float newX = x * c - y * s;
    float newY = x * s + y * c;
    x = newX;
    y = newY;
}

Model LoadModel(const char* path){
    Model model;
    FILE* file = fopen(path, "r");
    if (!file) {
        std::cerr << "Failed to open model file\n";
        exit(-1);
    }

    char line[256];
    model.vertexCount = 0;
    model.vertices = NULL;
    model.indexCount = 0;
    model.indices = NULL;

    int section = 0; // 0=vertices, 1=faces

    while (fgets(line, sizeof(line), file)) {
        if (line[0] == '/' && line[1] == '/') continue;
        if (line[0] == '\n' || line[0] == '\r' || line[0] == '\0') continue;

        // Check for section marker (faces)
        if (line[0] == 'f' && line[1] == 'a') {
            section = 1;
            continue;
        }

        if (section == 0) {
            float x, y, u, v;
            if (sscanf(line, "%f %f %f %f", &x, &y, &u, &v) == 4) {
                float* newverts = new float[(model.vertexCount + 1) * 4];
                if (model.vertices) {
                    memcpy(newverts, model.vertices, model.vertexCount * 4 * sizeof(float));
                    delete[] model.vertices;
                }
                model.vertices = newverts;
                model.vertices[model.vertexCount * 4] = x;
                model.vertices[model.vertexCount * 4 + 1] = y;
                model.vertices[model.vertexCount * 4 + 2] = u;
                model.vertices[model.vertexCount * 4 + 3] = v;
                model.vertexCount++;
            }
        } else if (section == 1) {
            unsigned int i0, i1, i2;
            if (sscanf(line, "%u %u %u", &i0, &i1, &i2) == 3) {
                unsigned int* newindices = new unsigned int[(model.indexCount + 3)];
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

    model.textureCount = 0;
    model.textures = NULL;

    fclose(file);
    return model;
}