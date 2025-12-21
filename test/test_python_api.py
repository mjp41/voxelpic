import numpy as np
import voxelpic as vp
import pytest


@pytest.mark.parametrize("depth", [7, 8, 9])
def test_transcode(depth: int):
    positions = np.random.rand(1000, 4) * 2 - 1
    positions[:, 3] = 1
    positions = positions.astype(np.float32)

    colors = np.random.randint(0, 255, size=(1000, 4))
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


if __name__ == "__main__":
    test_transcode(9)
