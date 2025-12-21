#include "stdio.h"

#include "test.h"
#include "voxelpic/voxelpic.h"

int read_test_file(const char *path, size_t *level, voxelpicPointCloud **input,
                   voxelpicPointCloud **expected) {
  FILE *fp;
  int rc = 0;

  int_least32_t value;

  fp = fopen(path, "rb");
  if (fp == NULL) {
    perror("Error opening test file");
    return 1;
  }

  if (read_be(&value, fp) < 1) {
    goto file_error;
  }

  *level = (size_t)value;

  if (read_be(&value, fp) < 1) {
    goto file_error;
  }

  size_t num_positions = (size_t)value;

  voxelpicPointCloud *cloud = voxelpicPointCloudNew((size_t)num_positions);
  if (cloud == NULL) {
    return VPIC_OUT_OF_MEMORY;
  }

  *input = cloud;
  cloud->size = num_positions;
  voxelpicVec4 *pos_ptr = cloud->positions;
  voxelpicColor *clr_ptr = cloud->colors;
  for (size_t i = 0; i < num_positions; ++i, ++pos_ptr, ++clr_ptr) {
    pos_ptr->w = 1;
    clr_ptr->a = 255;
    if (fread(pos_ptr, sizeof(pos_ptr->x), 3, fp) < 3) {
      goto file_error;
    }

    if (fread(clr_ptr, sizeof(clr_ptr->r), 3, fp) < 3) {
      goto file_error;
    }
  }

  if (read_be(&value, fp) < 1) {
    goto file_error;
  }

  num_positions = (size_t)value;

  cloud = voxelpicPointCloudNew((size_t)num_positions);
  if (cloud == NULL) {
    return VPIC_OUT_OF_MEMORY;
  }

  *expected = cloud;
  cloud->size = num_positions;
  pos_ptr = cloud->positions;
  clr_ptr = cloud->colors;
  for (size_t i = 0; i < num_positions; ++i, ++pos_ptr, ++clr_ptr) {
    pos_ptr->w = 1;
    clr_ptr->a = 255;
    if (fread(pos_ptr, sizeof(pos_ptr->x), 3, fp) < 3) {
      goto file_error;
    }

    if (fread(clr_ptr, sizeof(clr_ptr->r), 3, fp) < 3) {
      goto file_error;
    }
  }

  goto cleanup;

file_error:
  if (*input != NULL) {
    voxelpicPointCloudFree(*input);
    *input = NULL;
  }

  if (*expected != NULL) {
    voxelpicPointCloudFree(*expected);
    *expected = NULL;
  }

  perror("Error reading from test file");
  rc = 1;

cleanup:
  fclose(fp);
  return rc;
}

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
  if (argc != 2) {
    printf("Missing test file\n");
    return 1;
  }

  int ret = 0;

  size_t level_index;
  voxelpicPointCloud *input = NULL;
  voxelpicPointCloud *expected = NULL;
  ret = read_test_file(argv[1], &level_index, &input, &expected);

  if (ret) {
    goto error;
  }

  const char *io_test_path = "cloud_io_test.dat";
  ret = voxelpicPointCloudSave(input, io_test_path);

  if (ret) {
    goto error;
  }

  voxelpicPointCloud *cloud_io_test = voxelpicPointCloudNew(0);
  ret = voxelpicPointCloudLoad(io_test_path, cloud_io_test);

  if (compare_pointclouds(cloud_io_test, input)) {
    ret = 1;
    goto end;
  }

  voxelpicOcTree *octree =
      voxelpicOcTreeNew((1 + level_index) / 2, level_index);

  if (octree == NULL) {
    ret = VPIC_OUT_OF_MEMORY;
    goto error;
  }

  ret = voxelpicOcTreeBuild(octree, input);

  if (ret) {
    goto error;
  }

  voxelpicPointCloud *actual = voxelpicPointCloudNew(0);
  if (actual == NULL) {
    ret = VPIC_OUT_OF_MEMORY;
    goto error;
  }

  voxelpicLevel *level = NULL;
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

  voxelpicLevel *level_io_test = voxelpicLevelNew(0);
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