#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <cmath>

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

    // Triangle vertices
    float vertices[] = {
        // x,   y,   u,   v
        .0f,  .0f, .0f, .0f,
        .5f,  .0f, .5f, .0f,
        .5f, -.2f, .5f, -.2f
    };

    unsigned int VBO, VAO; // create vertex and vertex array buffer objects
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO); // use VAO to store vertex array

    glBindBuffer(GL_ARRAY_BUFFER, VBO); // use VBO to store vertex data
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW); // tell gpu what to store in its VBO. STATIC because transformations happen in a shader, not on CPU

    // Position attribute (location 0)
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Texture coordinate attribute (location 1)
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Shader program
    // shaders are stored as handles
    unsigned int vertshader = compileShader(GL_VERTEX_SHADER, vertexShaderSrc);
    unsigned int fragshader = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSrc);
    unsigned int shaderProgram = glCreateProgram(); // create a shader program
    glAttachShader(shaderProgram, vertshader);
    glAttachShader(shaderProgram, fragshader);
    glLinkProgram(shaderProgram);// create the executable shader program by linking the compiled shaders together

    glDeleteShader(vertshader); // we dont need them anymore
    glDeleteShader(fragshader);

    // Texture stuff
    // load image
    int width, height, channels;
    unsigned char* data = stbi_load("src/00001.png", &width, &height, &channels, 4);
    if (!data) {
        std::cerr << "Failed to load texture\n";
        return -1;
    }

    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
             width, height, 0,
             GL_RGBA, GL_UNSIGNED_BYTE, data);


             /////////////////////////////////////AI/////////////////////////////

    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    //////////////////////////////////////////////////////////////////

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
        glDrawArrays(GL_TRIANGLES, 0, 3); // call shaders

        glfwSwapBuffers(window); // update frame
        glfwPollEvents();
    }

    return 0;
}