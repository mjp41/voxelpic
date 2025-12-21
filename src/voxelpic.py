"""VoxelPic: Voxel-based point cloud compression and decompression."""

import os
import struct
from typing import NamedTuple, Tuple

import numpy as np
import _voxelpic

try:
    import scenepic as sp
except ImportError:
    print("Unable to import scenepic. Visualisation methods will not work.")


def image_shape(depth: int) -> Tuple[int, int]:
    """Get the shape of the image for a given depth.

    Args:
        depth: The depth of a level within an OcTree.

    Returns:
        A tuple (height, width) representing the shape of the image.
    """
    return _voxelpic.image_shape(depth)


def voxel_size(depth: int) -> float:
    """Get the size of a voxel for a given depth.

    Args:
        depth: The depth of a level within an OcTree.

    Returns:
        The length of one side of a voxel.
    """
    return _voxelpic.voxel_size(depth)


class PointCloud(NamedTuple("PointCloud", [("positions", np.ndarray), ("colors", np.ndarray)])):
    """A point cloud with positions and colors."""

    def to3(self) -> "PointCloud":
        """Convert to 3D positions and colors (drop alpha channel and homogenous coordinate if present)."""
        if self.positions.shape[1] == 3:
            return self

        positions = np.ascontiguousarray(self.positions[:, :3])
        colors = np.ascontiguousarray(self.colors[:, :3])
        return PointCloud(positions, colors)

    def to4(self) -> "PointCloud":
        """Convert to 4D positions and colors (add alpha channel and homogenous coordinate if absent)."""
        if self.positions.shape[1] == 4:
            return self

        size = len(self.positions)
        positions = np.empty((size, 4), np.float32)
        colors = np.empty((size, 4), np.uint8)
        positions[:, 3] = self.positions
        positions[3] = 1
        colors[:, :3] = self.colors
        colors[:, 3] = 255
        return PointCloud(positions, colors)

    @staticmethod
    def load(path: str) -> "PointCloud":
        """Load a point cloud from a binary file.

        Args:
            path: The path to the binary file.

        Returns:
            The loaded PointCloud.
        """
        with open(path, "rb") as file:
            struct_size = struct.calcsize(">i")
            binary_data = file.read(struct_size)
            num_points = struct.unpack(">i", binary_data)[0]
            positions = np.empty((num_points, 4), np.float32)
            colors = np.empty((num_points, 4), np.uint8)
            struct_size = struct.calcsize("fffBBB")
            binary_data = file.read(struct_size * num_points)
            for p in range(num_points):
                x, y, z, r, g, b = struct.unpack_from(
                    "fffBBB", binary_data, p * struct_size)
                positions[p, :3] = x, y, z
                positions[p, 3] = 1
                colors[p, :3] = r, g, b
                colors[p, 3] = 255

            return PointCloud(positions, colors)

    def save(self, path: str):
        """Save the point cloud to a binary file.

        Args:
            path: The path to the binary file.
        """
        dirname = os.path.dirname(path)
        if not os.path.exists(dirname):
            os.makedirs(dirname)

        with open(path, "wb") as file:
            file.write(struct.pack(">i", len(self.positions)))
            for p, c in zip(self.positions, self.colors):
                x, y, z = p[:3]
                r, g, b = c[:3]
                file.write(struct.pack("fffBBB", x, y, z, r, g, b))


class DecodeResult(NamedTuple("DecodeResult", [("positions", np.ndarray), ("colors", np.ndarray),
                                               ("count", int), ("depth", int)])):
    """The result of decoding a voxelpic image."""

    @property
    def cloud(self) -> PointCloud:
        """Get the decoded point cloud.

        Description:
            The point cloud represents the centroids and colors of all
            occupied voxels in the decoded level.

        Returns:
            The decoded PointCloud.
        """
        return PointCloud(self.positions, self.colors)

    def to_scenepic_mesh(self, scene, layer_id="voxels"):
        """Create a scenepic mesh representing the decoded voxels.

        Args:
            scene: The scenepic Scene to create the mesh in.
            layer_id: The layer ID for the mesh (default "voxels").

        Returns:
            The created scenepic Mesh.
        """
        mesh = scene.create_mesh(layer_id=layer_id)
        size = voxel_size(self.depth)
        mesh.add_cube(sp.Colors.White, transform=sp.Transforms.scale(size))
        cloud = self.cloud.to3()
        mesh.enable_instancing(cloud.positions, colors=cloud.colors.astype(np.float32) / 255)
        return mesh


def encode(cloud: PointCloud, depth: int = 9, out_image: np.ndarray = None) -> np.ndarray:
    """Encode a point cloud into a voxelpic image.

    Args:
        cloud: The input PointCloud to encode.
        depth: The depth of the OcTree level to encode (default 9).
        out_image: An optional preallocated output image array.

    Returns:
        The encoded voxelpic image as a numpy ndarray. If out_image is provided,
        it will be used to store the result. Otherwise a new array will be created.
    """
    cloud = cloud.to4()
    return _voxelpic.encode(cloud.positions, cloud.colors, depth, out_image)


def decode(image: np.ndarray, truncate: bool = False, out_positions: np.ndarray = None,
           out_colors: np.ndarray = None) -> DecodeResult:
    """Decode a voxelpic image into a point cloud.

    Args:
        image: The input voxelpic image as a numpy ndarray.
        truncate: Whether to truncate the number of output points to the capacity of the preallocated
                  arrays (default False).
        out_positions: An optional preallocated output array for positions.
        out_colors: An optional preallocated output array for colors.

    Returns:
        A DecodeResult named tuple containing positions, colors, count, and depth.
    """
    return DecodeResult(*_voxelpic.decode(image, truncate, out_positions, out_colors))
