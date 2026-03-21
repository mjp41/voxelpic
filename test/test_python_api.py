import numpy as np
import voxelpic as vp
import pytest


@pytest.mark.parametrize("depth", [7, 8, 9])
def test_transcode(depth: int):
    rng = np.random.RandomState(42 + depth)
    positions = rng.rand(1000, 4) * 2 - 1
    positions[:, 3] = 1
    positions = positions.astype(np.float32)

    colors = rng.randint(0, 255, size=(1000, 4))
    colors[:, 3] = 255
    colors = colors.astype(np.uint8)

    expected_image = vp.encode(vp.PointCloud(positions, colors), depth)
    expected_shape = vp.image_shape(depth)
    assert expected_image.shape[:2] == expected_shape
    expected_pos, expected_clr, expected_count, expected_depth = vp.decode(expected_image)
    assert expected_depth == depth

    actual_image = np.zeros_like(expected_image)
    vp.encode(vp.PointCloud(expected_pos, expected_clr), depth, out_image=actual_image)

    np.testing.assert_array_equal(actual_image, expected_image)

    actual_pos, actual_clr, actual_count, actual_depth = vp.decode(
        actual_image, out_positions=positions, out_colors=colors)

    assert actual_count == expected_count
    assert actual_depth == expected_depth
    np.testing.assert_array_almost_equal(actual_pos[:actual_count], expected_pos)
    np.testing.assert_array_equal(actual_clr[:actual_count], expected_clr)


def test_io(tmp_path):
    expected_positions = np.random.rand(1000, 4) * 2 - 1
    expected_positions[:, 3] = 1
    expected_colors = np.random.randint(0, 255, size=(1000, 4))
    expected_colors[:, 3] = 255

    expected_cloud = vp.PointCloud(expected_positions, expected_colors)
    expected_cloud.save(tmp_path / "cloud.dat")
    actual_cloud = vp.PointCloud.load(tmp_path / "cloud.dat")

    np.testing.assert_array_almost_equal(actual_cloud.positions, expected_cloud.positions)
    np.testing.assert_array_equal(actual_cloud.colors, expected_cloud.colors)


def test_voxel_size():
    for depth in range(1, 11):
        size = vp.voxel_size(depth)
        expected = 2.0 / (2 ** depth)
        assert abs(size - expected) < 1e-6, f"depth={depth}: {size} != {expected}"

    # voxel size should decrease as depth increases
    for depth in range(2, 11):
        assert vp.voxel_size(depth) < vp.voxel_size(depth - 1)


@pytest.mark.parametrize("depth", range(7, 11))
def test_image_shape(depth: int):
    h, w = vp.image_shape(depth)
    assert h > 0
    assert w > 0
    # dimensions should increase with depth
    if depth > 7:
        h_prev, w_prev = vp.image_shape(depth - 1)
        assert h > h_prev
        assert w > w_prev


def test_to3_to4_roundtrip():
    positions = np.random.rand(100, 4).astype(np.float32) * 2 - 1
    positions[:, 3] = 1
    colors = np.random.randint(0, 255, size=(100, 4)).astype(np.uint8)
    colors[:, 3] = 255
    cloud4 = vp.PointCloud(positions, colors)

    # to3 drops the 4th column
    cloud3 = cloud4.to3()
    assert cloud3.positions.shape == (100, 3)
    assert cloud3.colors.shape == (100, 3)
    np.testing.assert_array_almost_equal(cloud3.positions, positions[:, :3])
    np.testing.assert_array_equal(cloud3.colors, colors[:, :3])

    # to4 restores homogeneous coordinate and alpha
    cloud4_rt = cloud3.to4()
    assert cloud4_rt.positions.shape == (100, 4)
    assert cloud4_rt.colors.shape == (100, 4)
    np.testing.assert_array_almost_equal(cloud4_rt.positions[:, :3], positions[:, :3])
    assert (cloud4_rt.positions[:, 3] == 1).all()
    np.testing.assert_array_equal(cloud4_rt.colors[:, :3], colors[:, :3])
    assert (cloud4_rt.colors[:, 3] == 255).all()

    # no-op when already in target format
    assert cloud4.to4() is cloud4
    assert cloud3.to3() is cloud3


def test_encode_3d_cloud():
    positions3 = np.random.rand(500, 3).astype(np.float32) * 2 - 1
    colors3 = np.random.randint(0, 255, size=(500, 3)).astype(np.uint8)
    cloud3 = vp.PointCloud(positions3, colors3)

    image = vp.encode(cloud3, depth=7)
    h, w = vp.image_shape(7)
    assert image.shape == (h, w, 4)

    result = vp.decode(image)
    assert result.depth == 7
    assert result.count > 0


def test_decode_result_cloud():
    positions = np.random.rand(500, 4).astype(np.float32) * 2 - 1
    positions[:, 3] = 1
    colors = np.random.randint(0, 255, size=(500, 4)).astype(np.uint8)
    colors[:, 3] = 255

    image = vp.encode(vp.PointCloud(positions, colors), depth=7)
    result = vp.decode(image)

    cloud = result.cloud
    assert isinstance(cloud, vp.PointCloud)
    np.testing.assert_array_equal(cloud.positions, result.positions)
    np.testing.assert_array_equal(cloud.colors, result.colors)


def test_decode_truncate():
    positions = np.random.rand(500, 4).astype(np.float32) * 2 - 1
    positions[:, 3] = 1
    colors = np.random.randint(0, 255, size=(500, 4)).astype(np.uint8)
    colors[:, 3] = 255

    image = vp.encode(vp.PointCloud(positions, colors), depth=7)

    # decode once to learn the actual count
    full_result = vp.decode(image)
    assert full_result.count > 1

    # preallocate undersized buffers and decode with truncate=True
    small_n = full_result.count // 2
    out_pos = np.empty((small_n, 4), dtype=np.float32)
    out_clr = np.empty((small_n, 4), dtype=np.uint8)

    result = vp.decode(image, truncate=True,
                       out_positions=out_pos, out_colors=out_clr)
    assert result.count == full_result.count
    assert result.depth == full_result.depth


def test_encode_deterministic():
    positions = np.random.rand(500, 4).astype(np.float32) * 2 - 1
    positions[:, 3] = 1
    colors = np.random.randint(0, 255, size=(500, 4)).astype(np.uint8)
    colors[:, 3] = 255
    cloud = vp.PointCloud(positions, colors)

    image1 = vp.encode(cloud, depth=8)
    image2 = vp.encode(cloud, depth=8)
    np.testing.assert_array_equal(image1, image2)


def test_single_point():
    positions = np.array([[0.0, 0.0, 0.0, 1.0]], dtype=np.float32)
    colors = np.array([[128, 64, 32, 255]], dtype=np.uint8)
    cloud = vp.PointCloud(positions, colors)

    image = vp.encode(cloud, depth=7)
    result = vp.decode(image)
    assert result.count == 1
    assert result.depth == 7


def test_encode_invalid_positions_ndim():
    positions = np.random.rand(100, 5).astype(np.float32)
    colors = np.random.randint(0, 255, size=(100, 4)).astype(np.uint8)

    with pytest.raises(ValueError):
        vp.encode(vp.PointCloud(positions, colors), depth=7)


def test_encode_invalid_colors_shape():
    positions = np.random.rand(100, 4).astype(np.float32)
    colors = np.random.randint(0, 255, size=(100, 3)).astype(np.uint8)

    with pytest.raises(ValueError):
        vp.encode(vp.PointCloud(positions, colors), depth=7)


def test_decode_invalid_image_ndim():
    image = np.zeros((100, 100), dtype=np.uint8)

    with pytest.raises(ValueError):
        vp.decode(image)


def test_decode_preallocated_too_small():
    positions = np.random.rand(500, 4).astype(np.float32) * 2 - 1
    positions[:, 3] = 1
    colors = np.random.randint(0, 255, size=(500, 4)).astype(np.uint8)
    colors[:, 3] = 255

    image = vp.encode(vp.PointCloud(positions, colors), depth=7)

    out_pos = np.empty((1, 4), dtype=np.float32)
    out_clr = np.empty((1, 4), dtype=np.uint8)

    with pytest.raises(ValueError):
        vp.decode(image, truncate=False,
                  out_positions=out_pos, out_colors=out_clr)


def test_io_3d_cloud(tmp_path):
    positions = np.random.rand(200, 4).astype(np.float32) * 2 - 1
    positions[:, 3] = 1
    colors = np.random.randint(0, 255, size=(200, 4)).astype(np.uint8)
    colors[:, 3] = 255
    cloud = vp.PointCloud(positions, colors).to3()

    cloud.save(str(tmp_path / "cloud3.dat"))
    loaded = vp.PointCloud.load(str(tmp_path / "cloud3.dat"))

    # load always returns Nx4
    assert loaded.positions.shape == (200, 4)
    np.testing.assert_array_almost_equal(loaded.positions[:, :3], cloud.positions)
    np.testing.assert_array_equal(loaded.colors[:, :3], cloud.colors)


if __name__ == "__main__":
    test_transcode(9)
