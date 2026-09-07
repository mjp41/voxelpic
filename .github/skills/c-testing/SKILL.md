---
description: "Write, build, and run C tests for the VoxelPic library. USE FOR: adding new C test cases, understanding the test data format, debugging test failures, or running the existing C test suite."
---

# Testing the C Library

## Test Suite Overview

There are three C test executables under `test/`, each registered with CTest:

| Executable   | Tests                        | Data Files              |
|--------------|------------------------------|-------------------------|
| `voxel_test` | OcTree build round-trip      | `voxel_*_input.dat`, `voxel_*_expected.dat` |
| `color_test` | Hue-codec encode/decode      | *(none — self-contained)* |
| `image_test` | Level encode/decode to image | `image_7_cloud.dat`–`image_9_cloud.dat` |

## Building and Running

```sh
cmake -B build --preset debug      # or release / debug-clang / release-clang
cmake --build build
ctest --test-dir build -V           # -V for verbose output
```

All four CMake presets enable `VOXELPIC_BUILD_TESTS`. To run a single test:

```sh
ctest --test-dir build -R color_test -V
```

To build with AddressSanitizer (clang):

```sh
cmake -B build --preset release-clang -DVOXELPIC_SANITIZE=address
cmake --build build
ctest --test-dir build -V
```

## Test Data Generation

Test data files are **generated automatically** by Python scripts as a
post-build step (defined in `test/CMakeLists.txt`):

- `test/voxel_test_gen.py` → produces `voxel_1.dat`–`voxel_5.dat`
- `test/image_test_gen.py` → produces `image_7.dat`–`image_9.dat`

Both scripts use a fixed random seed for reproducibility, and skip generation if
the output file already exists. Delete the `.dat` files to force regeneration.

### Binary Data Format

Generated fixtures use the public point-cloud file format:

```
[int32 big-endian]   number of points/voxels (N)
  repeated N times:
    [float32 native]  x
    [float32 native]  y
    [float32 native]  z
    [uint8]           r
    [uint8]           g
    [uint8]           b
```

  The test depth is passed as a separate command-line argument. Tests load each
  fixture through `voxelpicPointCloudLoad()` so they do not duplicate the
  production file parser.

## Conventions for Writing a New C Test

### File Structure

1. Create `test/<name>_test.c`.
2. `#include <voxelpic/voxelpic.h>` and `#include "test.h"`.
3. Link against `voxelpic::voxelpic` in `test/CMakeLists.txt`.
4. Register with CTest via `add_test(...)`.

### Pass / Fail Signalling

- Return **0** from `main()` on success, **1** on assertion failure.
- CTest treats any non-zero exit code as failure.
- Use `printf()` to log diagnostics on failure (no external assertion library).

### Comparison Pattern

Compare results element-by-element and print details on mismatch. Example:

```c
for (size_t i = 0; i < cloud->size; ++i) {
  if (voxelpicVec3Compare(&actual->positions[i], &expected->positions[i])) {
    printf("Position mismatch at %zu\n", i);
    return 1;
  }
  if (actual->colors[i].value != expected->colors[i].value) {
    printf("Color mismatch at %zu\n", i);
    return 1;
  }
}
```

### Resource Cleanup

Use `goto end` with a cleanup label to ensure all `*New()` allocations are
matched by `*Free()` calls, even on error paths:

```c
int main(int argc, char *argv[]) {
  int result = 1;
  voxelpicOcTree *octree = voxelpicOcTreeNew(4, 9);
  voxelpicPointCloud *cloud = voxelpicPointCloudNew(0);
  // ... test logic ...
  result = 0;
end:
  voxelpicOcTreeFree(octree);
  voxelpicPointCloudFree(cloud);
  return result;
}
```

### Test Data Files

If the test needs generated data, add a Python generator script and a
`add_custom_command(TARGET ... POST_BUILD ...)` in `test/CMakeLists.txt`
following the existing pattern. Use `struct.pack(">i", ...)` for big-endian
header integers and native byte order for floats and uint8 colour values.

### CMakeLists.txt Registration

```cmake
add_executable(<name>_test <name>_test.c)
target_link_libraries(<name>_test PRIVATE voxelpic::voxelpic)
add_test(NAME <name>_test COMMAND <name>_test [args...]
         WORKING_DIRECTORY $<TARGET_FILE_DIR:<name>_test>)
```

## Existing Test Details

### voxel_test

Tests the full OcTree round-trip: loads a point cloud, builds an OcTree,
extracts the level back to a cloud, and compares with expected output. Also
verifies save/load I/O for both `voxelpicPointCloud` and `voxelpicLevel`.

### color_test

Tests `voxelpicValueToColor()` / `voxelpicColorToValue()` round-trip across the
full `[0, VPIC_MAX_ENCODE_VALUE]` range, plus many smaller sub-ranges. Tests
both explicit-range and implicit-range (auto-detected min/max) modes.

### image_test

Tests level-to-image encoding and image-to-level decoding. Builds an OcTree,
encodes a level into an RGBA image via `voxelpicLevelEncode()`, decodes it back
with `voxelpicLevelDecode()`, and compares the reconstructed cloud against the
original.
