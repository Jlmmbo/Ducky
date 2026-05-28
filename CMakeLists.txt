cmake_minimum_required(VERSION 3.16)
project(Ducky VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${CMAKE_SOURCE_DIR}/src)

# ── Library ──
add_library(ducky
    src/core.cpp
    src/renderer.cpp
    src/io.cpp
    src/shader.cpp
    src/application.cpp
    src/glad.c
)

target_include_directories(ducky
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    PRIVATE ${CMAKE_SOURCE_DIR}/src
)

# ── Dependencies ──
if(CMAKE_CROSSCOMPILING OR CMAKE_SYSTEM_NAME MATCHES "Windows")
    include(FetchContent)
    FetchContent_Declare(
        glfw
        URL https://github.com/glfw/glfw/releases/download/3.4/glfw-3.4.zip
    )
    FetchContent_MakeAvailable(glfw)
    if(NOT glfw_POPULATED)
        message(FATAL_ERROR "Failed to fetch GLFW. Please install glfw3 manually or check your internet connection.")
    endif()
    target_include_directories(ducky PRIVATE ${glfw_SOURCE_DIR}/include)
    target_link_libraries(ducky PUBLIC glfw opengl32 gdi32 user32 shell32)
    if(EXISTS ${CMAKE_SOURCE_DIR}/bin/glfw3.dll)
        file(COPY ${CMAKE_SOURCE_DIR}/bin/glfw3.dll DESTINATION ${CMAKE_BINARY_DIR})
        file(COPY ${CMAKE_SOURCE_DIR}/bin/libgcc_s_seh-1.dll DESTINATION ${CMAKE_BINARY_DIR})
        file(COPY ${CMAKE_SOURCE_DIR}/bin/libstdc++-6.dll DESTINATION ${CMAKE_BINARY_DIR})
        file(COPY ${CMAKE_SOURCE_DIR}/bin/libwinpthread-1.dll DESTINATION ${CMAKE_BINARY_DIR})
    endif()
else()
    find_package(OpenGL REQUIRED)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GLFW REQUIRED glfw3)
    target_include_directories(ducky PRIVATE ${GLFW_INCLUDE_DIRS})
    target_link_libraries(ducky PUBLIC ${OPENGL_LIBRARIES} ${GLFW_LIBRARIES})
endif()

# ── Executable ──
add_executable(ducky_app src/main.cpp)
target_link_libraries(ducky_app ducky)

# ── Copy assets to build directory ──
if(EXISTS ${CMAKE_SOURCE_DIR}/model.dky)
    file(COPY ${CMAKE_SOURCE_DIR}/model.dky DESTINATION ${CMAKE_BINARY_DIR})
endif()
if(EXISTS ${CMAKE_SOURCE_DIR}/model_3d.dky)
    file(COPY ${CMAKE_SOURCE_DIR}/model_3d.dky DESTINATION ${CMAKE_BINARY_DIR})
endif()
if(EXISTS ${CMAKE_SOURCE_DIR}/model_5d.dky)
    file(COPY ${CMAKE_SOURCE_DIR}/model_5d.dky DESTINATION ${CMAKE_BINARY_DIR})
endif()
if(EXISTS ${CMAKE_SOURCE_DIR}/shaders)
    file(COPY ${CMAKE_SOURCE_DIR}/shaders DESTINATION ${CMAKE_BINARY_DIR})
endif()
