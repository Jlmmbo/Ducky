cmake_minimum_required(VERSION 3.16)
project(Ducky VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

include_directories(${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src)

find_package(OpenGL REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Widgets OpenGLWidgets)

add_executable(ducky
    src/main.cpp
    src/ducky_view.cpp
    src/ducky_window.cpp
)

target_sources(ducky PRIVATE
    src/main.hpp
    src/transform.hpp
    src/shader.hpp
    src/render.hpp
    src/io.hpp
)

target_link_libraries(ducky
    Qt6::Widgets
    Qt6::OpenGLWidgets
    OpenGL::GL
)

if(EXISTS ${CMAKE_SOURCE_DIR}/model.dky)
    file(COPY ${CMAKE_SOURCE_DIR}/model.dky DESTINATION ${CMAKE_BINARY_DIR})
endif()
if(EXISTS ${CMAKE_SOURCE_DIR}/model_5d.dky)
    file(COPY ${CMAKE_SOURCE_DIR}/model_5d.dky DESTINATION ${CMAKE_BINARY_DIR})
endif()
if(EXISTS ${CMAKE_SOURCE_DIR}/shaders)
    file(COPY ${CMAKE_SOURCE_DIR}/shaders DESTINATION ${CMAKE_BINARY_DIR})
endif()

target_compile_definitions(ducky PRIVATE SHADER_DIR="${CMAKE_SOURCE_DIR}/shaders")
