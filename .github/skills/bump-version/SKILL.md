---
description: "Bump the VoxelPic version to a user-specified value. USE FOR: setting or updating the project version across all files that embed it. Applies to requests like 'bump version to X.Y.Z', 'set version', or 'update version'."
---

# Bump Version

## When to Use

Use this skill when the user asks to set, bump, or update the project version.

## Version Locations

The version string (semver `MAJOR.MINOR.PATCH`) must be updated in **all four**
of the following files. Missing any one will cause a mismatch between the C
library, the Python package, and the citation metadata.

### 1. `VERSION` (root)

Single-line file containing the bare version string. This is the source of truth
for CMake — the root `CMakeLists.txt` reads it at configure time and derives
`VOXELPIC_VERSION_MAJOR`, `VOXELPIC_VERSION_MINOR`, and
`VOXELPIC_VERSION_REVISION`, which are substituted into `tools/version.h.in` to
produce the `vpic` tool's build-info header.

```
X.Y.Z
```

### 2. `include/voxelpic/voxelpic.h` (line ~9)

The public C header defines a string macro used by consumers of the library:

```c
#define VOXELPIC_VERSION "X.Y.Z"
```

### 3. `pyproject.toml` (under `[project]`)

The Python package version is hardcoded here (not derived from the `VERSION`
file):

```toml
version = "X.Y.Z"
```

### 4. `CITATION.cff` (field `version`)

Academic citation metadata:

```yaml
version: X.Y.Z
```

Also update the `date-released` field to today's date (`YYYY-MM-DD`).

## Procedure

1. Ask the user for the target version if not already provided.
2. Edit all four files listed above, replacing the old version with the new one.
3. Update the `date-released` field in `CITATION.cff` to today's date.
4. Verify there are no other stale version references by searching the codebase
   for the old version string (exclude `build/`, `build-*/`, and `__pycache__/`
   directories).
5. Summarise the changes made.

## Notes

- `tools/version.h.in` does **not** need editing — it uses CMake variables
  populated from the `VERSION` file at configure time.
- `CMakeLists.txt` does **not** need editing — it reads from the `VERSION` file.
- After bumping, the user should reconfigure CMake (`cmake -B build --preset
  release`) so the generated `version.h` picks up the new value.
