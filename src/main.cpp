#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <cmath>
#include <cstring>

#include "main.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// Vertex shader (how to transform vertices)
const char* vertexShaderSrc = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;

out vec2 vTexCoord;

uniform float angle;

void main() {
    float c = cos(angle);
    float s = sin(angle);

    mat2 rot = mat2(
        c, -s,
        s,  c
    );

    gl_Position = vec4(rot * aPos, 0.0, 1.0);
    vTexCoord = aTexCoord;
}
)";

// Fragment shader (what color to draw pixels)
const char* fragmentShaderSrc = R"(
#version 330 core

in vec2 vTexCoord;
out vec4 FragColor;

uniform sampler2D uTexture;

void main() {
    FragColor = texture(uTexture, vTexCoord);
}
)";

static void checkShader(unsigned int shader) {
    int success;
    char log[512];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(shader, 512, NULL, log);
        std::cout << "Shader error:\n" << log << std::endl;
    }
}

static unsigned int compileShader(unsigned int type, const char* src) {
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, NULL);
    glCompileShader(shader);
    checkShader(shader);
    return shader;
}

int main() {
    glfwInit(); // initialize GLFW

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(1920, 1920, "RRT", NULL, NULL); // create a window
    glfwMakeContextCurrent(window); // make the OpenGL context current (enable GPU communication)

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to init GLAD\n";
        return -1;
    } // tell OpenGL where to find the OpenGL functions

    glViewport(0, 0, 1920, 1920);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

Model model = LoadModel("model.dky");
    std::cout << "Loaded " << model.vertexCount << " vertices, " << model.indexCount << " indices\n";
    std::cout << "Vertices: ";
    for (unsigned int i = 0; i < model.vertexCount * 4; i++) std::cout << model.vertices[i] << " ";
    std::cout << "\nIndices: ";
    for (unsigned int i = 0; i < model.indexCount; i++) std::cout << model.indices[i] << " ";
    std::cout << "\n";
    if (!model.vertices) {
        std::cerr << "Vertices is NULL!\n";
        return -1;
    }

    unsigned int VBO, VAO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);
    std::cout << "After gen buffers\n";

    glBindVertexArray(VAO);
    std::cout << "After bind VAO\n";

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, model.vertexCount * 4 * sizeof(float), model.vertices, GL_STATIC_DRAW);
    std::cout << "After buffer VBO\n";

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, model.indexCount * sizeof(unsigned int), model.indices, GL_STATIC_DRAW);
    std::cout << "After buffer EBO\n";

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Shader program
    // shaders are stored as handles
    unsigned int vertshader = compileShader(GL_VERTEX_SHADER, vertexShaderSrc);
    unsigned int fragshader = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSrc);
    unsigned int shaderProgram = glCreateProgram(); // create a shader program
    glAttachShader(shaderProgram, vertshader);
    glAttachShader(shaderProgram, fragshader);
    glLinkProgram(shaderProgram);

    int success;
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        char log[512];
        glGetProgramInfoLog(shaderProgram, 512, NULL, log);
        std::cout << "Program link error:\n" << log << "\n";
    }

    glDeleteShader(vertshader); // we dont need them anymore
    glDeleteShader(fragshader);

    GLenum err = glGetError();
    if (err) std::cout << "GL error after shader setup: " << err << "\n";

    // Texture stuff
    // load image
    int width, height, channels;
    unsigned char* data = stbi_load("00001.png", &width, &height, &channels, 4);
    if (!data) {
        std::cerr << "Failed to load texture\n";
        return -1;
    }
    std::cout << "Loaded texture: " << width << "x" << height << " channels=" << channels << "\n";

    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
             width, height, 0,
             GL_RGBA, GL_UNSIGNED_BYTE, data);

    stbi_image_free(data);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glUniform1i(glGetUniformLocation(shaderProgram, "uTexture"), 0);

    glUseProgram(shaderProgram); //choose which program to use

    float angle = 0.0f;

    while (!glfwWindowShouldClose(window)) {
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        angle += 0.01f;

        int angleLoc = glGetUniformLocation(shaderProgram, "angle");
        glUniform1f(angleLoc, angle);

        glBindVertexArray(VAO); // update vertex positions
        GLenum err = glGetError();
        if (err) std::cout << "GL error before clear: " << err << "\n";

        glDrawElements(GL_TRIANGLES, model.indexCount, GL_UNSIGNED_INT, 0);

        err = glGetError();
        if (err) std::cout << "GL error after draw: " << err << "\n";

        glfwSwapBuffers(window); // update frame
        glfwPollEvents();
    }

    return 0;
}