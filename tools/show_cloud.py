"""Show voxelpic point cloud data stored in a binary file using an HTML viewer."""

import argparse
import struct
from typing import NamedTuple

import numpy as np
import scenepic as sp


PointCloud = NamedTuple(
    "PointCloud", [("positions", np.ndarray), ("colors", np.ndarray)])


def load_data(path: str):
    with open(path, "rb") as file:
        struct_size = struct.calcsize(">i")
        binary_data = file.read(struct_size)
        num_points = struct.unpack(">i", binary_data)[0]
        positions = []
        colors = []
        struct_size = struct.calcsize("fffBBB")
        binary_data = file.read(struct_size * num_points)
        for p in range(num_points):
            x, y, z, r, g, b = struct.unpack_from(
                "fffBBB", binary_data, p * struct_size)
            positions.append([x, y, z])
            colors.append([r, g, b])

        cloud = PointCloud(np.array(positions, np.float32),
                           np.array(colors, np.float32) / 255)

        return cloud


def main():
    parser = argparse.ArgumentParser("Show Cloud data")
    parser.add_argument("cloud_path", type=str)
    parser.add_argument("output_path", type=str)
    args = parser.parse_args()

    cloud = load_data(args.cloud_path)

    scene = sp.Scene()
    points_mesh = scene.create_mesh("points", layer_id="points")
    points_mesh.add_sphere(sp.Colors.White, sp.Transforms.scale(0.01))
    points_mesh.enable_instancing(cloud.positions, colors=cloud.colors)

    bounds_mesh = scene.create_mesh("bounds", layer_id="bounds")
    bounds_mesh.add_cube(sp.Colors.White, sp.Transforms.scale(2),
                         fill_triangles=False, add_wireframe=True)

    canvas = scene.create_canvas_3d(width=800, height=800)
    canvas.create_frame(meshes=[points_mesh])

    scene.save_as_html(args.output_path, "Cloud Data")


if __name__ == "__main__":
    main()
