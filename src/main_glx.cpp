#include <X11/X.h>
#include <X11/Xlib.h>
#include <iostream>
#include <cmath>
#include <cstring>

#include <glad/glad.h>
#include <GL/gl.h>
#include <GL/glx.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

const char* vertexShaderSrc = R"(
#version 330 core
layout(location = 0) in vec4 aPos;
layout(location = 1) in vec2 aTexCoord;

out vec2 vTexCoord;

uniform float angle4d;

void main() {
    float c = cos(angle4d);
    float s = sin(angle4d);
    float x = aPos.x;
    float w = aPos.w;
    float newX = x * c - w * s;
    float newW = x * s + w * c;

    float dist4d = 3.0;
    float wDepth = dist4d - aPos.w;
    float scale4d = dist4d / wDepth;
    vec3 p3 = vec3(newX, aPos.y, aPos.z) * scale4d;

    float dist3d = 2.0;
    float zDepth = dist3d + p3.z;
    float scale3d = dist3d / zDepth;
    vec2 p2 = p3.xy * scale3d;

    gl_Position = vec4(p2, 0.0, 1.0);
    vTexCoord = aTexCoord;
}
)";

const char* fragmentShaderSrc = R"(
#version 330 core
in vec2 vTexCoord;
out vec4 FragColor;
uniform sampler2D uTexture;

void main() {
    FragColor = texture(uTexture, vTexCoord);
}
)";

unsigned int compileShader(unsigned int type, const char* src) {
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, NULL);
    glCompileShader(shader);
    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(shader, 512, NULL, log);
        std::cout << "Shader error:\n" << log << std::endl;
    }
    return shader;
}

int main() {
    Display* display = XOpenDisplay(NULL);
    if (!display) {
        std::cerr << "Failed to open X display\n";
        return -1;
    }
    std::cout << "Opened X display" << std::endl;

    int screen = DefaultScreen(display);
    Window root = RootWindow(display, screen);

    GLint attribs[] = {
        GLX_RGBA,
        GLX_DEPTH_SIZE, 24,
        GLX_DOUBLEBUFFER,
        GLX_CONTEXT_MAJOR_VERSION_ARB, 3,
        GLX_CONTEXT_MINOR_VERSION_ARB, 3,
        GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
        None
    };

    GLXFBConfig fbConfig = 0;
    int numConfigs = 0;
    GLXFBConfig* configs = glXChooseFBConfig(display, screen, attribs, &numConfigs);
    if (numConfigs > 0) {
        fbConfig = configs[0];
    }
    XFree(configs);

    if (!fbConfig) {
        std::cerr << "Failed to get FB config\n";
        return -1;
    }

    PFNGLXCREATECONTEXTATTRIBSARBPROC glXCreateContextAttribsARB = (PFNGLXCREATECONTEXTATTRIBSARBPROC)glXGetProcAddress((GLubyte*)"glXCreateContextAttribsARB");
    int contextAttribs[] = { None };
    GLXContext ctx = glXCreateContextAttribsARB(display, fbConfig, NULL, True, contextAttribs);
    if (!ctx) {
        std::cerr << "Failed to create GL context\n";
        return -1;
    }
    std::cout << "Created GL context" << std::endl;

    Window win = XCreateWindow(display, root, 0, 0, 1920, 1920, 0, CopyFromParent, InputOutput, CopyFromParent, 0, NULL);
    XMapWindow(display, win);
    XStoreName(display, win, "4D Tesseract");

    glXMakeContextCurrent(display, win, win, ctx);
    std::cout << "Made GL context current" << std::endl;

    // Load model
    FILE* file = fopen("model.dky", "r");
    if (!file) {
        std::cerr << "Failed to open model\n";
        return -1;
    }

    float* vertices = NULL;
    unsigned int vertexCount = 0;
    unsigned int* indices = NULL;
    unsigned int indexCount = 0;
    int section = 0;
    char line[256];

    while (fgets(line, sizeof(line), file)) {
        if (line[0] == '/' && line[1] == '/') continue;
        if (line[0] == '\n' || line[0] == '\r' || line[0] == '\0') continue;
        if (line[0] == 'f' && line[1] == 'a') { section = 1; continue; }

        if (section == 0) {
            float x, y, z, w, u, v;
            if (sscanf(line, "%f %f %f %f %f %f", &x, &y, &z, &w, &u, &v) == 6) {
                float* newverts = new float[(vertexCount + 1) * 6];
                if (vertices) { memcpy(newverts, vertices, vertexCount * 6 * sizeof(float)); delete[] vertices; }
                vertices = newverts;
                vertices[vertexCount * 6] = x; vertices[vertexCount * 6 + 1] = y;
                vertices[vertexCount * 6 + 2] = z; vertices[vertexCount * 6 + 3] = w;
                vertices[vertexCount * 6 + 4] = u; vertices[vertexCount * 6 + 5] = v;
                vertexCount++;
            }
        } else {
            unsigned int i0, i1, i2;
            if (sscanf(line, "%u %u %u", &i0, &i1, &i2) == 3) {
                unsigned int* newindices = new unsigned int[indexCount + 3];
                if (indices) { memcpy(newindices, indices, indexCount * sizeof(unsigned int)); delete[] indices; }
                indices = newindices;
                indices[indexCount] = i0; indices[indexCount + 1] = i1; indices[indexCount + 2] = i2;
                indexCount += 3;
            }
        }
    }
    fclose(file);
    std::cout << "Loaded " << vertexCount << " vertices, " << indexCount << " indices" << std::endl;

    // Setup GL
    unsigned int VAO, VBO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertexCount * 6 * sizeof(float), vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexCount * sizeof(unsigned int), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(4 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Shaders
    unsigned int vertShader = compileShader(GL_VERTEX_SHADER, vertexShaderSrc);
    unsigned int fragShader = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSrc);
    unsigned int program = glCreateProgram();
    glAttachShader(program, vertShader);
    glAttachShader(program, fragShader);
    glLinkProgram(program);

    // Texture
    int width, height, channels;
    unsigned char* data = stbi_load("00001.png", &width, &height, &channels, 4);
    if (!data) { std::cerr << "Failed to load texture\n"; return -1; }
    std::cout << "Loaded texture: " << width << "x" << height << std::endl;

    unsigned int texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    stbi_image_free(data);

    glUseProgram(program);
    glUniform1i(glGetUniformLocation(program, "uTexture"), 0);

    float angle4d = 0.0f;
    while (1) {
        XEvent evt;
        if (XPending(display)) {
            XNextEvent(display, &evt);
            if (evt.type == KeyPress || evt.type == DestroyNotify) break;
        }

        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        angle4d += 0.01f;
        glUniform1f(glGetUniformLocation(program, "angle4d"), angle4d);

        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);

        glXSwapBuffers(display, win);
    }

    delete[] vertices;
    delete[] indices;
    glXMakeContextCurrent(display, None, None, NULL);
    glXDestroyContext(display, ctx);
    XDestroyWindow(display, win);
    XCloseDisplay(display);

    return 0;
}