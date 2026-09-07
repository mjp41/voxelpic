// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <voxelpic/voxelpic.h>

#define FAIL(...)                                                              \
  do {                                                                         \
    printf(__VA_ARGS__);                                                       \
    printf("\n");                                                              \
    result = 1;                                                                \
    goto end;                                                                  \
  } while (0)

static int test_vec4_compare(void) {
  int result = 0;

  voxelpicVec4 a = {{1.0f, 2.0f, 3.0f, 4.0f}};
  voxelpicVec4 b = {{1.0f, 2.0f, 3.0f, 4.0f}};
  voxelpicVec4 c = {{1.0f, 2.0f, 3.0f, 5.0f}};
  voxelpicVec4 d = {{0.0f, 2.0f, 3.0f, 4.0f}};

  if (voxelpicVec4Compare(&a, &b) != 0) {
    FAIL("Vec4Compare: equal vectors should return 0");
  }

  if (voxelpicVec4Compare(&a, &c) >= 0) {
    FAIL("Vec4Compare: a < c should return negative");
  }

  if (voxelpicVec4Compare(&c, &a) <= 0) {
    FAIL("Vec4Compare: c > a should return positive");
  }

  if (voxelpicVec4Compare(&d, &a) >= 0) {
    FAIL("Vec4Compare: d < a should return negative");
  }

  // Vec3Compare ignores the 4th component
  voxelpicVec4 e = {{1.0f, 2.0f, 3.0f, 99.0f}};
  if (voxelpicVec3Compare(&a, &e) != 0) {
    FAIL("Vec3Compare: should ignore 4th component");
  }

  if (voxelpicVec3Compare(&d, &a) >= 0) {
    FAIL("Vec3Compare: d < a in first 3 components");
  }

end:
  return result;
}

static int test_pointcloud_new_capacity(void) {
  int result = 0;
  voxelpicPointCloud *cloud = voxelpicPointCloudNew(3);
  if (cloud == NULL) {
    FAIL("PointCloudNew returned NULL");
  }
  if (cloud->capacity != 3 || cloud->size != 0) {
    FAIL("PointCloudNew: expected capacity 3 and size 0, got %zu and %zu",
         cloud->capacity, cloud->size);
  }
  if (cloud->positions == NULL || cloud->colors == NULL) {
    FAIL("PointCloudNew: nonzero capacity requires allocated buffers");
  }

end:
  if (cloud != NULL) {
    voxelpicPointCloudFree(cloud);
  }
  return result;
}

static int test_octree_queries(void) {
  int result = 0;
  voxelpicOcTree *octree = voxelpicOcTreeNew(3, 9);
  voxelpicPointCloud *cloud = NULL;
  if (octree == NULL) {
    FAIL("OcTreeNew returned NULL");
  }

  size_t min_depth = voxelpicOcTreeMinDepth(octree);
  size_t max_depth = voxelpicOcTreeMaxDepth(octree);

  if (min_depth != 3) {
    FAIL("MinDepth: expected 3, got %zu", min_depth);
  }

  // MaxDepth returns min_depth + num_levels (exclusive upper bound)
  if (max_depth != 10) {
    FAIL("MaxDepth: expected 10, got %zu", max_depth);
  }

  // Before building, levels exist but are empty
  voxelpicLevel *level = NULL;
  voxelpicEnum rc = voxelpicOcTreeLevel(octree, 5, &level);
  if (rc != VPIC_OK) {
    FAIL("OcTreeLevel(5) before build: expected VPIC_OK, got %d", rc);
  }
  if (voxelpicLevelSize(level) != 0) {
    FAIL("Level size before build: expected 0, got %zu",
         voxelpicLevelSize(level));
  }

  // Build with a small cloud to initialize levels
  cloud = voxelpicPointCloudNew(10);
  if (cloud == NULL) {
    FAIL("PointCloudNew returned NULL");
  }
  for (size_t i = 0; i < 10; ++i) {
    float v = (float)i / 10.0f * 2.0f - 1.0f;
    cloud->positions[i] = (voxelpicVec4){{v, v, v, 1.0f}};
    cloud->colors[i] = (voxelpicColor){{128, 64, 32, 255}};
  }
  cloud->size = 10;

  rc = voxelpicOcTreeBuild(octree, cloud);
  if (rc != VPIC_OK) {
    FAIL("OcTreeBuild failed: %s", voxelpicError(rc));
  }

  // Valid level access
  rc = voxelpicOcTreeLevel(octree, 5, &level);
  if (rc != VPIC_OK) {
    FAIL("OcTreeLevel(5) failed: %s", voxelpicError(rc));
  }

  // Out of range: below min
  rc = voxelpicOcTreeLevel(octree, 1, &level);
  if (rc != VPIC_INVALID_LEVEL) {
    FAIL("OcTreeLevel(1): expected VPIC_INVALID_LEVEL, got %d", rc);
  }

  // Out of range: above max
  rc = voxelpicOcTreeLevel(octree, 10, &level);
  if (rc != VPIC_INVALID_LEVEL) {
    FAIL("OcTreeLevel(10): expected VPIC_INVALID_LEVEL, got %d", rc);
  }

end:
  if (cloud) {
    voxelpicPointCloudFree(cloud);
  }
  voxelpicOcTreeFree(octree);
  return result;
}

static int test_level_queries(void) {
  int result = 0;
  voxelpicOcTree *octree = voxelpicOcTreeNew(4, 8);
  voxelpicPointCloud *cloud = voxelpicPointCloudNew(100);
  if (octree == NULL || cloud == NULL) {
    FAIL("Allocation failed");
  }

  for (size_t i = 0; i < 100; ++i) {
    float v = (float)i / 100.0f * 2.0f - 1.0f;
    cloud->positions[i] = (voxelpicVec4){{v, v * 0.5f, v * 0.3f, 1.0f}};
    cloud->colors[i] =
        (voxelpicColor){{(uint_least8_t)(i % 256), 128, 64, 255}};
  }
  cloud->size = 100;

  voxelpicEnum rc = voxelpicOcTreeBuild(octree, cloud);
  if (rc != VPIC_OK) {
    FAIL("OcTreeBuild: %s", voxelpicError(rc));
  }

  voxelpicLevel *level = NULL;
  rc = voxelpicOcTreeLevel(octree, 7, &level);
  if (rc != VPIC_OK) {
    FAIL("OcTreeLevel(7): %s", voxelpicError(rc));
  }

  size_t depth = voxelpicLevelDepth(level);
  if (depth != 7) {
    FAIL("LevelDepth: expected 7, got %zu", depth);
  }

  size_t size = voxelpicLevelSize(level);
  if (size == 0) {
    FAIL("LevelSize: expected > 0 at depth 7");
  }

end:
  voxelpicPointCloudFree(cloud);
  voxelpicOcTreeFree(octree);
  return result;
}

static int test_depth_image_size(void) {
  int result = 0;

  // Valid depths
  for (size_t depth = 7; depth <= 10; ++depth) {
    size_t width = 0, height = 0;
    voxelpicEnum rc = voxelpicDepthImageSize(depth, &width, &height);
    if (rc != VPIC_OK) {
      FAIL("DepthImageSize(%zu): %s", depth, voxelpicError(rc));
    }
    if (width == 0 || height == 0) {
      FAIL("DepthImageSize(%zu): zero dimension w=%zu h=%zu", depth, width,
           height);
    }

    // dimensions should grow with depth
    if (depth > 7) {
      size_t pw = 0, ph = 0;
      voxelpicDepthImageSize(depth - 1, &pw, &ph);
      if (width <= pw || height <= ph) {
        FAIL("DepthImageSize(%zu): not larger than depth %zu", depth,
             depth - 1);
      }
    }
  }

  // Invalid depth (too low)
  size_t w, h;
  voxelpicEnum rc = voxelpicDepthImageSize(6, &w, &h);
  if (rc != VPIC_INVALID_LEVEL) {
    FAIL("DepthImageSize(6): expected VPIC_INVALID_LEVEL, got %d", rc);
  }

  rc = voxelpicDepthImageSize(0, &w, &h);
  if (rc != VPIC_INVALID_LEVEL) {
    FAIL("DepthImageSize(0): expected VPIC_INVALID_LEVEL, got %d", rc);
  }

end:
  return result;
}

static int test_depth_voxel_size(void) {
  int result = 0;

  for (size_t depth = 1; depth <= 10; ++depth) {
    float size = voxelpicDepthVoxelSize(depth);
    float expected = 2.0f / (float)((size_t)1 << depth);
    if (fabsf(size - expected) > 1e-7f) {
      FAIL("VoxelSize(%zu): expected %f, got %f", depth, expected, size);
    }

    // should decrease with depth
    if (depth > 1) {
      float prev = voxelpicDepthVoxelSize(depth - 1);
      if (size >= prev) {
        FAIL("VoxelSize(%zu): not smaller than depth %zu", depth, depth - 1);
      }
    }
  }

end:
  return result;
}

static int test_error_strings(void) {
  int result = 0;

  struct {
    voxelpicEnum code;
    const char *expected;
  } cases[] = {
      {VPIC_OK, "OK"},
      {VPIC_ERROR, "Unspecified error"},
      {VPIC_INVALID_LEVEL, "Level does not exist"},
      {VPIC_BAD_POINTER, "Bad pointer"},
      {VPIC_TOO_SMALL, "Point cloud was too small"},
      {VPIC_OUT_OF_MEMORY, "Out of memory"},
      {VPIC_OUT_OF_RANGE, "Out of range"},
      {VPIC_IO_ERROR, "I/O error"},
      {VPIC_UNINITIALIZED, "Not initialized"},
  };
  size_t num_cases = sizeof(cases) / sizeof(cases[0]);

  for (size_t i = 0; i < num_cases; ++i) {
    const char *msg = voxelpicError(cases[i].code);
    if (msg == NULL) {
      FAIL("Error(%d): returned NULL", cases[i].code);
    }
    if (strcmp(msg, cases[i].expected) != 0) {
      FAIL("Error(%d): expected \"%s\", got \"%s\"", cases[i].code,
           cases[i].expected, msg);
    }
  }

  // Unknown code
  const char *unknown = voxelpicError(99999);
  if (unknown == NULL || strcmp(unknown, "Invalid error code") != 0) {
    FAIL("Error(99999): expected \"Invalid error code\", got \"%s\"",
         unknown ? unknown : "(null)");
  }

end:
  return result;
}

static int test_level_to_cloud_truncate(void) {
  int result = 0;
  voxelpicOcTree *octree = voxelpicOcTreeNew(4, 9);
  voxelpicPointCloud *input = voxelpicPointCloudNew(200);
  voxelpicPointCloud *output = NULL;
  if (octree == NULL || input == NULL) {
    FAIL("Allocation failed");
  }

  for (size_t i = 0; i < 200; ++i) {
    float v = (float)i / 200.0f * 2.0f - 1.0f;
    input->positions[i] = (voxelpicVec4){{v, v * 0.7f, v * 0.3f, 1.0f}};
    input->colors[i] = (voxelpicColor){{128, 64, 32, 255}};
  }
  input->size = 200;

  voxelpicEnum rc = voxelpicOcTreeBuild(octree, input);
  if (rc != VPIC_OK) {
    FAIL("OcTreeBuild: %s", voxelpicError(rc));
  }

  voxelpicLevel *level = NULL;
  rc = voxelpicOcTreeLevel(octree, 7, &level);
  if (rc != VPIC_OK) {
    FAIL("OcTreeLevel(7): %s", voxelpicError(rc));
  }

  size_t level_size = voxelpicLevelSize(level);
  if (level_size == 0) {
    FAIL("Level has zero voxels");
  }

  // Undersized cloud without truncate should fail
  size_t small_cap = level_size / 2;
  if (small_cap == 0) {
    small_cap = 1;
  }
  output = voxelpicPointCloudNew(small_cap);
  if (output == NULL) {
    FAIL("PointCloudNew failed");
  }

  rc = voxelpicLevelToCloud(level, output, false);
  if (rc != VPIC_TOO_SMALL) {
    FAIL("LevelToCloud(truncate=false): expected VPIC_TOO_SMALL, got %d", rc);
  }

  // With truncate=true, should succeed
  rc = voxelpicLevelToCloud(level, output, true);
  if (rc != VPIC_OK) {
    FAIL("LevelToCloud(truncate=true): %s", voxelpicError(rc));
  }

end:
  if (output) {
    voxelpicPointCloudFree(output);
  }
  voxelpicPointCloudFree(input);
  voxelpicOcTreeFree(octree);
  return result;
}

static int write_test_file(const char *path, const unsigned char *data,
                           size_t size) {
  FILE *file = fopen(path, "wb");
  if (file == NULL) {
    return 1;
  }

  int result = size > 0 && fwrite(data, 1, size, file) != size;
  return fclose(file) != 0 || result;
}

static int test_level_load_rejects_malformed_files(void) {
  int result = 0;
  const char *path = "invalid_level_header.dat";
  voxelpicLevel *level = voxelpicLevelNew(0);
  if (level == NULL) {
    FAIL("LevelNew failed");
  }

  const unsigned char partial_depth[] = {0, 0, 0};
  const unsigned char missing_count[] = {0, 0, 0, 7};
  const unsigned char partial_count[] = {0, 0, 0, 7, 0, 0, 0};
  const unsigned char minimum_invalid_size[] = {0, 0, 0, 7, 0x80, 0, 0, 0};
  const unsigned char negative_size[] = {0, 0, 0, 7, 0xff, 0xff, 0xff, 0xff};
  const unsigned char missing_position[] = {0, 0, 0, 7, 0, 0, 0, 1};
  const unsigned char partial_position[] = {0, 0, 0, 7, 0, 0, 0,
                                            1, 0, 0, 0, 0, 0};
  const unsigned char partial_color[] = {0, 0, 0, 7, 0, 0, 0, 1,
                                         0, 0, 0, 0, 0, 0, 1, 2};
  const unsigned char valid_min_depth[] = {0, 0, 0, 0, 0, 0, 0, 0};
  const unsigned char valid_max_depth[] = {0, 0, 0, 14, 0, 0, 0, 0};
  const unsigned char invalid_depth[] = {0, 0, 0, 15};
  const unsigned char oversized_depth[] = {0, 0, 0, 32};
  struct {
    const char *name;
    const unsigned char *data;
    size_t size;
    voxelpicEnum expected;
  } cases[] = {
      {"empty", NULL, 0, VPIC_IO_ERROR},
      {"partial depth", partial_depth, sizeof(partial_depth), VPIC_IO_ERROR},
      {"missing count", missing_count, sizeof(missing_count), VPIC_IO_ERROR},
      {"partial count", partial_count, sizeof(partial_count), VPIC_IO_ERROR},
      {"minimum invalid size", minimum_invalid_size,
       sizeof(minimum_invalid_size), VPIC_IO_ERROR},
      {"negative size", negative_size, sizeof(negative_size), VPIC_IO_ERROR},
      {"missing position", missing_position, sizeof(missing_position),
       VPIC_IO_ERROR},
      {"partial position", partial_position, sizeof(partial_position),
       VPIC_IO_ERROR},
      {"partial color", partial_color, sizeof(partial_color), VPIC_IO_ERROR},
      {"minimum depth", valid_min_depth, sizeof(valid_min_depth), VPIC_OK},
      {"maximum depth", valid_max_depth, sizeof(valid_max_depth), VPIC_OK},
      {"invalid depth", invalid_depth, sizeof(invalid_depth),
       VPIC_INVALID_LEVEL},
      {"oversized depth", oversized_depth, sizeof(oversized_depth),
       VPIC_INVALID_LEVEL},
  };

  size_t num_cases = sizeof(cases) / sizeof(cases[0]);
  for (size_t i = 0; i < num_cases; ++i) {
    if (write_test_file(path, cases[i].data, cases[i].size)) {
      FAIL("Failed to write %s level file", cases[i].name);
    }

    voxelpicEnum rc = voxelpicLevelLoad(path, level);
    if (rc != cases[i].expected) {
      FAIL("LevelLoad %s: expected %d, got %d", cases[i].name,
           cases[i].expected, rc);
    }
  }

end:
  remove(path);
  if (level != NULL) {
    voxelpicLevelFree(level);
  }
  return result;
}

static int test_cloud_load_rejects_malformed_files(void) {
  int result = 0;
  const char *path = "invalid_cloud_header.dat";
  const unsigned char partial_count[] = {0, 0, 0};
  const unsigned char empty_cloud[] = {0, 0, 0, 0};
  const unsigned char minimum_invalid_size[] = {0x80, 0, 0, 0};
  const unsigned char negative_size[] = {0xff, 0xff, 0xff, 0xff};
  const unsigned char missing_position[] = {0, 0, 0, 1};
  const unsigned char partial_position[] = {0, 0, 0, 1, 0, 0, 0, 0,
                                            0, 0, 0, 0, 0, 0, 0};
  const unsigned char partial_color[] = {0, 0, 0, 1, 0, 0, 0, 0, 0,
                                         0, 0, 0, 0, 0, 0, 1, 2};
  struct {
    const char *name;
    const unsigned char *data;
    size_t size;
    voxelpicEnum expected;
  } cases[] = {
      {"empty", NULL, 0, VPIC_IO_ERROR},
      {"partial count", partial_count, sizeof(partial_count), VPIC_IO_ERROR},
      {"empty cloud", empty_cloud, sizeof(empty_cloud), VPIC_OK},
      {"minimum invalid size", minimum_invalid_size,
       sizeof(minimum_invalid_size), VPIC_IO_ERROR},
      {"negative size", negative_size, sizeof(negative_size), VPIC_IO_ERROR},
      {"missing position", missing_position, sizeof(missing_position),
       VPIC_IO_ERROR},
      {"partial position", partial_position, sizeof(partial_position),
       VPIC_IO_ERROR},
      {"partial color", partial_color, sizeof(partial_color), VPIC_IO_ERROR},
  };

  voxelpicPointCloud *cloud = voxelpicPointCloudNew(0);
  if (cloud == NULL) {
    FAIL("PointCloudNew failed");
  }

  size_t num_cases = sizeof(cases) / sizeof(cases[0]);
  for (size_t i = 0; i < num_cases; ++i) {
    if (write_test_file(path, cases[i].data, cases[i].size)) {
      voxelpicPointCloudFree(cloud);
      FAIL("Failed to write %s cloud file", cases[i].name);
    }

    voxelpicEnum rc = voxelpicPointCloudLoad(path, cloud);
    if (rc != cases[i].expected) {
      voxelpicPointCloudFree(cloud);
      FAIL("PointCloudLoad %s: expected %d, got %d", cases[i].name,
           cases[i].expected, rc);
    }
  }

  voxelpicPointCloudFree(cloud);

end:
  remove(path);
  return result;
}

static int test_load_rejects_invalid_arguments(void) {
  int result = 0;
  const char *path = "missing_loader_input.dat";
  voxelpicLevel *level = NULL;
  voxelpicPointCloud *cloud = NULL;

  remove(path);

  voxelpicEnum rc = voxelpicLevelLoad(path, NULL);
  if (rc != VPIC_BAD_POINTER) {
    FAIL("LevelLoad NULL: expected VPIC_BAD_POINTER, got %d", rc);
  }

  rc = voxelpicPointCloudLoad(path, NULL);
  if (rc != VPIC_BAD_POINTER) {
    FAIL("PointCloudLoad NULL: expected VPIC_BAD_POINTER, got %d", rc);
  }

  level = voxelpicLevelNew(0);
  cloud = voxelpicPointCloudNew(0);
  if (level == NULL || cloud == NULL) {
    FAIL("Loader argument allocation failed");
  }

  rc = voxelpicLevelLoad(path, level);
  if (rc != VPIC_IO_ERROR) {
    FAIL("LevelLoad missing file: expected VPIC_IO_ERROR, got %d", rc);
  }

  rc = voxelpicPointCloudLoad(path, cloud);
  if (rc != VPIC_IO_ERROR) {
    FAIL("PointCloudLoad missing file: expected VPIC_IO_ERROR, got %d", rc);
  }

end:
  if (level != NULL) {
    voxelpicLevelFree(level);
  }
  if (cloud != NULL) {
    voxelpicPointCloudFree(cloud);
  }
  return result;
}

int main(void) {
  struct {
    const char *name;
    int (*func)(void);
  } tests[] = {
      {"vec4_compare", test_vec4_compare},
      {"pointcloud_new_capacity", test_pointcloud_new_capacity},
      {"octree_queries", test_octree_queries},
      {"level_queries", test_level_queries},
      {"depth_image_size", test_depth_image_size},
      {"depth_voxel_size", test_depth_voxel_size},
      {"error_strings", test_error_strings},
      {"level_to_cloud_truncate", test_level_to_cloud_truncate},
      {"level_load_rejects_malformed_files",
       test_level_load_rejects_malformed_files},
      {"cloud_load_rejects_malformed_files",
       test_cloud_load_rejects_malformed_files},
      {"load_rejects_invalid_arguments", test_load_rejects_invalid_arguments},
  };
  size_t num_tests = sizeof(tests) / sizeof(tests[0]);
  int failures = 0;

  for (size_t i = 0; i < num_tests; ++i) {
    int rc = tests[i].func();
    if (rc) {
      printf("FAIL: %s\n", tests[i].name);
      failures++;
    } else {
      printf("PASS: %s\n", tests[i].name);
    }
  }

  if (failures) {
    printf("\n%d/%zu tests failed\n", failures, num_tests);
    return 1;
  }

  printf("\nAll %zu tests passed\n", num_tests);
  return 0;
}
