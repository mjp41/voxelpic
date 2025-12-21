"""Generate a random point cloud and save it to a binary file.

The point cloud will have a spiral structure and look somewhat like a galaxy.
"""

import argparse
import math
import random
import struct


def linspace(min_value, max_value, num_points):
    """Generate a list of evenly spaced values."""
    scale = (max_value - min_value) / (num_points - 1)
    return [min_value + (scale * x) for x in range(0, num_points)]


def generate_point_cloud(path: str, num_points, arms=3, noise=0.02, radius=.9):
    """Generate a random point cloud and save it to a binary file."""
    theta = linspace(0, 4 * math.pi, num_points)
    radii = linspace(0, radius, num_points)

    theta = [t + random.randint(0, arms) * (2 * math.pi / arms)
             for t in theta]

    with open(path, "wb") as file:
        file.write(struct.pack(">i", len(theta)))
        for t, r in zip(theta, radii):
            x = r * math.cos(t) + random.normalvariate(0, noise)
            y = r * math.sin(t) + random.normalvariate(0, noise)
            z = random.normalvariate(0, noise * 5)

            norm_r = r / radius
            red = int(255 * (1.0 - norm_r))
            green = int(255 * (1.0 - norm_r / 2))
            blue = 255
            file.write(struct.pack("fffBBB", x, y, z, red, green, blue))


if __name__ == "__main__":
    parser = argparse.ArgumentParser("Cloud generator")
    parser.add_argument("output_path", type=str)
    parser.add_argument("--seed", type=int, default=20080524)
    parser.add_argument("--size", type=int, default=20000)
    args = parser.parse_args()

    random.seed(args.seed)
    generate_point_cloud(args.output_path,  args.size)
