"""Show a voxelpic octree level stored in a binary file using an HTML viewer."""

import argparse
import struct
from typing import NamedTuple

import numpy as np
import scenepic as sp


PointCloud = NamedTuple(
    "PointCloud", [("positions", np.ndarray), ("colors", np.ndarray)])


def load_data(path: str):
    with open(path, "rb") as file:
        struct_size = struct.calcsize(">ii")
        binary_data = file.read(struct_size)
        level, num_points = struct.unpack(">ii", binary_data)
        inv_side = 2 ** (-level)
        positions = []
        colors = []
        struct_size = struct.calcsize("HHHBBB")
        binary_data = file.read(struct_size * num_points)
        for p in range(num_points):
            i, j, k, r, g, b = struct.unpack_from(
                "HHHBBB", binary_data, p * struct_size)
            x = (2 * i + 1) * inv_side - 1
            y = (2 * j + 1) * inv_side - 1
            z = (2 * k + 1) * inv_side - 1
            positions.append([x, y, z])
            colors.append([r, g, b])

        cloud = PointCloud(np.array(positions, np.float32),
                           np.array(colors, np.float32) / 255)

        return level, cloud


def main():
    parser = argparse.ArgumentParser("Show Voxel data")
    parser.add_argument("level_path", type=str)
    parser.add_argument("output_path", type=str)
    args = parser.parse_args()

    level, cloud = load_data(args.level_path)

    scene = sp.Scene()
    voxels_mesh = scene.create_mesh("voxels", layer_id="voxels")
    voxels_mesh.add_cube(sp.Colors.White, sp.Transforms.scale(
        2 ** (-level+1)))
    voxels_mesh.enable_instancing(
        cloud.positions, colors=cloud.colors)

    canvas = scene.create_canvas_3d(width=800, height=800)
    canvas.create_frame(meshes=[voxels_mesh])

    scene.save_as_html(args.output_path, "Voxel Data")


if __name__ == "__main__":
    main()
