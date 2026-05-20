#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <iostream>
#include <cmath>
#include <cstring>
#include <unistd.h>

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
out float vDepth;

uniform float angle4d;
uniform float angle4dY;
uniform float angle4dZ;
uniform vec4 translation;

void main() {
    float x = aPos.x + translation.x;
    float y = aPos.y + translation.y;
    float z = aPos.z + translation.z;
    float w = aPos.w + translation.w;

    // XW rotation
    float cx = cos(angle4d);
    float sx = sin(angle4d);
    float newX = x * cx - w * sx;
    float newW = x * sx + w * cx;
    x = newX;
    w = newW;

    // YW rotation
    float cy = cos(angle4dY);
    float sy = sin(angle4dY);
    float newY = y * cy - w * sy;
    newW = y * sy + w * cy;
    y = newY;
    w = newW;

    // ZW rotation
    float cz = cos(angle4dZ);
    float sz = sin(angle4dZ);
    float newZ = z * cz - w * sz;
    newW = z * sz + w * cz;
    z = newZ;
    w = newW;

    // 4D to 3D with better depth handling
    float dist4d = 4.0;
    float wDepth = dist4d - w;
    float scale4d = clamp(dist4d / wDepth, 0.1, 10.0);
    vec3 p3 = vec3(x, y, z) * scale4d;

    // 3D to 2D
    float dist3d = 3.0;
    float zDepth = dist3d - p3.z;
    float scale3d = clamp(dist3d / max(zDepth, 0.1), 0.1, 10.0);
    vec2 p2 = p3.xy * scale3d;

    gl_Position = vec4(p2, 0.0, 1.0);
    vTexCoord = aTexCoord;
    vDepth = wDepth * zDepth;
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
        GLX_RENDER_TYPE, GLX_RGBA_BIT,
        GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
        GLX_DEPTH_SIZE, 24,
        GLX_DOUBLEBUFFER,
        GLX_RED_SIZE, 8,
        GLX_GREEN_SIZE, 8,
        GLX_BLUE_SIZE, 8,
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

    Window win = XCreateWindow(display, root, 100, 100, 800, 600, 0, CopyFromParent, InputOutput, CopyFromParent, 0, NULL);
    XSelectInput(display, win, KeyPressMask | KeyReleaseMask | ExposureMask | StructureNotifyMask);
    XMapWindow(display, win);
    XStoreName(display, win, "4D Tesseract");

    // Wait for window to get focus
    XFlush(display);
    usleep(100000);

    // Try to grab keyboard
    int grabResult = XGrabKeyboard(display, win, True, GrabModeAsync, GrabModeAsync, CurrentTime);
    std::cout << "Keyboard grab result: " << grabResult << std::endl;

    glXMakeContextCurrent(display, win, win, ctx);
    std::cout << "Made GL context current" << std::endl;

    if (!gladLoadGLLoader((GLADloadproc)glXGetProcAddress)) {
        std::cerr << "Failed to load GL\n";
        return -1;
    }
    std::cout << "GL loaded" << std::endl;

    // Load model (two-pass: count then read)
    FILE* file = fopen("model.dky", "r");
    if (!file) {
        std::cerr << "Failed to open model\n";
        return -1;
    }

    unsigned int vertexCount = 0, indexCount = 0;
    int section = 0;
    char line[256];
    while (fgets(line, sizeof(line), file)) {
        if (line[0] == '/' && line[1] == '/') continue;
        if (line[0] == '\n' || line[0] == '\r' || line[0] == '\0') continue;
        if (line[0] == 'f' && line[1] == 'a') { section = 1; continue; }
        if (section == 0) vertexCount++;
        else indexCount += 3;
    }

    float* vertices = new float[vertexCount * 6];
    unsigned int* indices = new unsigned int[indexCount];

    rewind(file);
    section = 0;
    unsigned int vi = 0, ii = 0;
    while (fgets(line, sizeof(line), file)) {
        if (line[0] == '/' && line[1] == '/') continue;
        if (line[0] == '\n' || line[0] == '\r' || line[0] == '\0') continue;
        if (line[0] == 'f' && line[1] == 'a') { section = 1; continue; }
        if (section == 0) {
            float x, y, z, w, u, v;
            if (sscanf(line, "%f %f %f %f %f %f", &x, &y, &z, &w, &u, &v) == 6) {
                int off = vi * 6;
                vertices[off] = x; vertices[off+1] = y;
                vertices[off+2] = z; vertices[off+3] = w;
                vertices[off+4] = u; vertices[off+5] = v;
                vi++;
            }
        } else {
            unsigned int i0, i1, i2;
            if (sscanf(line, "%u %u %u", &i0, &i1, &i2) == 3) {
                indices[ii] = i0; indices[ii+1] = i1; indices[ii+2] = i2;
                ii += 3;
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
    GLint uAngle4d = glGetUniformLocation(program, "angle4d");
    GLint uAngle4dY = glGetUniformLocation(program, "angle4dY");
    GLint uAngle4dZ = glGetUniformLocation(program, "angle4dZ");
    GLint uTranslation = glGetUniformLocation(program, "translation");
    glUniform1i(glGetUniformLocation(program, "uTexture"), 0);

    float angle4d = 0.0f;
    float angle4dY = 0.0f;
    float angle4dZ = 0.0f;
    float transX = 0.0f, transY = 0.0f, transZ = 0.0f, transW = 0.0f;

    int keys[256] = {0};
    bool running = true;

    glEnable(GL_DEPTH_TEST);

    while (running) {
        // Handle events without blocking
        while (XPending(display)) {
            XEvent evt;
            XNextEvent(display, &evt);
            if (evt.type == KeyPress || evt.type == KeyRelease) {
                int keycode = evt.xkey.keycode;
                keys[keycode] = (evt.type == KeyPress);
            }
            if (evt.type == DestroyNotify) { running = false; break; }
        }
        if (!running) break;

        // Also poll key states directly
        char keys_return[32];
        XQueryKeymap(display, keys_return);
        for (int i = 0; i < 256; i++) {
            if (keys_return[i/8] & (1 << (i%8))) {
                keys[i] = 1;
            }
        }

        // Escape to quit
        if (keys[9]) running = false;

        // Translation (WASDQE) - using X11 keycodes
        // w=25, s=39, a=38, d=40, q=24, e=26
        if (keys[25]) transX += 0.02f;
        if (keys[39]) transX -= 0.02f;
        if (keys[38]) transY += 0.02f;
        if (keys[40]) transY -= 0.02f;
        if (keys[24]) transZ += 0.02f;
        if (keys[26]) transZ -= 0.02f;

        // 4D W translation (t=28, y=29)
        if (keys[28]) transW += 0.02f;
        if (keys[29]) transW -= 0.02f;

        // Manual rotation (IJKLUO) - i=31, k=45, j=44, l=46, u=30, o=32
        if (keys[31]) angle4d += 0.02f;
        if (keys[45]) angle4d -= 0.02f;
        if (keys[44]) angle4dY += 0.02f;
        if (keys[46]) angle4dY -= 0.02f;
        if (keys[30]) angle4dZ += 0.02f;
        if (keys[32]) angle4dZ -= 0.02f;

        // Auto-rotate if no manual input
        bool manual = keys[31] || keys[45] || keys[44] || keys[46] || keys[30] || keys[32];
        if (!manual) {
            angle4d += 0.005f;
            angle4dY += 0.003f;
            angle4dZ += 0.002f;
        }

        glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUniform1f(uAngle4d, angle4d);
        glUniform1f(uAngle4dY, angle4dY);
        glUniform1f(uAngle4dZ, angle4dZ);
        glUniform4f(uTranslation, transX, transY, transZ, transW);

        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);

        glXSwapBuffers(display, win);
        usleep(16667); // ~60 FPS
    }

    delete[] vertices;
    delete[] indices;
    glXMakeContextCurrent(display, None, None, NULL);
    glXDestroyContext(display, ctx);
    XDestroyWindow(display, win);
    XCloseDisplay(display);

    return 0;
}