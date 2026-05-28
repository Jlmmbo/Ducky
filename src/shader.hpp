#pragma once

#include <string>
#include <iostream>
#include <fstream>
#include <glad/glad.h>

std::string loadFile(const std::string& path);
void checkShaderCompile(GLuint shader, const std::string& type);
void checkProgramLink(GLuint program);
GLuint createShaderProgram(const std::string& vertPath, const std::string& fragPath);
GLuint createShaderProgramFromSrc(const char* vertSrc, const char* fragSrc);
