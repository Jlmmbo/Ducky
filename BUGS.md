# Bugs

All known bugs listed here have been fixed. This file is kept for historical reference.

## Fixed

- **Rotation key bindings broken** — Restored paired (+/-) behavior
- **`pushUndo` called on every left click** — Now scoped to slider hits only
- **Missing `#include <cstdio>`** — Added
- **`#include <sstream>` unused** — Removed
- **`float vals[32]` overflow for dims > 29** (`src/main.hpp:82`) — Changed to `std::vector<float> vals(fpv)`
- **No index validation against vertex count** — Added bounds check in index parsing
- **Case-sensitive header parsing** — Added `DIMS`, `Dims` variants alongside `dims`
- **`float pos[16]` stack overflow** — Changed to `alloca(dims * sizeof(float))`
- **`float origin[16]` / `float tip[16]` stack overflow** — Changed to `alloca`
- **Dead code: `accumulator` / `angleAccum`** — Removed
- **Hint text says `[=reset`** — Corrected to `[=focal`
- **`loadState` returns true on partial read failure** — Now returns false on failure
- **`alloca` not portable** — Added platform headers (`<malloc.h>` / `<alloca.h>`)
- **`DIST_3D = 3.0` hardcoded in vertex shaders** — Converted to `uniform float uDist3D` in all three shaders; passed as `3.0f * focalLength` from C++
- **Duplicate edges from vertex duplication** — `generateEdges` now deduplicates via canonical vertex positions; tesseract: 1152→32 edges, penteract: 8000→80 edges
- **Fallback edge count O(n*m) linear search** — Replaced with `std::map<std::pair<int,int>, int>` for O(log n) lookups
- **Right-click orbit conflicts with slider** — Orbit now checks mouse position is outside slider panel before engaging
- **Fullscreen restore no monitor bounds validation** — Restore position/size clamped to monitor workarea
- **Screenshot counter wraps silently** — Changed to timestamp-based naming (`screenshot_YYYYMMDD_HHMMSS.tga`)
- **SDL3.dll copied for Windows build** — Removed from CMakeLists.txt
- **No GLFW fetch fallback on Windows** — Added `if(NOT glfw_POPULATED)` error message
- **Model format `"faces"` → `"face"`** — Changed in both `model.dky` and `model_5d.dky`
- **README out of date** — Fully updated with all new key bindings, features, and controls

## Unchanged (Intentional / Future)

- **O(n²) edge generation for coordinate differencing**: Acceptable for typical model sizes (< 1000 vertices). The real fix was deduplication, not complexity reduction.
- **Nearly identical vertex shaders**: Consolidating into a shared shader would add complexity without functional benefit.
- **No depth clamping / polygon offset for wireframe edges**: Drawing edges on top of faces is the desired behavior for wireframe overlay.
