#include "stdio.h"
#include "stdlib.h"

#include "voxelpic/voxelpic.h"

int main(int argc, char *argv[]) {
  if (argc != 3) {
    printf("Usage: image_test <level> <input>\n");
    return 1;
  }

  voxelpicEnum ret;
  size_t level_index = (size_t)strtoul(argv[1], NULL, 10);
  voxelpicPointCloud *expected = NULL;
  voxelpicOcTree *octree = NULL;
  voxelpicImage *image = NULL;
  voxelpicLevel *output_level = NULL;
  voxelpicPointCloud *actual = NULL;

  expected = voxelpicPointCloudNew(0);
  if (expected == NULL) {
    ret = VPIC_OUT_OF_MEMORY;
    goto error;
  }

  ret = voxelpicPointCloudLoad(argv[2], expected);
  if (ret) {
    goto error;
  }

  octree = voxelpicOcTreeNew(4, level_index);
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

  image = voxelpicImageNew(width, height);
  if (image == NULL) {
    ret = VPIC_OUT_OF_MEMORY;
    goto error;
  }

  ret = voxelpicLevelEncode(input_level, image);

  if (ret) {
    goto error;
  }

  output_level = voxelpicLevelNew(0);

  if (output_level == NULL) {
    ret = VPIC_OUT_OF_MEMORY;
    goto error;
  }

  ret = voxelpicLevelDecode(image, output_level);

  if (ret) {
    goto error;
  }

  actual = voxelpicPointCloudNew(0);

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