#include <GLFW/glfw3.h>
#include <iostream>

int main() {
    std::cout << "1. Before glfwInit" << std::endl;
    if (!glfwInit()) {
        std::cerr << "Failed to init GLFW\n";
        return -1;
    }
    std::cout << "2. After glfwInit" << std::endl;
    
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);  // Invisible window for testing
    
    GLFWwindow* window = glfwCreateWindow(640, 480, "Test", NULL, NULL);
    if (!window) {
        std::cerr << "Failed to create window\n";
        glfwTerminate();
        return -1;
    }
    std::cout << "3. Window created" << std::endl;
    
    glfwMakeContextCurrent(window);
    std::cout << "4. Context made current" << std::endl;
    
    glfwDestroyWindow(window);
    glfwTerminate();
    std::cout << "5. Done!" << std::endl;
    return 0;
}