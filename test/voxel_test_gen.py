"""Generate random point cloud test data for voxel tests.

These scripts generate the (somewhat large) test files automatically as
part of the build process, if testing has been enabled.
"""

import argparse
import os
import random
import struct
import sys
from typing import List, NamedTuple


class Index(NamedTuple("Index", [("i", int), ("j", int), ("k", int)])):
    """Index of a voxel within an OcTree structure."""

    def __lt__(self, other: "Index"):
        if sys.byteorder == "little":
            return (self.k, self.j, self.i) < (other.k, other.j, other.i)

        return self < other


class Color(NamedTuple("Color", [("red", int), ("green", int),
                                 ("blue", int)])):
    """RGB color."""
    @staticmethod
    def random():
        return Color(random.randint(0, 255), random.randint(0, 255),
                     random.randint(0, 255))


Vec = NamedTuple("Vec", [("x", float), ("y", float), ("z", float)])
Bounds = NamedTuple("Bounds", [("min", Vec), ("max", Vec)])
Voxel = NamedTuple(
    "Voxel", [("index", Index), ("center", Vec), ("color", Color)])


def generate_point_cloud(path: str, level: int, bounds: Bounds,
                         points_per_voxel=10):
    """Generate a random point cloud and save it to a binary file."""
    side = 2 ** level
    voxels: List[Voxel] = []
    for i in range(side):
        x = (2 * i + 1) / side - 1
        for j in range(side):
            y = (2 * j + 1) / side - 1
            for k in range(side):
                z = (2 * k + 1) / side - 1
                voxels.append(
                    Voxel(Index(i, j, k), Vec(x, y, z), Color.random()))

    random.shuffle(voxels)
    voxels = voxels[:len(voxels) // 2]

    voxels.sort()

    scale = 2 ** (-level - 2)

    positions = []
    colors = []
    for v in voxels:
        for i in range(points_per_voxel):
            dx = random.uniform(-scale, scale)
            dy = random.uniform(-scale, scale)
            dz = random.uniform(-scale, scale)
            positions.append(
                Vec(v.center[0] + dx, v.center[1] + dy, v.center[2] + dz))
            colors.append(v.color)

    num_positions = len(positions)
    index = list(range(num_positions))
    random.shuffle(index)

    voxels.sort(key=lambda v: v.index)

    with open(path, "wb") as file:
        file.write(struct.pack(">ii", level, num_positions))
        for i in index:
            p = positions[i]
            c = colors[i]
            file.write(struct.pack("fffBBB", *p, *c))

        file.write(struct.pack(">i", len(voxels)))
        for v in voxels:
            p = v.center
            c = v.color
            file.write(struct.pack("fffBBB", *p, *c))


if __name__ == "__main__":
    parser = argparse.ArgumentParser("Voxel test generator")
    parser.add_argument("output_dir", type=str)
    parser.add_argument("--max_depth", type=int, default=5)
    parser.add_argument("--seed", type=int, default=20080524)
    args = parser.parse_args()

    random.seed(args.seed)
    if not os.path.exists(args.output_dir):
        os.makedirs(args.output_dir)

    for level in range(1, args.max_depth + 1):
        path = os.path.join(args.output_dir, f"voxel_{level}.dat")
        if os.path.exists(path):
            print(path, "exists, skipping")
            continue

        generate_point_cloud(path, level, Bounds(Vec(-1, -1, -1),
                                                 Vec(1, 1, 1)))
