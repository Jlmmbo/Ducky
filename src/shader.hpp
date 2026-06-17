#pragma once

#include <string>
#include <iostream>
#include <fstream>
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>

inline std::string loadFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to load file: " << path << std::endl;
        return "";
    }
    return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

inline bool checkShaderCompile(GLuint shader, const std::string& type) {
    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(shader, 512, nullptr, log);
        std::cerr << type << " shader error:\n" << log << std::endl;
        return false;
    }
    return true;
}

inline bool checkProgramLink(GLuint program) {
    int success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char log[512];
        glGetProgramInfoLog(program, 512, nullptr, log);
        std::cerr << "Program link error:\n" << log << std::endl;
        return false;
    }
    return true;
}

inline GLuint createShaderProgram(const std::string& vertPath, const std::string& fragPath) {
    std::string vertSrc = loadFile(vertPath);
    std::string fragSrc = loadFile(fragPath);
    if (vertSrc.empty() || fragSrc.empty()) return 0;

    const char* vertCStr = vertSrc.c_str();
    GLuint vertShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertShader, 1, &vertCStr, nullptr);
    glCompileShader(vertShader);
    if (!checkShaderCompile(vertShader, "Vertex")) {
        glDeleteShader(vertShader);
        return 0;
    }

    const char* fragCStr = fragSrc.c_str();
    GLuint fragShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragShader, 1, &fragCStr, nullptr);
    glCompileShader(fragShader);
    if (!checkShaderCompile(fragShader, "Fragment")) {
        glDeleteShader(vertShader);
        glDeleteShader(fragShader);
        return 0;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vertShader);
    glAttachShader(program, fragShader);
    glLinkProgram(program);
    if (!checkProgramLink(program)) {
        glDeleteShader(vertShader);
        glDeleteShader(fragShader);
        glDeleteProgram(program);
        return 0;
    }

    glDeleteShader(vertShader);
    glDeleteShader(fragShader);

    return program;
}

inline GLuint createShaderProgramFromSrc(const char* vertSrc, const char* fragSrc) {
    GLuint vertShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertShader, 1, &vertSrc, nullptr);
    glCompileShader(vertShader);
    checkShaderCompile(vertShader, "UI Vertex");

    GLuint fragShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragShader, 1, &fragSrc, nullptr);
    glCompileShader(fragShader);
    checkShaderCompile(fragShader, "UI Fragment");

    GLuint program = glCreateProgram();
    glAttachShader(program, vertShader);
    glAttachShader(program, fragShader);
    glLinkProgram(program);
    checkProgramLink(program);

    glDeleteShader(vertShader);
    glDeleteShader(fragShader);
    return program;
}
