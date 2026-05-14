# Bugs

## 1. `rotatePlane()` never defined (linker error)

**Files:** `src/2d.hpp:22`, `src/3d.hpp:27-29`, `src/4d.hpp:28-33`

`rotatePlane()` is called in all three vector math headers but is never declared or defined anywhere in the project. This will cause a linker error if any code path that calls these functions is compiled (e.g. `dimensions.hpp` or `render.hpp`).

## 2. `rotate4d()` return value discarded in `dimensions.hpp`

**File:** `src/dimensions.hpp:10`, `src/dimensions.hpp:44`

`rotate4d()` returns a `Vecf4` by value, but the result is never captured:

```cpp
rotate4d(point, camera.pos, camera.rotation);  // returned value discarded
point -= camera.pos;  // operates on un-rotated point
```

The rotation is silently never applied.

## 3. `rotate3d()` return value discarded in `dimensions.hpp`

**File:** `src/dimensions.hpp:29`

Same pattern as #2 — `rotate3d()` returns by value but the result is thrown away, so the 3D rotation in `project2d()` is never applied.

## 4. Missing closing brace in `renderPoly()`

**File:** `src/render.hpp:10-28`

`renderPoly()` opens its function body on line 10 but is missing the closing brace. The `}` on line 28 closes the outer `for` loop, leaving the function unclosed. This is a compilation error.

## 5. VLA (Variable Length Array) in C++ code

**File:** `src/render.hpp:11`

```cpp
Vecf2 screenVerts[n];
```

VLAs are not standard C++ (they are a C99 feature). This will fail to compile with MSVC and other strict-conforming compilers.

## 6. Potentially infinite loop from float accumulation

**File:** `src/render.hpp:23`

```cpp
for(float x = 0; x < 1; x += 0.01){
```

`0.01` is not exactly representable in binary floating point. Accumulating it may cause `x` to never land exactly on `1.0`, and rounding behavior could cause the loop to run more or fewer iterations than intended.

## 7. Empty loop body / dead code

**File:** `src/render.hpp:23-26`

The inner loop of `renderPoly()` does nothing — it calls `interpol()` but discards the result. The function has no rendering logic and is a stub. It is also never included/used anywhere (dead code).

## 8. Missing `#include <iostream>` in `main.hpp`

**File:** `src/main.hpp`

`std::cerr` is used on line 15, but `<iostream>` is not included. This compiles only by accident because `main.cpp` includes `<iostream>` before `#include "main.hpp"`.

## 9. Text can be occluded by 3D geometry

**File:** `src/main.cpp:315-317`

`GL_DEPTH_TEST` is enabled globally (line 153) but never disabled for text rendering. The text has NDC z=0.0 while geometry covers the full [-1, +1] depth range. With the default `GL_LESS` depth function, text pixels where geometry has a smaller depth (closer) will be hidden, potentially making the HUD partially invisible.

## 10. Memory leak: model data never freed

**File:** `src/main.hpp`, `src/main.cpp`

`LoadModel()` allocates `model.vertices` and `model.indices` with `new[]`, but they are never `delete[]`d after use (either during or after the render loop).

## 11. Shader perspective clamps distort geometry

**Files:** `shaders/tesseract.vert:54-59`, `shaders/axes.vert:50-55`

```glsl
float scale4d = clamp(DIST_4D / wDepth, 0.1, 10.0);
float perspDiv = max(zDepth, 1.0);
```

Clamping the perspective division prevents division-by-zero but introduces visible distortion: objects farther than `z=2.0` stop shrinking (perspDiv floor at 1.0), and the 4D projection scale is capped at 10x. When `w == DIST_4D` (i.e. exactly at the 4D camera distance), `wDepth = 0`, causing a division by zero that GLSL resolves to `inf`, then clamped to 10.0 — geometry discontinuously snaps to max scale.
