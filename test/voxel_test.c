#include "stdio.h"
#include "stdlib.h"

#include "voxelpic/voxelpic.h"

int compare_pointclouds(voxelpicPointCloud *actual,
                        voxelpicPointCloud *expected) {
  if (actual->size != expected->size) {
    printf("Size mismatch: %zu points != %zu points", actual->size,
           expected->size);
    return 1;
  }

  voxelpicVec4 *apos_ptr = actual->positions;
  voxelpicColor *aclr_ptr = actual->colors;
  voxelpicVec4 *epos_ptr = expected->positions;
  voxelpicColor *eclr_ptr = expected->colors;
  for (size_t i = 0; i < actual->size;
       ++i, ++apos_ptr, ++aclr_ptr, ++epos_ptr, ++eclr_ptr) {
    if (voxelpicVec3Compare(apos_ptr, epos_ptr)) {
      printf("%zu: (%f, %f, %f) != (%f, %f, %f)\n", i, apos_ptr->x, apos_ptr->y,
             apos_ptr->z, epos_ptr->x, epos_ptr->y, epos_ptr->z);
      return 1;
    }

    if (aclr_ptr->value != eclr_ptr->value) {
      printf("%zu: (%u, %u, %u) != (%u, %u, %u)\n", i, aclr_ptr->r, aclr_ptr->g,
             aclr_ptr->b, eclr_ptr->r, eclr_ptr->g, eclr_ptr->b);
      return 1;
    }
  }

  return 0;
}

int main(int argc, const char *argv[]) {
  if (argc != 4) {
    printf("Usage: voxel_test <level> <input> <expected>\n");
    return 1;
  }

  int ret = 0;

  size_t level_index = (size_t)strtoul(argv[1], NULL, 10);
  voxelpicPointCloud *input = NULL;
  voxelpicPointCloud *expected = NULL;
  voxelpicPointCloud *cloud_io_test = NULL;
  voxelpicOcTree *octree = NULL;
  voxelpicPointCloud *actual = NULL;
  voxelpicLevel *level = NULL;
  voxelpicLevel *level_io_test = NULL;

  input = voxelpicPointCloudNew(0);
  expected = voxelpicPointCloudNew(0);
  if (input == NULL || expected == NULL) {
    ret = VPIC_OUT_OF_MEMORY;
    goto error;
  }

  ret = voxelpicPointCloudLoad(argv[2], input);

  if (ret) {
    goto error;
  }

  ret = voxelpicPointCloudLoad(argv[3], expected);

  if (ret) {
    goto error;
  }

  const char *io_test_path = "cloud_io_test.dat";
  ret = voxelpicPointCloudSave(input, io_test_path);

  if (ret) {
    goto error;
  }

  cloud_io_test = voxelpicPointCloudNew(0);
  ret = voxelpicPointCloudLoad(io_test_path, cloud_io_test);

  if (compare_pointclouds(cloud_io_test, input)) {
    ret = 1;
    goto end;
  }

  octree = voxelpicOcTreeNew((1 + level_index) / 2, level_index);

  if (octree == NULL) {
    ret = VPIC_OUT_OF_MEMORY;
    goto error;
  }

  ret = voxelpicOcTreeBuild(octree, input);

  if (ret) {
    goto error;
  }

  actual = voxelpicPointCloudNew(0);
  if (actual == NULL) {
    ret = VPIC_OUT_OF_MEMORY;
    goto error;
  }

  ret = voxelpicOcTreeLevel(octree, level_index, &level);

  if (ret) {
    goto error;
  }

  ret = voxelpicLevelToCloud(level, actual, false);

  if (ret) {
    goto error;
  }

  if (compare_pointclouds(actual, expected)) {
    ret = 1;
    goto end;
  }

  const char *level_io_path = "level_io_test.dat";
  ret = voxelpicLevelSave(level, level_io_path);

  if (ret) {
    goto error;
  }

  level_io_test = voxelpicLevelNew(0);
  if (level_io_test == NULL) {
    ret = VPIC_OUT_OF_MEMORY;
    goto error;
  }

  ret = voxelpicLevelLoad(level_io_path, level_io_test);

  if (ret) {
    goto error;
  }

  ret = voxelpicLevelToCloud(level_io_test, actual, false);

  if (ret) {
    goto error;
  }

  if (compare_pointclouds(actual, expected)) {
    ret = 1;
    goto end;
  }

  goto end;

error:
  printf("%s\n", voxelpicError(ret));

end:
  if (octree) {
    voxelpicOcTreeFree(octree);
    octree = NULL;
  }

  if (input) {
    voxelpicPointCloudFree(input);
    input = NULL;
  }

  if (expected) {
    voxelpicPointCloudFree(expected);
    expected = NULL;
  }

  if (actual) {
    voxelpicPointCloudFree(actual);
    actual = NULL;
  }

  if (cloud_io_test) {
    voxelpicPointCloudFree(cloud_io_test);
    cloud_io_test = NULL;
  }

  if (level_io_test) {
    voxelpicLevelFree(level_io_test);
    level_io_test = NULL;
  }

  return ret;
}