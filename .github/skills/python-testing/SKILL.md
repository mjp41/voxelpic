---
description: "Write and run Python tests for the VoxelPic library. USE FOR: adding new pytest test cases, understanding the Python API and test patterns, debugging Python test failures, or running the existing Python test suite."
---

# Testing the Python Library

## Test Suite Overview

Python tests live in `test/test_python_api.py` and run with pytest. The test
file exercises the public API exposed by the `voxelpic` package — `encode`,
`decode`, `image_shape`, `voxel_size`, `PointCloud`, and `DecodeResult` — which
delegate to the `_voxelpic` C extension module.

| Test Function    | Parametrized | What It Tests                              |
|------------------|--------------|--------------------------------------------|
| `test_transcode` | depth 7,8,9  | Full encode → decode → re-encode round-trip |
| `test_io`        | no           | `PointCloud.save()` / `PointCloud.load()` binary I/O |

## Installing and Running

```sh
pip install -e .[test]    # editable install with test dependencies
pytest -vv                # run all tests from repo root
```

The `[test]` extra installs `pytest`, `pytest-md`, `pytest-emoji`, and
`pytest-cov`.

### Useful Commands

```sh
pytest test/test_python_api.py -vv                 # run only Python tests
pytest test/test_python_api.py::test_transcode -vv # single test function
pytest test/test_python_api.py::test_transcode[9]  # single parametrized case
pytest --cov=voxelpic --cov-report=term-missing    # with coverage
```

## Python API Surface

The C extension `_voxelpic` exposes four functions consumed by `src/voxelpic.py`:

| C Extension Function      | Python Wrapper              | Purpose                                |
|---------------------------|-----------------------------|----------------------------------------|
| `_voxelpic.encode()`      | `voxelpic.encode()`         | Point cloud → RGBA image               |
| `_voxelpic.decode()`      | `voxelpic.decode()`         | RGBA image → `DecodeResult`            |
| `_voxelpic.image_shape()` | `voxelpic.image_shape()`    | Image (height, width) for a depth      |
| `_voxelpic.voxel_size()`  | `voxelpic.voxel_size()`     | Voxel side length for a depth          |

### Key Types

- `PointCloud` — `NamedTuple` with `positions` (Nx4 float32) and `colors`
  (Nx4 uint8). Has `to3()` / `to4()` conversion methods and `save()` / `load()`
  for binary I/O.
- `DecodeResult` — `NamedTuple` with `positions`, `colors`, `count`, `depth`.
  Has a `.cloud` property that returns a `PointCloud`.

### Array Constraints (validated in the C extension)

- Positions: Nx4, dtype `float32`, C-contiguous.
- Colors: Nx4, dtype `uint8`, C-contiguous.
- Images: HxWx4, dtype `uint8`, C-contiguous.
- Position and color arrays must have the same N dimension.

## Conventions for Writing a New Test

### File and Imports

Add tests to `test/test_python_api.py` (or create a new `test/test_*.py` file).

```python
import numpy as np
import voxelpic as vp
import pytest
```

### Generating Test Data

Tests generate random data inline rather than reading from files:

```python
positions = np.random.rand(1000, 4).astype(np.float32) * 2 - 1
positions[:, 3] = 1  # homogeneous w coordinate
colors = np.random.randint(0, 255, (1000, 4)).astype(np.uint8)
colors[:, 3] = 255   # alpha channel
cloud = vp.PointCloud(positions, colors)
```

### Parametrization

Use `@pytest.mark.parametrize` for depth variation:

```python
@pytest.mark.parametrize("depth", [7, 8, 9])
def test_something(depth):
    ...
```

### Assertions

Use NumPy testing assertions for array comparisons:

```python
np.testing.assert_array_equal(actual, expected)          # exact (uint8, int)
np.testing.assert_array_almost_equal(actual, expected)    # float tolerance
```

Use standard `assert` for scalar values:

```python
assert result.depth == depth
assert image.shape == (height, width, 4)
```

### Fixtures

Use pytest built-in fixtures. `tmp_path` provides a temporary directory per
test, useful for I/O tests:

```python
def test_io(tmp_path):
    path = str(tmp_path / "cloud.dat")
    cloud.save(path)
    loaded = vp.PointCloud.load(path)
```

No custom fixtures or `conftest.py` exist — tests are self-contained.

### Round-Trip Testing Pattern

The primary pattern is encode → decode → re-encode, verifying that the second
encode produces an identical image:

```python
image = vp.encode(cloud, depth=depth)
result = vp.decode(image)
image2 = vp.encode(result.cloud, depth=depth)
np.testing.assert_array_equal(image, image2)
```

### Testing Preallocated Buffers

Both `encode` and `decode` accept optional preallocated output arrays. Test both
the default allocation path and the preallocated path:

```python
# Default allocation
image = vp.encode(cloud, depth=depth)

# Preallocated output
h, w = vp.image_shape(depth)
out = np.empty((h, w, 4), dtype=np.uint8)
image2 = vp.encode(cloud, depth=depth, out_image=out)
```

## Configuration

There is no `pytest.ini`, `conftest.py`, or `[tool.pytest.ini_options]` section.
Pytest uses default discovery: files matching `test_*.py` with functions matching
`test_*`.
