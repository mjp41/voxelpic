#include "stdio.h"

#include "test.h"
#include "voxelpic/voxelpic.h"

int read_test_file(const char *path, size_t *level,
                   voxelpicPointCloud **input) {
  FILE *fp;
  int rc = 0;

  fp = fopen(path, "rb");
  if (fp == NULL) {
    perror("Error opening test file");
    return 1;
  }

  int_least32_t value;

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

  goto cleanup;

file_error:
  if (*input != NULL) {
    voxelpicPointCloudFree(*input);
    *input = NULL;
  }

  perror("Error reading from test file");
  rc = 1;

cleanup:
  fclose(fp);
  return rc;
}

int main(int argc, char *argv[]) {
  voxelpicEnum ret;
  size_t level_index;
  voxelpicPointCloud *expected;
  read_test_file(argv[1], &level_index, &expected);

  voxelpicOcTree *octree = voxelpicOcTreeNew(4, level_index);
  if (octree == NULL) {
    ret = VPIC_OUT_OF_MEMORY;
    goto error;
  }

  ret = voxelpicOcTreeBuild(octree, expected);

  if (ret) {
    goto error;
  }

  voxelpicLevel *input_level;
  ret = voxelpicOcTreeLevel(octree, level_index, &input_level);

  if (ret) {
    goto error;
  }

  size_t width, height;
  ret = voxelpicLevelImageSize(input_level, &width, &height);

  if (ret) {
    goto error;
  }

  voxelpicImage *image = voxelpicImageNew(width, height);
  if (image == NULL) {
    ret = VPIC_OUT_OF_MEMORY;
    goto error;
  }

  ret = voxelpicLevelEncode(input_level, image);

  if (ret) {
    goto error;
  }

  voxelpicLevel *output_level = voxelpicLevelNew(0);

  if (output_level == NULL) {
    ret = VPIC_OUT_OF_MEMORY;
    goto error;
  }

  ret = voxelpicLevelDecode(image, output_level);

  if (ret) {
    goto error;
  }

  voxelpicPointCloud *actual = voxelpicPointCloudNew(0);

  if (actual == NULL) {
    ret = VPIC_OUT_OF_MEMORY;
    goto error;
  }

  ret = voxelpicLevelToCloud(output_level, actual, false);

  if (ret) {
    goto error;
  }

  if (actual->size != expected->size) {
    printf("Cloud size does not match (%zu != %zu)\n", actual->size,
           expected->size);
    ret = 1;
    goto end;
  }

  voxelpicVec4 *apos_ptr = actual->positions;
  voxelpicColor *aclr_ptr = actual->colors;
  voxelpicVec4 *epos_ptr = expected->positions;
  voxelpicColor *eclr_ptr = expected->colors;
  for (size_t i = 0; i < actual->size;
       ++i, ++apos_ptr, ++epos_ptr, ++aclr_ptr, ++eclr_ptr) {
    if (voxelpicVec3Compare(apos_ptr, epos_ptr)) {
      printf("%zu: pos(%f, %f, %f) != pos(%f, %f, %f)\n", i, apos_ptr->x,
             apos_ptr->y, apos_ptr->z, epos_ptr->x, epos_ptr->y, epos_ptr->z);
      ret = 1;
      goto end;
    }

    if (aclr_ptr->value != eclr_ptr->value) {
      printf("%zu: clr(%u, %u, %u) != clr(%u, %u, %u)\n", i, aclr_ptr->r,
             aclr_ptr->g, aclr_ptr->b, eclr_ptr->r, eclr_ptr->g, eclr_ptr->b);
      ret = 1;
      goto end;
    }
  }

  goto end;

error:
  printf("%s\n", voxelpicError(ret));

end:
  if (octree) {
    voxelpicOcTreeFree(octree);
    octree = NULL;
  }

  if (expected) {
    voxelpicPointCloudFree(expected);
    expected = NULL;
  }

  if (image) {
    voxelpicImageFree(image);
    image = NULL;
  }

  if (output_level) {
    voxelpicLevelFree(output_level);
    output_level = NULL;
  }

  if (actual) {
    voxelpicPointCloudFree(actual);
    actual = NULL;
  }

  return ret;
}