#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <cmath>

// Vertex shader (how to transform vertices)
const char* vertexShaderSrc = R"(
#version 330 core
layout(location = 0) in vec2 aPos;

uniform float angle;

void main() {
    float c = cos(angle);
    float s = sin(angle);

    mat2 rot = mat2(
        c, -s,
        s,  c
    );

    gl_Position = vec4(rot * aPos, 0.0, 1.0);
}
)";

// Fragment shader (what color to draw pixels)
const char* fragmentShaderSrc = R"(
#version 330 core
out vec4 FragColor;

void main() {
    FragColor = vec4(1.0, 0.0, 0.0, 1.0); // red
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

    GLFWwindow* window = glfwCreateWindow(1080, 1080, "RRT", NULL, NULL); // create a window
    glfwMakeContextCurrent(window); // make the OpenGL context current (enable GPU communication)

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to init GLAD\n";
        return -1;
    } // tell OpenGL where to find the OpenGL functions

    // Triangle vertices
    float vertices[] = {
        0.0f,  0.0f,
        0.5f,  0.0f,
        0.5f, -0.2f
    };

    unsigned int VBO, VAO; // create vertex and vertex array buffer objects
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO); // use VAO to store vertex array

    glBindBuffer(GL_ARRAY_BUFFER, VBO); // use VBO to store vertex data
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW); // tell gpu what to store in its VBO. STATIC because transformations happen in a shader, not on CPU

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0); //define how to interpret vertex data
    //(index, size, type, normalized, stride(byte offset), pointer)
    glEnableVertexAttribArray(0);


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