# Ducky

A real-time 4D object viewer that renders hyperdimensional geometry through perspective projection.

## Origin

The name "Ducky" comes from the evolution: **4D vectors** → quad-vect → quad-ec → quack → duck → ducky

## Overview

Ducky visualizes 4D objects (currently a tesseract/hypercube) by projecting them into 3D space, then into 2D for display. It supports full 4D rotation and translation with real-time rendering using OpenGL.

## Features

- **4D Perspective Projection**: Projects 4D geometry to 3D, then to 2D screen space
- **6 Rotation Planes**: Rotate in XY, XZ, XW, YZ, YW, ZW planes
- **4D Translation**: Move through X, Y, Z, and W dimensions
- **Textured Rendering**: Apply 2D textures to 4D geometry with UV mapping
- **Coordinate Axes**: Visualize X (red), Y (green), Z (blue), and W (purple) axes
- **Custom Model Format**: Simple `.dky` format for defining 4D meshes
- **Cross-Platform**: Builds on Linux and Windows (via cross-compilation)

## Building

### Prerequisites

- CMake 3.16+
- C++17 compiler
- OpenGL 3.3+
- GLFW (fetched automatically on Windows, system package on Linux)

### Linux

```bash
mkdir build && cd build
cmake ..
make
./ducky
```

### Windows (Cross-compile from Linux)

```bash
mkdir build && cd build
cmake -DCMAKE_TOOLCHAIN_FILE=/usr/share/mingw-w64/cmake/toolchain.cmake ..
make
wine ducky.exe
```

Or use the manual compile command:

```bash
x86_64-w64-mingw32-g++ src/glad.c src/main.cpp \
  -Iinclude \
  -Lexternal/glfw/build-win/src \
  -lglfw3 -lopengl32 -lgdi32 -luser32 -lshell32 \
  -o bin/ducky.exe
```

## Controls

### Translation (4D Movement)
| Key | Action |
|-----|--------|
| `W` / `S` | Move in Y axis |
| `A` / `D` | Move in X axis |
| `Q` / `E` | Move in Z axis |
| `Z` / `X` | Move in W axis |

### Rotation (4D Planes)
| Key | Action |
|-----|--------|
| `1` / `2` | Rotate in XY plane |
| `3` / `4` | Rotate in XZ plane |
| `5` / `6` | Rotate in XW plane |
| `7` / `8` | Rotate in YZ plane |
| `9` / `0` | Rotate in YW plane |
| `-` / `=` | Rotate in ZW plane |

## .dky Model Format

The custom `.dky` format defines 4D meshes with vertices and faces:

```
// Comments start with //
[x1] [y1] [z1] [w1] [u1] [v1]
[x2] [y2] [z2] [w2] [u2] [v2]
...
face
[index1] [index2] [index3]
...
```

- **Vertices**: 6 values per vertex (x, y, z, w, u, v) where (x,y,z,w) is the 4D position and (u,v) are texture coordinates
- **Faces**: `face` header followed by triangle indices (0-based)

Example from `model.dky`:
```
// Tesseract vertex
-0.5 -0.5 -0.5 -0.5 0.0 0.0

// Face definition
face
0 1 2
```

## Project Structure

```
Ducky/
├── src/              # Source files
│   ├── main.cpp      # Main application & rendering loop
│   ├── main.hpp      # Model loading (LoadModel)
│   ├── 4d.hpp        # 4D vector & rotation math
│   ├── 3d.hpp        # 3D utilities
│   ├── 2d.hpp        # 2D utilities
│   ├── camera.hpp    # Camera controls
│   ├── render.hpp    # Rendering helpers
│   ├── glad.c        # OpenGL loader
│   ├── stb_image.h   # Texture loading
│   └── stb_easy_font.h # Text rendering
├── shaders/          # GLSL shaders
│   ├── tesseract.vert # 4D→3D→2D projection shader
│   ├── tesseract.frag # Textured fragment shader
│   ├── axes.vert     # Coordinate axes shader
│   ├── axes.frag
│   ├── text.vert     # On-screen text shader
│   └── text.frag
├── include/          # Headers (GLFW, GLAD, KHR)
├── bin/              # Compiled binaries
├── model.dky         # Default 4D model (tesseract)
├── 00001.png         # Default texture
└── CMakeLists.txt    # Build configuration
```

## Dependencies

- **GLFW 3.4** - Window management & input (fetched via FetchContent for Windows)
- **GLAD** - OpenGL function loader (included in src/)
- **stb_image** - Texture loading (included in src/)
- **stb_easy_font** - Bitmap font rendering (included in src/)

## License

MIT License - see [LICENSE](LICENSE) for details

## Author

Jlmmbo (2026)
