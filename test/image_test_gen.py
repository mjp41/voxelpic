"""Generate random point cloud test data for image tests.

These scripts generate the (somewhat large) test files automatically as
part of the build process, if testing has been enabled.
"""

import argparse
import os
import random
import struct
import sys
from typing import NamedTuple, Set


Vec = NamedTuple("Vec", [("x", float), ("y", float), ("z", float)])


class Index(NamedTuple("Index", [("i", int), ("j", int), ("k", int)])):
    """Index of a voxel within an OcTree structure."""

    def __lt__(self, other: "Index"):
        if sys.byteorder == "little":
            return (self.k, self.j, self.i) < (other.k, other.j, other.i)

        return self < other

    def to_vec(self, side: int) -> Vec:
        x = (2 * self.i + 1) / side - 1
        y = (2 * self.j + 1) / side - 1
        z = (2 * self.k + 1) / side - 1
        assert -1 < x < 1
        assert -1 < y < 1
        assert -1 < z < 1
        return Vec(x, y, z)


class BoundingBox(NamedTuple("BoundingBox", [("i_min", int), ("k_min", int),
                                             ("i_max", int), ("k_max", int),
                                             ("side", int)])):
    """A bounding box within an OcTree structure."""

    def sample(self) -> Index:
        i = random.randint(self.i_min, self.i_max - 1)
        j = random.randint(0, self.side - 1)
        k = random.randint(self.k_min, self.k_max - 1)
        return Index(i, j, k)

    @staticmethod
    def random(side: int) -> "BoundingBox":
        i_range = random.randint(side // 2, side * 9 // 10)
        k_range = random.randint(side // 2, side * 9 // 10)
        i_min = random.randint(0, side - i_range)
        k_min = random.randint(0, side - k_range)
        return BoundingBox(i_min, k_min,
                           i_min + i_range, k_min + k_range, side)


class Color(NamedTuple("Color", [("red", int), ("green", int),
                                 ("blue", int)])):
    """RGB color."""
    @staticmethod
    def random():
        return Color(random.randint(0, 255), random.randint(0, 255),
                     random.randint(0, 255))


Voxel = NamedTuple(
    "Voxel", [("index", Index), ("center", Vec), ("color", Color)])


def generate_point_cloud(path: str, level: int, max_voxels: int):
    """Generate a random point cloud and save it to a binary file."""
    side = 2 ** level
    bb = BoundingBox.random(side)

    voxels: Set[Index] = set()
    x_count = {}
    z_count = {}
    x_full = (bb.i_max - bb.i_min) * bb.side
    z_full = (bb.k_max - bb.k_min) * bb.side

    max_voxels = x_full + z_full
    while len(x_count) + len(z_count) < max_voxels:
        index = bb.sample()
        if index in voxels:
            continue

        x_key = (index.i, index.j)
        x_count[x_key] = x_count.get(x_key, 0) + 1
        z_key = (index.k, index.j)
        z_count[z_key] = z_count.get(z_key, 0) + 1

        if x_count[x_key] > 2:
            continue

        if z_count[z_key] > 2:
            continue

        voxels.add(index)

    voxels = list(sorted(voxels))

    with open(path, "wb") as file:
        file.write(struct.pack(">ii", level, len(voxels)))
        for v in voxels:
            p = v.to_vec(side)
            c = Color.random()
            file.write(struct.pack("fffBBB", *p, *c))


if __name__ == "__main__":
    parser = argparse.ArgumentParser("Image test generator")
    parser.add_argument("output_dir", type=str)
    parser.add_argument("--min-depth", type=int, default=7)
    parser.add_argument("--max-depth", type=int, default=9)
    parser.add_argument("--seed", type=int, default=20080524)
    parser.add_argument("--max-voxels", type=int, default=500000)
    args = parser.parse_args()

    random.seed(args.seed)
    if not os.path.exists(args.output_dir):
        os.makedirs(args.output_dir)

    for level in range(args.min_depth, args.max_depth + 1):
        path = os.path.join(args.output_dir, f"image_{level}.dat")
        if os.path.exists(path):
            print(path, "exists, skipping")
            continue

        generate_point_cloud(path, level, args.max_voxels)
