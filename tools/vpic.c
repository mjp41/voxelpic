#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include <cargs.h>
#include <string.h>

#ifdef VOXELPIC_PNG
#include <png.h>
#endif

#include <voxelpic/voxelpic.h>
#include "version.h"

#define MODE_BUILD 0
#define MODE_ENCODE 1
#define MODE_DECODE 2

static struct cag_option options[] = {
    {.identifier = 'm',
     .access_letters = "m",
     .access_name = "mode",
     .value_name = "VALUE",
     .description = "Mode (build (default), encode, decode)"},
    {.identifier = 'i',
     .access_letters = "i",
     .access_name = "image",
     .value_name = "VALUE",
     .description = "Image path"},
    {.identifier = 'v',
     .access_letters = "v",
     .access_name = "voxels",
     .value_name = "VALUE",
     .description = "Voxels path"},
    {.identifier = 'd',
     .access_letters = "d",
     .access_name = "depth",
     .value_name = "VALUE",
     .description = "Octree depth (for build mode)"},
    {.identifier = 'c',
     .access_letters = "c",
     .access_name = "cloud",
     .value_name = "VALUE",
     .description = "Cloud file path"},
    {.identifier = 'h',
     .access_letters = "h",
     .access_name = "help",
     .value_name = NULL,
     .description = "Shows the command help"}};

struct vpic_configuration {
  int mode;
  size_t depth;
  const char *image_path;
  const char *voxels_path;
  const char *cloud_path;
};

int skip_whitespace(FILE *fp) {
  bool inComment = false;
  char c = fgetc(fp);
  while (inComment || c == ' ' || c == '\n' || c == '\t' || c == '\r' ||
         c == '#') {
    if (c == '#') {
      inComment = true;
    } else if (c == '\n') {
      inComment = false;
    }

    c = fgetc(fp);
    if (c == EOF) {
      return VPIC_ERROR;
    }
  }

  fseek(fp, -1, SEEK_CUR);
  return 0;
}

int ppm_read(const char *path, voxelpicImage **image_out) {
  FILE *fp;
  fp = fopen(path, "rb");
  if (fp == NULL) {
    return VPIC_IO_ERROR;
  }

  if (skip_whitespace(fp)) {
    goto file_error;
  }

  if (fgetc(fp) != 'P') {
    goto file_error;
  }

  if (fgetc(fp) != '6') {
    goto file_error;
  }

  if (skip_whitespace(fp)) {
    goto file_error;
  }

  size_t width, height;
  if (fscanf(fp, "%zu", &width) < 1) {
    goto file_error;
  }

  if (skip_whitespace(fp)) {
    goto file_error;
  }

  if (fscanf(fp, "%zu", &height) < 1) {
    goto file_error;
  }

  voxelpicImage *image = voxelpicImageNew(width, height);
  if (image == NULL) {
    fclose(fp);
    return VPIC_OUT_OF_MEMORY;
  }

  if (skip_whitespace(fp)) {
    goto file_error;
  }

  uint_least32_t max_value;

  if (fscanf(fp, "%u", &max_value) < 1) {
    goto file_error;
  }

  char c = fgetc(fp);
  if (c == EOF) {
    goto file_error;
  }

  assert(c == ' ' || c == '\n' || c == '\t');

  voxelpicColor *color_ptr = image->pixels;
  for (size_t r = 0; r < height; ++r) {
    for (size_t c = 0; c < width; ++c, ++color_ptr) {
      uint_least32_t value = fgetc(fp);
      if (value == EOF) {
        goto file_error;
      }

      color_ptr->r = (uint_least8_t)(value * 255 / max_value);

      value = fgetc(fp);
      if (value == EOF) {
        goto file_error;
      }

      color_ptr->g = (uint_least8_t)(value * 255 / max_value);

      value = fgetc(fp);
      if (value == EOF) {
        goto file_error;
      }

      color_ptr->b = (uint_least8_t)(value * 255 / max_value);

      color_ptr->a = 255;
    }
  }

  fclose(fp);

  *image_out = image;
  return VPIC_OK;

file_error:
  fclose(fp);
  return VPIC_IO_ERROR;
}

int ppm_write(const char *path, voxelpicImage *image) {
  FILE *fp;
  fp = fopen(path, "wb");
  if (fp == NULL) {
    return VPIC_IO_ERROR;
  }

  if (fprintf(fp, "P6\n%zu %zu\n255\n", image->width, image->height) < 0) {
    goto file_error;
  }

  voxelpicColor *color_ptr = image->pixels;
  for (size_t r = 0; r < image->height; ++r) {
    for (size_t c = 0; c < image->width; ++c, ++color_ptr) {
      if (fputc(color_ptr->r, fp) == EOF) {
        goto file_error;
      }

      if (fputc(color_ptr->g, fp) == EOF) {
        goto file_error;
      }

      if (fputc(color_ptr->b, fp) == EOF) {
        goto file_error;
      }
    }
  }

  fclose(fp);
  return VPIC_OK;

file_error:
  fclose(fp);
  return VPIC_IO_ERROR;
}

#ifdef VOXELPIC_PNG

int png_read(const char *path, voxelpicImage **image) {
  png_struct *png_ptr;
  png_info *info_ptr;
  int sig_read = 0;
  FILE *fp;

  *image = NULL;

  if ((fp = fopen(path, "rb")) == NULL) {
    return VPIC_ERROR;
  }

  png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

  if (png_ptr == NULL) {
    fclose(fp);
    return VPIC_ERROR;
  }

  info_ptr = png_create_info_struct(png_ptr);
  if (info_ptr == NULL) {
    fclose(fp);
    png_destroy_read_struct(&png_ptr, NULL, NULL);
    return VPIC_ERROR;
  }

  if (setjmp(png_jmpbuf(png_ptr))) {
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    fclose(fp);
    return VPIC_ERROR;
  }

  png_init_io(png_ptr, fp);

  png_set_sig_bytes(png_ptr, sig_read);

  png_read_info(png_ptr, info_ptr);

  size_t width = (size_t)png_get_image_width(png_ptr, info_ptr);
  size_t height = (size_t)png_get_image_height(png_ptr, info_ptr);
  int bit_depth = png_get_bit_depth(png_ptr, info_ptr);
  int color_type = png_get_color_type(png_ptr, info_ptr);

  if (bit_depth != 8) {
    printf("Invalid bit depth in PNG: %d\n", bit_depth);
    fclose(fp);
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    return VPIC_ERROR;
  }

  if (color_type != PNG_COLOR_TYPE_RGBA) {
    printf("PNG color type must be RGBA\n");
    fclose(fp);
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
  }

  png_byte **row_pointers = malloc(height * sizeof(png_byte *));
  for (size_t row = 0; row < height; ++row) {
    row_pointers[row] =
        png_malloc(png_ptr, png_get_rowbytes(png_ptr, info_ptr));
  }

  png_read_image(png_ptr, row_pointers);

  png_destroy_read_struct(&png_ptr, &info_ptr, NULL);

  fclose(fp);
  fp = NULL;

  voxelpicImage *vpic = voxelpicImageNew(width, height);
  if (vpic == NULL) {
    for (size_t row = 0; row < height; ++row) {
      free(row_pointers[row]);
    }

    free(row_pointers);
    return VPIC_OUT_OF_MEMORY;
  }

  png_byte *vpic_ptr = (png_byte *)vpic->pixels;
  size_t scan = width * 4;
  for (size_t row = 0; row < height; ++row, vpic_ptr += scan) {
    memcpy(vpic_ptr, row_pointers[row], scan);
    free(row_pointers[row]);
  }

  free(row_pointers);

  *image = vpic;

  return VPIC_OK;
}

int png_write(const char *path, voxelpicImage *image) {
  FILE *fp;
  png_struct *png_ptr;
  png_info *info_ptr;
  png_color *palette;

  fp = fopen(path, "wb");
  if (fp == NULL) {
    return VPIC_ERROR;
  }

  png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (png_ptr == NULL) {
    fclose(fp);
    return VPIC_ERROR;
  }

  info_ptr = png_create_info_struct(png_ptr);
  if (info_ptr == NULL) {
    fclose(fp);
    png_destroy_write_struct(&png_ptr, NULL);
    return VPIC_ERROR;
  }

  if (setjmp(png_jmpbuf(png_ptr))) {
    fclose(fp);
    png_destroy_write_struct(&png_ptr, &info_ptr);
    return VPIC_ERROR;
  }

  png_init_io(png_ptr, fp);

  png_set_IHDR(png_ptr, info_ptr, image->width, image->height, 8,
               PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_ADAM7,
               PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

  {
    png_text text_ptr[3];

    char key0[] = "Source";
    char text0[] = "vpic";
    text_ptr[0].key = key0;
    text_ptr[0].text = text0;
    text_ptr[0].compression = PNG_TEXT_COMPRESSION_NONE;
    text_ptr[0].itxt_length = 0;
    text_ptr[0].lang = NULL;
    text_ptr[0].lang_key = NULL;

    char key1[] = "Build";
    char text1[] = VPIC_BUILD_INFO;
    text_ptr[1].key = key1;
    text_ptr[1].text = text1;
    text_ptr[1].compression = PNG_TEXT_COMPRESSION_NONE;
    text_ptr[1].itxt_length = 0;
    text_ptr[1].lang = NULL;
    text_ptr[1].lang_key = NULL;

    char key2[] = "Format";
    char text2[] = "voxelpic";
    text_ptr[2].key = key2;
    text_ptr[2].text = text2;
    text_ptr[2].compression = PNG_TEXT_COMPRESSION_NONE;
    text_ptr[2].itxt_length = 0;
    text_ptr[2].lang = NULL;
    text_ptr[2].lang_key = NULL;

    png_set_text(png_ptr, info_ptr, text_ptr, 3);
  }

  png_write_info(png_ptr, info_ptr);

  png_byte *pixels = (png_byte *)image->pixels;
  png_byte **row_pointers = malloc(image->height * sizeof(png_byte *));

  for (size_t k = 0; k < image->height; ++k) {
    row_pointers[k] = pixels + k * image->width * 4;
  }

  png_write_image(png_ptr, row_pointers);

  png_write_end(png_ptr, info_ptr);

  png_destroy_write_struct(&png_ptr, &info_ptr);

  free(row_pointers);
  fclose(fp);

  return VPIC_OK;
}

#endif

bool ends_with(const char *str, const char *suffix) {
  size_t str_len = strlen(str);
  size_t suffix_len = strlen(suffix);
  if (suffix_len > str_len) {
    return false;
  }

  const char *end = str + str_len - suffix_len;
  return strcmp(end, suffix) == 0;
}

int main(int argc, char *argv[]) {
  printf("vpic %s\n", VPIC_BUILD_INFO);
  char identifier;
  struct vpic_configuration config = {.mode = MODE_BUILD,
                                      .depth = 9,
                                      .image_path = NULL,
                                      .cloud_path = NULL,
                                      .voxels_path = NULL};
  int rc = EXIT_SUCCESS;
  voxelpicEnum err;

  voxelpicPointCloud *cloud = NULL;
  voxelpicOcTree *octree = NULL;
  voxelpicLevel *voxels = NULL;
  voxelpicImage *image = NULL;

  cag_option_context context;
  cag_option_prepare(&context, options, CAG_ARRAY_SIZE(options), argc, argv);
  while (cag_option_fetch(&context)) {
    identifier = cag_option_get(&context);
    switch (identifier) {
    case 'm': {
      const char *mode = cag_option_get_value(&context);
      if (strcmp(mode, "build") == 0) {
        config.mode = MODE_BUILD;
      } else if (strcmp(mode, "encode") == 0) {
        config.mode = MODE_ENCODE;
      } else if (strcmp(mode, "decode") == 0) {
        config.mode = MODE_DECODE;
      } else {
        rc = EXIT_FAILURE;
        goto usage;
      }
      break;
    }

    case 'i':
      config.image_path = cag_option_get_value(&context);
      break;

    case 'c':
      config.cloud_path = cag_option_get_value(&context);
      break;

    case 'v':
      config.voxels_path = cag_option_get_value(&context);
      break;

    case 'd':
      config.depth = atoi(cag_option_get_value(&context));
      break;

    case 'h':
      goto usage;
    }
  }

  if (config.mode == MODE_BUILD) {
    if (config.cloud_path == NULL) {
      printf("cloud required for building input\n");
      err = EXIT_FAILURE;
      goto usage;
    }

    if (config.voxels_path == NULL) {
      printf("voxels path required for building output\n");
      err = EXIT_FAILURE;
      goto usage;
    }

    if (config.depth < 1) {
      printf("depth must be positive\n");
      err = EXIT_FAILURE;
      goto usage;
    }

    cloud = voxelpicPointCloudNew(0);

    if (cloud == NULL) {
      rc = VPIC_OUT_OF_MEMORY;
      goto error;
    }

    rc = voxelpicPointCloudLoad(config.cloud_path, cloud);

    if (rc) {
      goto error;
    }

    printf("Building an OcTree to depth %zu from %zu points...", config.depth,
           cloud->size);

    octree = voxelpicOcTreeNew(config.depth, config.depth);
    if (octree == NULL) {
      rc = VPIC_ERROR;
      goto error;
    }

    rc = voxelpicOcTreeBuild(octree, cloud);
    if (rc) {
      goto error;
    }

    printf("done.\n");

    rc = voxelpicOcTreeLevel(octree, config.depth, &voxels);

    if (rc) {
      goto error;
    }

    printf("Saving %zu voxels to %s...", voxelpicLevelSize(voxels),
           config.voxels_path);

    rc = voxelpicLevelSave(voxels, config.voxels_path);

    if (rc) {
      goto error;
    }

    printf("done.\n");

    goto cleanup;
  }

  if (config.mode == MODE_ENCODE) {
    if (config.voxels_path == NULL) {
      printf("voxels path required for encoding input\n");
      err = EXIT_FAILURE;
      goto usage;
    }

    if (config.image_path == NULL) {
      printf("image path required for encoding output\n");
      err = EXIT_FAILURE;
      goto usage;
    }

    printf("Loading voxels from %s...", config.voxels_path);

    voxels = voxelpicLevelNew(0);
    if (voxels == NULL) {
      rc = VPIC_OUT_OF_MEMORY;
      goto error;
    }

    rc = voxelpicLevelLoad(config.voxels_path, voxels);

    if (rc) {
      goto error;
    }

    printf("done\n");

    size_t width, height;
    rc = voxelpicLevelImageSize(voxels, &width, &height);
    if (rc) {
      goto error;
    }

    image = voxelpicImageNew(width, height);
    if (image == NULL) {
      rc = VPIC_OUT_OF_MEMORY;
      goto error;
    }

    printf("Encoding %zu voxels at level %zu to %s...",
           voxelpicLevelSize(voxels), voxelpicLevelDepth(voxels),
           config.image_path);

    rc = voxelpicLevelEncode(voxels, image);

    if (rc) {
      goto error;
    }

    if (ends_with(config.image_path, ".ppm")) {
      rc = ppm_write(config.image_path, image);
      if (rc) {
        goto error;
      }
    }
#ifdef VOXELPIC_PNG
    else if (ends_with(config.image_path, ".png")) {
      png_write(config.image_path, image);
    }
#endif
    else {
      printf("Unsupported image format. Supported: PPM (.ppm)");
#ifdef VOXELPIC_PNG
      printf(", PNG (.png)");
#endif
      printf("\n");
      err = EXIT_FAILURE;
      goto usage;
    }

    printf("done.\n");

    goto cleanup;
  }

  if (config.mode == MODE_DECODE) {
    if (config.image_path == NULL) {
      printf("image path required for decoding input\n");
      err = EXIT_FAILURE;
      goto usage;
    }

    if (config.voxels_path == NULL) {
      printf("voxels path required for decoding output\n");
      err = EXIT_FAILURE;
      goto usage;
    }

    printf("Decoding voxels from %s...", config.image_path);

    if (ends_with(config.image_path, ".ppm")) {
      rc = ppm_read(config.image_path, &image);
      if (rc) {
        goto error;
      }
    }
#ifdef VOXELPIC_PNG
    else if (ends_with(config.image_path, ".png")) {
      png_read(config.image_path, &image);
    }
#endif
    else {
      printf("Unsupported image format. Supported: PPM (.ppm)");
#ifdef VOXELPIC_PNG
      printf(", PNG (.png)");
#endif
      printf("\n");
      err = EXIT_FAILURE;
      goto usage;
    }

    voxels = voxelpicLevelNew(0);
    if (voxels == NULL) {
      rc = VPIC_OUT_OF_MEMORY;
      goto error;
    }

    rc = voxelpicLevelDecode(image, voxels);

    printf("done\n");

    if (rc) {
      goto error;
    }

    printf("Saving %zu voxels to %s...", voxelpicLevelSize(voxels),
           config.voxels_path);

    rc = voxelpicLevelSave(voxels, config.voxels_path);

    if (rc) {
      goto error;
    }

    printf("done.\n");

    goto cleanup;
  }

error:
  err = EXIT_FAILURE;
  printf("%s\n", voxelpicError(rc));

usage:
  printf("Usage: vpic [OPTION]...\n");
  printf("voxelpic voxel to image encoder/decoder.\n\n");
  cag_option_print(options, CAG_ARRAY_SIZE(options), stdout);

cleanup:
  if (cloud != NULL) {
    voxelpicPointCloudFree(cloud);
    cloud = NULL;
  }

  if (voxels != NULL && config.mode != MODE_BUILD) {
    voxelpicLevelFree(voxels);
    voxels = NULL;
  }

  if (octree != NULL) {
    voxelpicOcTreeFree(octree);
    octree = NULL;
  }

  if (image != NULL) {
    voxelpicImageFree(image);
    image = NULL;
  }

  printf("\n");

  return rc;
}
