#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <fstream>
#include <string>

#include "main.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_EASY_FONT_IMPLEMENTATION
#include "stb_easy_font.h"

// Constants
constexpr int WINDOW_WIDTH = 1920;
constexpr int WINDOW_HEIGHT = 1920;
constexpr float MOVE_SPEED = 0.02f;
constexpr float ROTATE_SPEED = 0.02f;
constexpr float AXIS_LENGTH = 1.5f;

// 4D Transform struct
struct Transform4D {
    float angleXY = 0.0f, angleXZ = 0.0f, angleXW = 0.0f;
    float angleYZ = 0.0f, angleYW = 0.0f, angleZW = 0.0f;
    float transX = 0.0f, transY = 0.0f, transZ = 0.0f, transW = 0.0f;
};

// Uniform location caches
struct TesseractUniforms {
    GLuint angleXY, angleXZ, angleXW, angleYZ, angleYW, angleZW;
    GLuint translation, uTexture;
};

struct AxesUniforms {
    GLuint angleXY, angleXZ, angleXW, angleYZ, angleYW, angleZW;
};

// Load file to string
static std::string loadFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to load file: " << path << std::endl;
        return "";
    }
    return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

// Shader error checking
static void checkShaderCompile(GLuint shader, const std::string& type) {
    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(shader, 512, nullptr, log);
        std::cerr << type << " shader error:\n" << log << std::endl;
    }
}

static void checkProgramLink(GLuint program) {
    int success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char log[512];
        glGetProgramInfoLog(program, 512, nullptr, log);
        std::cerr << "Program link error:\n" << log << std::endl;
    }
}

// Create shader program from files
static GLuint createShaderProgram(const std::string& vertPath, const std::string& fragPath) {
    std::string vertSrc = loadFile(vertPath);
    std::string fragSrc = loadFile(fragPath);
    if (vertSrc.empty() || fragSrc.empty()) return 0;

    const char* vertCStr = vertSrc.c_str();
    GLuint vertShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertShader, 1, &vertCStr, nullptr);
    glCompileShader(vertShader);
    checkShaderCompile(vertShader, "Vertex");

    const char* fragCStr = fragSrc.c_str();
    GLuint fragShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragShader, 1, &fragCStr, nullptr);
    glCompileShader(fragShader);
    checkShaderCompile(fragShader, "Fragment");

    GLuint program = glCreateProgram();
    glAttachShader(program, vertShader);
    glAttachShader(program, fragShader);
    glLinkProgram(program);
    checkProgramLink(program);

    // Shaders can be deleted after linking
    glDeleteShader(vertShader);
    glDeleteShader(fragShader);

    return program;
}

// Input handling
static void processInput(GLFWwindow* window, Transform4D& t) {
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) t.transX += MOVE_SPEED;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) t.transX -= MOVE_SPEED;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) t.transY += MOVE_SPEED;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) t.transY -= MOVE_SPEED;
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) t.transZ += MOVE_SPEED;
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) t.transZ -= MOVE_SPEED;
    if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS) t.transW += MOVE_SPEED;
    if (glfwGetKey(window, GLFW_KEY_X) == GLFW_PRESS) t.transW -= MOVE_SPEED;

    if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS) t.angleXY += ROTATE_SPEED;
    if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS) t.angleXY -= ROTATE_SPEED;
    if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS) t.angleXZ += ROTATE_SPEED;
    if (glfwGetKey(window, GLFW_KEY_4) == GLFW_PRESS) t.angleXZ -= ROTATE_SPEED;
    if (glfwGetKey(window, GLFW_KEY_5) == GLFW_PRESS) t.angleXW += ROTATE_SPEED;
    if (glfwGetKey(window, GLFW_KEY_6) == GLFW_PRESS) t.angleXW -= ROTATE_SPEED;
    if (glfwGetKey(window, GLFW_KEY_7) == GLFW_PRESS) t.angleYZ += ROTATE_SPEED;
    if (glfwGetKey(window, GLFW_KEY_8) == GLFW_PRESS) t.angleYZ -= ROTATE_SPEED;
    if (glfwGetKey(window, GLFW_KEY_9) == GLFW_PRESS) t.angleYW += ROTATE_SPEED;
    if (glfwGetKey(window, GLFW_KEY_0) == GLFW_PRESS) t.angleYW -= ROTATE_SPEED;
    if (glfwGetKey(window, GLFW_KEY_MINUS) == GLFW_PRESS) t.angleZW += ROTATE_SPEED;
    if (glfwGetKey(window, GLFW_KEY_EQUAL) == GLFW_PRESS) t.angleZW -= ROTATE_SPEED;
}

int main() {
    // Init GLFW (disable libdecor for Wayland compatibility)
    glfwInitHint(GLFW_WAYLAND_LIBDECOR, GLFW_WAYLAND_DISABLE_LIBDECOR);
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "4D Tesseract", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        glfwTerminate();
        return -1;
    }

    glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
    glEnable(GL_DEPTH_TEST);

    // Load model
    Model model = LoadModel("model.dky");
    std::cout << "Loaded " << model.vertexCount << " vertices, " << model.indexCount << " indices" << std::endl;

    // === Tesseract setup ===
    GLuint tessVAO, tessVBO, tessEBO;
    glGenVertexArrays(1, &tessVAO);
    glGenBuffers(1, &tessVBO);
    glGenBuffers(1, &tessEBO);

    glBindVertexArray(tessVAO);
    glBindBuffer(GL_ARRAY_BUFFER, tessVBO);
    glBufferData(GL_ARRAY_BUFFER, model.vertexCount * 6 * sizeof(float), model.vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, tessEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, model.indexCount * sizeof(unsigned int), model.indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(4 * sizeof(float)));
    glEnableVertexAttribArray(1);

    GLuint tessProgram = createShaderProgram("shaders/tesseract.vert", "shaders/tesseract.frag");
    if (!tessProgram) {
        glfwTerminate();
        return -1;
    }

    TesseractUniforms tessUni;
    tessUni.angleXY = glGetUniformLocation(tessProgram, "angleXY");
    tessUni.angleXZ = glGetUniformLocation(tessProgram, "angleXZ");
    tessUni.angleXW = glGetUniformLocation(tessProgram, "angleXW");
    tessUni.angleYZ = glGetUniformLocation(tessProgram, "angleYZ");
    tessUni.angleYW = glGetUniformLocation(tessProgram, "angleYW");
    tessUni.angleZW = glGetUniformLocation(tessProgram, "angleZW");
    tessUni.translation = glGetUniformLocation(tessProgram, "translation");
    tessUni.uTexture = glGetUniformLocation(tessProgram, "uTexture");

    // Texture setup
    int texW, texH, texC;
    unsigned char* texData = stbi_load("00001.png", &texW, &texH, &texC, 4);
    if (!texData) {
        std::cerr << "Failed to load texture" << std::endl;
        glfwTerminate();
        return -1;
    }

    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texW, texH, 0, GL_RGBA, GL_UNSIGNED_BYTE, texData);
    std::cout << "Texture loaded: " << texW << "x" << texH << " channels=" << texC << std::endl;
    stbi_image_free(texData);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glUseProgram(tessProgram);
    glUniform1i(tessUni.uTexture, 0);

    // === Axes setup ===
    GLuint axesVAO, axesVBO;
    glGenVertexArrays(1, &axesVAO);
    glGenBuffers(1, &axesVBO);

    float axesVertices[] = {
        // X (red)
        0,0,0,0, 1,0,0, AXIS_LENGTH,0,0,0, 1,0,0,
        // Y (green)
        0,0,0,0, 0,1,0, 0,AXIS_LENGTH,0,0, 0,1,0,
        // Z (blue)
        0,0,0,0, 0,0,1, 0,0,AXIS_LENGTH,0, 0,0,1,
        // W (purple)
        0,0,0,0, 0.5f,0,0.5f, 0,0,0,AXIS_LENGTH, 0.5f,0,0.5f,
    };

    glBindVertexArray(axesVAO);
    glBindBuffer(GL_ARRAY_BUFFER, axesVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(axesVertices), axesVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 7 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)(4 * sizeof(float)));
    glEnableVertexAttribArray(1);

    GLuint axesProgram = createShaderProgram("shaders/axes.vert", "shaders/axes.frag");
    if (!axesProgram) {
        glfwTerminate();
        return -1;
    }

    AxesUniforms axesUni;
    axesUni.angleXY = glGetUniformLocation(axesProgram, "angleXY");
    axesUni.angleXZ = glGetUniformLocation(axesProgram, "angleXZ");
    axesUni.angleXW = glGetUniformLocation(axesProgram, "angleXW");
    axesUni.angleYZ = glGetUniformLocation(axesProgram, "angleYZ");
    axesUni.angleYW = glGetUniformLocation(axesProgram, "angleYW");
    axesUni.angleZW = glGetUniformLocation(axesProgram, "angleZW");

    // === Text setup ===
    GLuint textProgram = createShaderProgram("shaders/text.vert", "shaders/text.frag");
    if (!textProgram) {
        glfwTerminate();
        return -1;
    }

    const char* hintText = "Controls: WASD-move XY, QE-move Z, ZX-move W, 1234567890-=-rotate planes";
    char textBuffer[20000];
    int numQuads = stb_easy_font_print(10, WINDOW_HEIGHT - 40, (char*)hintText, nullptr, textBuffer, sizeof(textBuffer));

    for (int i = 0; i < numQuads * 4; i++) {
        float* v = (float*)(textBuffer + i * 16);
        v[0] = (v[0] / WINDOW_WIDTH) * 2.0f - 1.0f;
        v[1] = 1.0f - (v[1] / WINDOW_HEIGHT) * 2.0f;
    }

    GLuint textVAO, textVBO;
    glGenVertexArrays(1, &textVAO);
    glGenBuffers(1, &textVBO);
    glBindVertexArray(textVAO);
    glBindBuffer(GL_ARRAY_BUFFER, textVBO);
    glBufferData(GL_ARRAY_BUFFER, numQuads * 64, textBuffer, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 16, nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, 16, (void*)12);
    glEnableVertexAttribArray(1);

    Transform4D transform;

    // Render loop
    while (!glfwWindowShouldClose(window)) {
        processInput(window, transform);
        glClearColor(0.05f, 0.05f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Draw tesseract
        glUseProgram(tessProgram);
        glUniform1f(tessUni.angleXY, transform.angleXY);
        glUniform1f(tessUni.angleXZ, transform.angleXZ);
        glUniform1f(tessUni.angleXW, transform.angleXW);
        glUniform1f(tessUni.angleYZ, transform.angleYZ);
        glUniform1f(tessUni.angleYW, transform.angleYW);
        glUniform1f(tessUni.angleZW, transform.angleZW);
        glUniform4f(tessUni.translation, transform.transX, transform.transY, transform.transZ, transform.transW);
        glBindVertexArray(tessVAO);
        glDrawElements(GL_TRIANGLES, model.indexCount, GL_UNSIGNED_INT, nullptr);

        // Draw axes
        glUseProgram(axesProgram);
        glUniform1f(axesUni.angleXY, transform.angleXY);
        glUniform1f(axesUni.angleXZ, transform.angleXZ);
        glUniform1f(axesUni.angleXW, transform.angleXW);
        glUniform1f(axesUni.angleYZ, transform.angleYZ);
        glUniform1f(axesUni.angleYW, transform.angleYW);
        glUniform1f(axesUni.angleZW, transform.angleZW);
        glBindVertexArray(axesVAO);
        glDrawArrays(GL_LINES, 0, 8);

        // Draw text
        glUseProgram(textProgram);
        glBindVertexArray(textVAO);
        glDrawArrays(GL_QUADS, 0, numQuads * 4);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Cleanup
    glDeleteVertexArrays(1, &tessVAO);
    glDeleteBuffers(1, &tessVBO);
    glDeleteBuffers(1, &tessEBO);
    glDeleteVertexArrays(1, &axesVAO);
    glDeleteBuffers(1, &axesVBO);
    glDeleteVertexArrays(1, &textVAO);
    glDeleteBuffers(1, &textVBO);
    glDeleteProgram(tessProgram);
    glDeleteProgram(axesProgram);
    glDeleteProgram(textProgram);
    glDeleteTextures(1, &textureID);

    glfwTerminate();
    return 0;
}
