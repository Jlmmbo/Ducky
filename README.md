# Ducky

A real-time N-dimensional object viewer that renders hyperdimensional geometry through recursive perspective projection.

## Origin

The name "Ducky" comes from the evolution: **4D vectors** → quad-vect → quad-ec → quack → duck → ducky

## Overview

Ducky visualizes N-dimensional objects (3D, 4D, 5D, ...) by recursively projecting them down through each dimension (N-D → (N-1)-D → ... → 3D) then into 2D for display. It supports full N-D rotation and translation with real-time rendering using OpenGL. The number of dimensions is read from the model file, so any dimension count (3+) works automatically.

## Features

- **N-D Perspective Projection**: Recursively projects N-D geometry down to 3D, then to 2D screen space
- **N-D Rotation**: All N×(N-1)/2 rotation planes supported (first 6 have key bindings, rest auto-rotate)
- **N-D Translation**: Move through all N dimensions
- **Face-Colored Rendering**: Distinct HSL-based colors per face
- **Coordinate Axes**: Visualize all N axes with distinct colors
- **Wireframe Overlay**: Auto-generated edges from mesh topology
- **Custom Model Format**: Simple `.dky` format with `dims N` header for defining N-D meshes
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

### Translation (N-D Movement)
| Key | Action |
|-----|--------|
| `A` / `D` | Move in dimension 0 (X) |
| `W` / `S` | Move in dimension 1 (Y) |
| `Q` / `E` | Move in dimension 2 (Z) |
| `Z` / `X` | Move in dimension 3 (W) |
| `T` / `G` | Move in dimension 4 |
| `B` / `H` | Move in dimension 5 |

### Rotation (N-D Planes)
| Key | Action |
|-----|--------|
| `1` / `2` | Rotate in plane 0-1 |
| `3` / `4` | Rotate in plane 0-2 |
| `5` / `6` | Rotate in plane 1-2 |
| `7` / `8` | Rotate in plane 0-3 |
| `9` / `0` | Rotate in plane 1-3 |
| `-` / `=` | Rotate in plane 2-3 |

Planes beyond the first 6 auto-rotate at a slow pace.

### Other
| Key | Action |
|-----|--------|
| `R` | Reset all rotations and translations |

## .dky Model Format

The custom `.dky` format defines N-dimensional meshes with a dimension header, vertices, and faces:

```
dims [N]
// Comments start with //
[p1] [p2] ... [pN] [r] [g] [b]
...
face
[index1] [index2] [index3]
...
```

- **Header**: `dims N` sets the spatial dimension count (must be 3+)
- **Vertices**: N + 3 values per vertex — N position coordinates followed by RGB color
- **Faces**: `face` header followed by triangle indices (0-based)

Example from `model.dky`:
```
dims 4
// Tesseract vertex: x y z w r g b
-0.5 -0.5 -0.5 -0.5 0.0 0.0 0.0

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
│   ├── dimensions.hpp # N-D coordinate math (unused in current build)
│   ├── 4d.hpp        # 4D vector math (unused in current build)
│   ├── 3d.hpp        # 3D utilities (unused in current build)
│   ├── 2d.hpp        # 2D utilities (unused in current build)
│   ├── camera.hpp    # Camera controls (unused in current build)
│   ├── render.hpp    # Rendering helpers (unused in current build)
│   ├── glad.c        # OpenGL loader
│   ├── stb_image.h   # Texture loading
│   └── stb_easy_font.h # Text rendering
├── shaders/          # GLSL shaders
│   ├── tesseract.vert # 3D→2D projection vertex shader
│   ├── tesseract.frag # Flat-colored fragment shader
│   ├── axes.vert     # Coordinate axes vertex shader
│   ├── axes.frag
│   ├── edge.vert     # Wireframe edge vertex shader
│   ├── edge.frag
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
