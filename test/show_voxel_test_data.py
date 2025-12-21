"""This script provides a way to visualize voxel test data stored in a binary file."""

import argparse
import struct
from typing import NamedTuple

import numpy as np
import scenepic as sp


PointCloud = NamedTuple(
    "PointCloud", [("positions", np.ndarray), ("colors", np.ndarray)])
TestData = NamedTuple(
    "TestData", [("level", int), ("input", PointCloud),
                 ("output", PointCloud)])


def load_data(path: str):
    with open(path, "rb") as file:
        struct_size = struct.calcsize(">ii")
        binary_data = file.read(struct_size)
        level, num_points = struct.unpack(">ii", binary_data)
        positions = []
        colors = []
        struct_size = struct.calcsize("fffBBB")
        binary_data = file.read(struct_size * num_points)
        for i in range(num_points):
            x, y, z, r, g, b = struct.unpack_from(
                "fffBBB", binary_data, i * struct_size)
            positions.append([x, y, z])
            colors.append([r, g, b])

        cloud_in = PointCloud(np.array(positions, np.float32),
                              np.array(colors, np.float32) / 255)

        struct_size = struct.calcsize(">i")
        binary_data = file.read(struct_size)
        num_points = struct.unpack(">i", binary_data)[0]
        struct_size = struct.calcsize("fffBBB")
        binary_data = file.read(struct_size * num_points)
        positions = []
        colors = []
        for i in range(num_points):
            x, y, z, r, g, b = struct.unpack_from(
                "fffBBB", binary_data, i * struct_size)
            positions.append([x, y, z])
            colors.append([r, g, b])

        cloud_out = PointCloud(np.array(positions, np.float32),
                               np.array(colors, np.float32) / 255)
        return TestData(level, cloud_in, cloud_out)


def main():
    parser = argparse.ArgumentParser("Show Voxel data")
    parser.add_argument("test_path", type=str)
    parser.add_argument("output_path", type=str)
    args = parser.parse_args()

    data = load_data(args.test_path)

    scene = sp.Scene()
    points_mesh = scene.create_mesh("points", layer_id="points")
    points_mesh.add_sphere(sp.Colors.White, sp.Transforms.scale(0.01))
    points_mesh.enable_instancing(
        data.input.positions, colors=data.input.colors)

    voxels_mesh = scene.create_mesh("voxels", layer_id="voxels")
    voxels_mesh.add_cube(sp.Colors.White, sp.Transforms.scale(
        2 ** (-data.level)), fill_triangles=False, add_wireframe=True)
    voxels_mesh.enable_instancing(
        data.output.positions, colors=data.output.colors)

    canvas = scene.create_canvas_3d(width=800, height=800)
    canvas.create_frame(meshes=[points_mesh, voxels_mesh])

    scene.save_as_html(args.output_path, "Voxel Data")


if __name__ == "__main__":
    main()
