#include "voxelpic.h"
#include <assert.h>
#include <complex.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef union voxelpic_vec4i_t {
  int_least32_t values[4];
  struct {
    int_least32_t x;
    int_least32_t y;
    int_least32_t z;
    int_least32_t w;
  };
  struct {
    int_least32_t r;
    int_least32_t g;
    int_least32_t b;
    int_least32_t a;
  };
} Vec4i;

typedef voxelpicVec4 Vec;
typedef voxelpicColor Color;
const Vec BOUNDS_MIN = {{-1.0f, -1.0f, -1.0f, 1.0f}};
const Vec BOUNDS_MAX = {{1.0f, 1.0f, 1.0f, 1.0f}};
const Vec VEC_ZERO = {{0.0f, 0.0f, 0.0f, 0.0f}};

typedef struct voxel_s {
  union {
    uint_least16_t pos[4];
    uint_least64_t index;
  };

  Vec position;
  Vec center;
  Vec4i color;
} Voxel;

typedef struct level_s {
  size_t depth;
  int_least16_t side;
  size_t capacity;
  size_t size;
  Voxel *voxels;
  bool by_ref;
} Level;

typedef struct bounding_box_s {
  Vec min;
  Vec max;
} BoundingBox;

typedef struct octree_s {
  size_t min_depth;
  size_t max_depth;
  size_t num_levels;
  Level *levels;
  BoundingBox bounds;
} OcTree;

const size_t ALIGNMENT = 16; // SSE requires 16-byte alignment

#if defined(_MSC_VER)
#include <malloc.h>
#define aligned_malloc(size, align) _aligned_malloc(size, align)
#define aligned_free(ptr) _aligned_free(ptr)
#else
#include <stdlib.h>
#define aligned_malloc(size, align) aligned_alloc(align, size)
#define aligned_free(ptr) free(ptr)
#endif

static void *alloc_simd_vector(size_t requested_size) {
  size_t size = requested_size;
  if (size % ALIGNMENT != 0) {
    size += ALIGNMENT - size % ALIGNMENT;
  }

  return aligned_malloc(size, ALIGNMENT);
}

int voxelpicVec4Compare(voxelpicVec4 *lhs, voxelpicVec4 *rhs) {
  for (size_t j = 0; j < 4; ++j) {
    if (lhs->values[j] < rhs->values[j]) {
      return -1;
    }
    if (lhs->values[j] > rhs->values[j]) {
      return 1;
    }
  }

  return 0;
}

int voxelpicVec3Compare(voxelpicVec4 *lhs, voxelpicVec4 *rhs) {
  for (size_t j = 0; j < 3; ++j) {
    if (lhs->values[j] < rhs->values[j]) {
      return -1;
    }

    if (lhs->values[j] > rhs->values[j]) {
      return 1;
    }
  }

  return 0;
}

voxelpicEnum cloud_grow(voxelpicPointCloud *cloud, size_t new_capacity) {
  Vec *positions = alloc_simd_vector(new_capacity * sizeof(Vec));
  if (positions == NULL) {
    return VPIC_OUT_OF_MEMORY;
  }

  Color *colors = alloc_simd_vector(new_capacity * sizeof(Color));
  if (colors == NULL) {
    aligned_free(positions);
    return VPIC_OUT_OF_MEMORY;
  }

  if (cloud->capacity == 0) {
    assert(cloud->positions == NULL);
    assert(cloud->colors == NULL);
    cloud->positions = positions;
    cloud->colors = colors;
    cloud->capacity = new_capacity;
    cloud->size = 0;
    return VPIC_OK;
  }

  memcpy((void *)positions, (void *)cloud->positions,
         cloud->size * sizeof(Vec));
  memcpy((void *)colors, (void *)cloud->colors, cloud->size * sizeof(Color));
  aligned_free(cloud->positions);
  aligned_free(cloud->colors);
  cloud->positions = positions;
  cloud->colors = colors;
  cloud->capacity = new_capacity;

  return VPIC_OK;
}

voxelpicPointCloud *voxelpicPointCloudNew(size_t capacity) {
  voxelpicPointCloud *cloud = malloc(sizeof(voxelpicPointCloud));
  if (cloud == NULL) {
    return NULL;
  }

  cloud->capacity = capacity;
  cloud->size = 0;
  cloud->positions = NULL;
  cloud->colors = NULL;
  if (capacity == 0) {
    return cloud;
  }

  if (cloud_grow(cloud, capacity)) {
    free(cloud);
    return NULL;
  }

  return cloud;
}

void voxelpicPointCloudFree(voxelpicPointCloud *cloud) {
  if (cloud->positions) {
    aligned_free(cloud->positions);
  }

  if (cloud->colors) {
    aligned_free(cloud->colors);
  }

  free(cloud);
}

typedef struct cloud_stats_s {
  Vec mean;
  Vec std;
} CloudStats;

static int compare_voxels(const void *lhs_opaque, const void *rhs_opaque) {
  Voxel *lhs = (Voxel *)lhs_opaque;
  Voxel *rhs = (Voxel *)rhs_opaque;
  if (lhs->index < rhs->index) {
    return -1;
  }

  if (lhs->index > rhs->index) {
    return 1;
  }

  return 0;
}

voxelpicEnum level_grow(Level *level, size_t new_capacity) {
  Voxel *voxels = malloc(new_capacity * sizeof(Voxel));
  if (voxels == NULL) {
    return VPIC_OUT_OF_MEMORY;
  }

  if (level->capacity == 0) {
    assert(level->voxels == NULL);
    level->voxels = voxels;
    level->capacity = new_capacity;
    level->size = 0;
    return VPIC_OK;
  }

  memcpy((void *)voxels, (void *)level->voxels, level->size * sizeof(Voxel));
  free(level->voxels);
  level->capacity = new_capacity;
  level->voxels = voxels;

  return VPIC_OK;
}

voxelpicEnum build_level(Level *level, Voxel *voxels, size_t size) {
  qsort(voxels, size, sizeof(Voxel), compare_voxels);

  // count the unique values
  size_t num_voxels = 1;
  Voxel *src_ptr = voxels;
  uint_least64_t last = src_ptr->index;
  ++src_ptr;
  for (size_t i = 1; i < size; ++i, ++src_ptr) {
    if (src_ptr->index != last) {
      num_voxels += 1;
      last = src_ptr->index;
    }
  }

  if (level->capacity < num_voxels) {
    if (level_grow(level, num_voxels)) {
      return VPIC_OUT_OF_MEMORY;
    }
  }

  level->size = num_voxels;

  src_ptr = voxels;
  Voxel *dst_ptr = level->voxels;

  dst_ptr->position = VEC_ZERO;
  dst_ptr->index = src_ptr->index;
  dst_ptr->center = src_ptr->center;
  dst_ptr->color = src_ptr->color;
  int_least32_t count = 1;
  last = src_ptr->index;
  ++src_ptr;
  for (size_t i = 1; i < size; ++i, ++src_ptr) {
    if (src_ptr->index != last) {
      for (size_t j = 0; j < 4; ++j) {
        dst_ptr->color.values[j] /= count;
        assert(dst_ptr->color.values[j] >= 0 && dst_ptr->color.values[j] < 256);
      }

      ++dst_ptr;
      count = 1;

      dst_ptr->position = VEC_ZERO;
      dst_ptr->index = src_ptr->index;
      dst_ptr->center = src_ptr->center;
      dst_ptr->color = src_ptr->color;
      last = src_ptr->index;
      continue;
    }

    count += 1;
    for (size_t j = 0; j < 4; ++j) {
      dst_ptr->color.values[j] += src_ptr->color.values[j];
    }
  }

  for (size_t j = 0; j < 4; ++j) {
    dst_ptr->color.values[j] /= count;
    assert(dst_ptr->color.values[j] >= 0 && dst_ptr->color.values[j] < 256);
  }

  return VPIC_OK;
}

voxelpicOcTree *voxelpicOcTreeNew(size_t min_depth, size_t max_depth) {
  OcTree *octree = malloc(sizeof(OcTree));
  if (octree == NULL) {
    return NULL;
  }

  octree->min_depth = min_depth;
  octree->max_depth = max_depth;
  octree->num_levels = max_depth - min_depth + 1;
  octree->bounds.min = BOUNDS_MIN;
  octree->bounds.max = BOUNDS_MAX;
  octree->levels = malloc(sizeof(Level) * octree->num_levels);
  if (octree->levels == NULL) {
    free(octree);
    return NULL;
  }

  float scale = 0.5f;
  int_least16_t side = 1;
  for (size_t depth = 0; depth < octree->max_depth + 1;
       ++depth, scale *= 0.5f, side *= 2) {
    if (depth >= octree->min_depth) {
      Level *level_ptr = octree->levels + depth - octree->min_depth;
      level_ptr->depth = depth;
      level_ptr->side = side;
      level_ptr->capacity = 0;
      level_ptr->size = 0;
      level_ptr->voxels = NULL;
      level_ptr->by_ref = true;
    }
  }

  return octree;
}

voxelpicEnum voxelpicOcTreeBuild(voxelpicOcTree *octree_opaque,
                                 const voxelpicPointCloud *point_cloud) {
  OcTree *octree = (OcTree *)octree_opaque;
  size_t cloud_size = point_cloud->size;
  size_t cloud_bytes = sizeof(Vec) * cloud_size;
  voxelpicEnum ret = VPIC_OK;

  if (point_cloud == NULL) {
    return VPIC_BAD_POINTER;
  }

  if (point_cloud->size == 0) {
    return VPIC_UNINITIALIZED;
  }

  Vec *positions = alloc_simd_vector(cloud_bytes);
  Voxel *voxels = NULL;
  if (positions == NULL) {
    ret = VPIC_OUT_OF_MEMORY;
    goto end;
  }

  memcpy(positions, point_cloud->positions, cloud_bytes);

  voxels = (Voxel *)malloc(sizeof(Voxel) * cloud_size);
  if (voxels == NULL) {
    ret = VPIC_OUT_OF_MEMORY;
    goto end;
  }

  Vec *position_ptr = positions;
  Color *color_ptr = point_cloud->colors;
  Voxel *voxel_ptr = voxels;
  for (size_t i = 0; i < cloud_size;
       ++i, ++position_ptr, ++color_ptr, ++voxel_ptr) {
    voxel_ptr->index = 0;
    voxel_ptr->position = *position_ptr;
    voxel_ptr->color.r = color_ptr->r;
    voxel_ptr->color.g = color_ptr->g;
    voxel_ptr->color.b = color_ptr->b;
    voxel_ptr->color.a = color_ptr->a;
    voxel_ptr->center = VEC_ZERO;
  }

  float scale = 0.5f;
  int_least16_t side = 1;
  for (size_t level = 0; level < octree->max_depth + 1;
       ++level, scale *= 0.5f, side *= 2) {
    if (level >= octree->min_depth) {
      Level *level_ptr = octree->levels + level - octree->min_depth;
      ret = build_level(level_ptr, voxels, cloud_size);
      if (ret) {
        goto end;
      }
    }

    Voxel *voxel_ptr = voxels;
    for (size_t i = 0; i < cloud_size; ++i, ++voxel_ptr) {
      for (size_t j = 0; j < 3; ++j) {
        if (voxel_ptr->position.values[j] > voxel_ptr->center.values[j]) {
          voxel_ptr->pos[j] = (voxel_ptr->pos[j] * 2) + 1;
          voxel_ptr->center.values[j] += scale;
        } else {
          voxel_ptr->pos[j] = voxel_ptr->pos[j] * 2;
          voxel_ptr->center.values[j] -= scale;
        }
      }
    }
  }

end:
  if (positions) {
    aligned_free(positions);
  }

  if (voxels) {
    free(voxels);
  }

  return ret;
}

void voxelpicOcTreeFree(voxelpicOcTree *octree_opaque) {
  OcTree *octree = (OcTree *)octree_opaque;
  if (octree->levels) {
    for (size_t i = 0; i < octree->num_levels; ++i) {
      if (octree->levels[i].voxels) {
        free(octree->levels[i].voxels);
      }
    }

    free(octree->levels);
  }
  free(octree);
}

size_t voxelpicOcTreeMinDepth(const voxelpicOcTree *octree_opaque) {
  OcTree *octree = (OcTree *)octree_opaque;
  return octree->min_depth;
}

size_t voxelpicOcTreeMaxDepth(const voxelpicOcTree *octree_opaque) {
  OcTree *octree = (OcTree *)octree_opaque;
  return octree->min_depth + octree->num_levels;
}

voxelpicEnum voxelpicOcTreeLevel(const voxelpicOcTree *octree_opaque,
                                 size_t index, voxelpicLevel **level) {
  OcTree *octree = (OcTree *)octree_opaque;
  if (octree->levels == NULL) {
    return VPIC_UNINITIALIZED;
  }

  if (index < octree->min_depth) {
    return VPIC_INVALID_LEVEL;
  }

  index -= octree->min_depth;
  if (index >= octree->num_levels) {
    return VPIC_INVALID_LEVEL;
  }

  *level = octree->levels + index;
  return VPIC_OK;
}

size_t voxelpicLevelSize(const voxelpicLevel *level_opaque) {
  const Level *level = (const Level *)level_opaque;
  return level->size;
}

size_t voxelpicLevelDepth(const voxelpicLevel *level_opaque) {
  const Level *level = (const Level *)level_opaque;
  return level->depth;
}

voxelpicEnum voxelpicLevelToCloud(const voxelpicLevel *level_opaque,
                                  voxelpicPointCloud *cloud, bool truncate) {
  const Level *level = (const Level *)level_opaque;

  if (cloud == NULL) {
    return VPIC_BAD_POINTER;
  }

  if (cloud->capacity != 0) {
    if (cloud->positions == NULL) {
      return VPIC_BAD_POINTER;
    }

    if (cloud->colors == NULL) {
      return VPIC_BAD_POINTER;
    }

    if (cloud->capacity < level->size && !truncate) {
      return VPIC_TOO_SMALL;
    }
  } else {
    cloud->capacity = level->size;
    cloud->positions = alloc_simd_vector(level->size * sizeof(Vec));
    if (cloud->positions == NULL) {
      return VPIC_OUT_OF_MEMORY;
    }

    cloud->colors = alloc_simd_vector(level->size * sizeof(Vec));
    if (cloud->colors == NULL) {
      aligned_free(cloud->positions);
      return VPIC_OUT_OF_MEMORY;
    }
  }

  cloud->size = level->size;
  const Voxel *voxel_ptr = level->voxels;
  Vec *position_ptr = cloud->positions;
  Color *color_ptr = cloud->colors;
  size_t size = level->size;
  if (truncate) {
    size = size < cloud->capacity ? size : cloud->capacity;
  }
  for (size_t i = 0; i < size; ++i, ++voxel_ptr, ++position_ptr, ++color_ptr) {
    *position_ptr = voxel_ptr->center;
    color_ptr->r = (uint_least8_t)voxel_ptr->color.r;
    color_ptr->g = (uint_least8_t)voxel_ptr->color.g;
    color_ptr->b = (uint_least8_t)voxel_ptr->color.b;
    color_ptr->a = (uint_least8_t)voxel_ptr->color.a;
  }

  return VPIC_OK;
}

const char *voxelpicError(voxelpicEnum errorCode) {
  switch (errorCode) {
  case VPIC_OK:
    return "OK";

  case VPIC_ERROR:

    return "Unspecified error";

  case VPIC_TOO_SMALL:
    return "Point cloud was too small";

  case VPIC_BAD_POINTER:
    return "Bad pointer";

  case VPIC_INVALID_LEVEL:
    return "Level does not exist";

  case VPIC_OUT_OF_MEMORY:
    return "Out of memory";

  case VPIC_OUT_OF_RANGE:
    return "Out of range";

  case VPIC_UNINITIALIZED:
    return "Not initialized";

  case VPIC_IO_ERROR:
    return "I/O error";
  }

  return "Invalid error code";
}

// This scheme is based on the hue-codec described here:
// https://github.com/jdtremaine/hue-codec/

// we artificially limit the range to avoid errors at the wraparound
const uint_least32_t ENC_RANGE = 1274;
const uint_least32_t ENC_MIN = 128;
const uint_least32_t ENC_MAX = 1402;

static inline void value_to_color(uint_least32_t value, uint_least32_t min,
                                  uint_least32_t range, Color *color) {
  assert(value >= min && value < min + range);
  const uint_least32_t rem = ENC_RANGE % range ? 1 : 0;
  const uint_least32_t d_normal =
      (value - min) * ENC_RANGE / range + ENC_MIN + rem;

  color->a = 255;
  if (d_normal <= 255) {
    color->r = 255;
    color->g = (uint_least8_t)d_normal;
    color->b = 0;
  } else if (d_normal <= 510) {
    color->r = (uint_least8_t)(510 - d_normal);
    color->g = 255;
    color->b = 0;
  } else if (d_normal <= 765) {
    color->r = 0;
    color->g = 255;
    color->b = (uint_least8_t)(d_normal - 510);
  } else if (d_normal <= 1020) {
    color->r = 0;
    color->g = (uint_least8_t)(1020 - d_normal);
    color->b = 255;
  } else if (d_normal <= 1275) {
    color->r = (uint_least8_t)(d_normal - 1020);
    color->g = 0;
    color->b = 255;
  } else {
    color->r = 255;
    color->g = 0;
    color->b = (uint_least8_t)(1530 - d_normal);
  }
}

#define ABS(x) ((x) < 0 ? -(x) : (x))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
const uint_least32_t EMPTY = 0xFFFFFFFF;

static inline void color_to_value(Color color, uint_least32_t min,
                                  uint_least32_t range, voxelpicInt *value) {
  if (!(color.r != 0 || color.g != 0 || color.b != 0)) {
    *value = EMPTY;
    return;
  }

  const uint_least32_t r = color.r;
  const uint_least32_t g = color.g;
  const uint_least32_t b = color.b;
  const uint_least32_t rg = r < g ? g - r : r - g;
  const uint_least32_t rb = r < b ? b - r : r - b;
  const uint_least32_t gb = g < b ? b - g : g - b;
  uint_least32_t d_normal = 0;
  if (!(r < g || r < b || gb >= 128)) {
    if (g >= b) {
      d_normal = gb;
    } else {
      d_normal = 1529 - gb;
    }
  } else if (!(g < r || g < b || rb >= 128)) {
    d_normal = 510 + b - r;
  } else if (!(b < r || b < g || rg >= 128)) {
    d_normal = 1020 + r - g;
  } else if (!(b > r || b > g || rg >= 128)) {
    d_normal = 255 + g - r;
  } else if (!(r > g || r > b || gb >= 128)) {
    d_normal = 765 + b - g;
  } else if (!(g > r || g > b || rb >= 128)) {
    d_normal = 1275 + r - b;
  } else {
    d_normal = 0;
  }

  assert(0 <= d_normal && d_normal <= 1529);
  d_normal = MAX(MIN(d_normal, ENC_MAX), ENC_MIN);
  d_normal = ((d_normal - ENC_MIN) * range) / ENC_RANGE + min;
  *value = (voxelpicInt)d_normal;
}

voxelpicEnum voxelpicValueToColor(const voxelpicInt *values, voxelpicInt *min,
                                  voxelpicInt *max, Color *colors,
                                  size_t count) {
  if (values == NULL) {
    return VPIC_BAD_POINTER;
  }

  if (colors == NULL) {
    return VPIC_BAD_POINTER;
  }

  const voxelpicInt *value_ptr = values;
  if (*min == *max) {
    // compute min and max
    for (size_t i = 0; i < count; ++i, ++value_ptr) {
      *min = MIN(*min, *value_ptr);
      *max = MAX(*max, *value_ptr);
    }

    *max += 1;
    value_ptr = values;
  }

  if (*max - *min > ENC_RANGE) {
    return VPIC_OUT_OF_RANGE;
  }

  const voxelpicInt range = *max - *min;

  Color *color_ptr = colors;
  for (size_t i = 0; i < count; ++i, ++value_ptr, ++color_ptr) {
    value_to_color(*value_ptr, *min, range, color_ptr);
  }

  return VPIC_OK;
}

voxelpicEnum voxelpicColorToValue(const voxelpicColor *colors, voxelpicInt min,
                                  voxelpicInt max, voxelpicInt *values,
                                  size_t count) {
  if (values == NULL) {
    return VPIC_BAD_POINTER;
  }

  if (colors == NULL) {
    return VPIC_BAD_POINTER;
  }

  voxelpicInt *value_ptr = values;
  const Color *color_ptr = colors;
  const voxelpicInt range = max - min;
  for (size_t i = 0; i < count; ++i, ++value_ptr, ++color_ptr) {
    color_to_value(*color_ptr, min, range, value_ptr);
  }

  return VPIC_OK;
}

#define PIXELS_PER_BLOCK 1
#define BB_RESOLUTION 128

voxelpicEnum voxelpicLevelImageSize(const voxelpicLevel *level_opaque,
                                    size_t *width, size_t *height) {
  Level *level = (Level *)level_opaque;
  if (level->depth < 6) {
    return VPIC_INVALID_LEVEL;
  }

  size_t scale = (size_t)1 << (level->depth - 6);
  *height = ((135 * PIXELS_PER_BLOCK) * scale);
  *width = (16 * *height) / 9;
  return VPIC_OK;
}

voxelpicImage *voxelpicImageNew(size_t width, size_t height) {
  voxelpicImage *image = malloc(sizeof(voxelpicImage));
  if (image == NULL) {
    return NULL;
  }

  image->width = width;
  image->height = height;
  image->pixels = malloc(width * height * sizeof(Color));
  if (image->pixels == NULL) {
    free(image);
    return NULL;
  }

  return image;
}

void voxelpicImageFree(voxelpicImage *image) {
  free(image->pixels);
  free(image);
}

static inline void fit_down(uint_least32_t *value, const uint_least32_t res) {
  const uint_least32_t rem = *value % res;
  if (rem == 0) {
    return;
  }

  *value = *value - rem;
}

static inline void fit_up(uint_least32_t *value, const uint_least32_t res) {
  const uint_least32_t rem = *value % res;
  if (rem == 0) {
    return;
  }

  *value = *value + res - rem;
}

static inline void center(uint_least32_t min, uint_least32_t range, size_t size,
                          size_t max_size, uint_least32_t *cmin) {
  if (range <= size) {
    // center within window
    uint_least32_t offset = ((uint_least32_t)size - range) / 2;
    if (min < offset) {
      *cmin = 0;
    } else if (offset + range > max_size) {
      *cmin = (uint_least32_t)(max_size - size);
    } else {
      *cmin = min - offset;
    }
  } else {
    // center window within range
    uint_least32_t offset = (range - (uint_least32_t)size) / 2;
    *cmin = min + offset;
  }
}

voxelpicEnum voxelpicLevelEncode(const voxelpicLevel *level_opaque,
                                 voxelpicImage *image) {
  const Level *level = (const Level *)level_opaque;
  const size_t side = (size_t)1 << level->depth;
  const size_t wide = (image->width / PIXELS_PER_BLOCK) / 4;
  const Voxel *voxel_ptr = level->voxels;

  uint_least32_t i_min, k_min, i_max, k_max;
  i_min = k_min = (uint_least32_t)side;
  i_max = k_max = 0;
  for (size_t v = 0; v < level->size; ++v, ++voxel_ptr) {
    const uint_least32_t i = voxel_ptr->pos[0];
    const uint_least32_t k = voxel_ptr->pos[2];
    i_min = MIN(i_min, i);
    i_max = MAX(i_max, i + 1);
    k_min = MIN(k_min, k);
    k_max = MAX(k_max, k + 1);
  }

  const uint_least32_t res = (uint_least32_t)(side / BB_RESOLUTION);
  fit_down(&i_min, res);
  fit_up(&i_max, res);
  fit_down(&k_min, res);
  fit_up(&k_max, res);

  const uint_least32_t i_range = i_max - i_min;
  const uint_least32_t k_range = k_max - k_min;

  uint_least32_t x_left, z_left;
  center(i_min, i_range, wide, side, &z_left);
  center(k_min, k_range, wide, side, &x_left);

  const size_t scan = image->width;
  const size_t cell_scan_right = 2 * wide;

  Color *depth_ptr = image->pixels;
  Color *color_ptr = image->pixels + scan * side;
  for (size_t r = 0; r < side; ++r) {
    for (size_t c = 0; c < scan; ++c, ++depth_ptr, ++color_ptr) {
      depth_ptr->value = EMPTY;
      color_ptr->value = 0;
    }
  }

  voxel_ptr = level->voxels;
  for (size_t v = 0; v < level->size; ++v, ++voxel_ptr) {
    const uint_least32_t i = voxel_ptr->pos[0];
    const uint_least32_t j = voxel_ptr->pos[1];
    const uint_least32_t k = voxel_ptr->pos[2];
    const uint_least8_t red = (uint_least8_t)voxel_ptr->color.r;
    const uint_least8_t green = (uint_least8_t)voxel_ptr->color.g;
    const uint_least8_t blue = (uint_least8_t)voxel_ptr->color.b;
    const uint_least8_t alpha = (uint_least8_t)voxel_ptr->color.a;
    const Color color = {{red, green, blue, alpha}};

    assert(i >= i_min && i < i_max);
    assert(j >= 0 && j < side);
    assert(k >= k_min && k < k_max);

    const size_t row = side - j - 1;

    Color *depth_ptr = image->pixels + row * scan;
    Color *color_ptr = image->pixels + (row + side) * scan;
    if (k >= x_left && k - x_left < wide) {
      const size_t x_col = k - x_left;
      assert(x_col >= 0 && x_col < wide);

      Color *x_depth_0 = depth_ptr + x_col;
      Color *x_color_0 = color_ptr + x_col;

      if (x_depth_0->value == EMPTY || i < x_depth_0->value) {
        x_depth_0->value = i;
        *x_color_0 = color;
      } else {
        assert(x_depth_0->value >= i_min && x_depth_0->value < i_max);
      }

      Color *x_depth_1 = x_depth_0 + cell_scan_right;
      Color *x_color_1 = x_color_0 + cell_scan_right;

      if (x_depth_1->value == EMPTY || i > x_depth_1->value) {
        x_depth_1->value = i;
        *x_color_1 = color;
      } else {
        assert(x_depth_1->value >= i_min && x_depth_1->value < i_max);
      }
    }

    if (i >= z_left && i - z_left < wide) {
      const size_t z_col = i - z_left;
      assert(z_col >= 0 && z_col < wide);

      Color *z_depth_0 = depth_ptr + wide + z_col;
      Color *z_color_0 = color_ptr + wide + z_col;

      if (z_depth_0->value == EMPTY || k < z_depth_0->value) {
        z_depth_0->value = k;
        *z_color_0 = color;
      } else {
        assert(z_depth_0->value >= k_min && z_depth_0->value < k_max);
      }

      Color *z_depth_1 = z_depth_0 + cell_scan_right;
      Color *z_color_1 = z_color_0 + cell_scan_right;

      if (z_depth_1->value == EMPTY || k > z_depth_1->value) {
        z_depth_1->value = k;
        *z_color_1 = color;
      } else {
        assert(z_depth_1->value >= k_min && z_depth_1->value < k_max);
      }
    }
  }

  Color *row_ptr = image->pixels;
  for (size_t r = 0; r < side; ++r, row_ptr += scan) {
    Color *x0_ptr = row_ptr;
    Color *z0_ptr = x0_ptr + wide;
    Color *x1_ptr = z0_ptr + wide;
    Color *z1_ptr = x1_ptr + wide;
    for (size_t c = 0; c < wide; ++c, ++x0_ptr, ++x1_ptr, ++z0_ptr, ++z1_ptr) {
      if (x0_ptr->value == EMPTY) {
        x0_ptr->value = 0;
      } else {
        value_to_color(x0_ptr->value, i_min, i_range, x0_ptr);
      }

      if (x1_ptr->value == EMPTY) {
        x1_ptr->value = 0;
      } else {
        value_to_color(x1_ptr->value, i_min, i_range, x1_ptr);
      }

      if (z0_ptr->value == EMPTY) {
        z0_ptr->value = 0;
      } else {
        value_to_color(z0_ptr->value, k_min, k_range, z0_ptr);
      }

      if (z1_ptr->value == EMPTY) {
        z1_ptr->value = 0;
      } else {
        value_to_color(z1_ptr->value, k_min, k_range, z1_ptr);
      }
    }
  }

  Color bb_color[4];
  value_to_color(i_min / res, 0, BB_RESOLUTION, bb_color);
  value_to_color(k_min / res, 0, BB_RESOLUTION, bb_color + 1);
  value_to_color(i_max / res, 0, BB_RESOLUTION, bb_color + 2);
  value_to_color(k_max / res, 0, BB_RESOLUTION, bb_color + 3);

  const size_t info_start = side * PIXELS_PER_BLOCK * 2;
  const size_t info_height = image->height - info_start;
  row_ptr = image->pixels + info_start * scan;
  for (size_t r = 0; r < info_height; ++r, row_ptr += scan) {
    Color *x0_ptr = row_ptr;
    Color *z0_ptr = x0_ptr + wide;
    Color *x1_ptr = z0_ptr + wide;
    Color *z1_ptr = x1_ptr + wide;
    for (size_t c = 0; c < wide; ++c, ++x0_ptr, ++z0_ptr, ++x1_ptr, ++z1_ptr) {
      *x0_ptr = bb_color[0];
      *z0_ptr = bb_color[1];
      *x1_ptr = bb_color[2];
      *z1_ptr = bb_color[3];
    }
  }

  return VPIC_OK;
}

size_t get_depth(const voxelpicImage *image) {
  if (image->width == 480) {
    return 7;
  }

  if (image->width == 960) {
    return 8;
  }

  if (image->width == 1920) {
    return 9;
  }

  return VPIC_INVALID_LEVEL;
}

void decode_bounding_box(const voxelpicImage *image, size_t side, size_t wide,
                         uint_least32_t *i_min, uint_least32_t *k_min,
                         uint_least32_t *i_max, uint_least32_t *k_max) {
  const size_t info_start = 2 * side * PIXELS_PER_BLOCK;
  const size_t info_height = image->height - info_start;
  const size_t scan = image->width;
  const uint_least32_t count = (uint_least32_t)(info_height * wide);
  const uint_least32_t res = (uint_least32_t)(side / BB_RESOLUTION);

  uint_least32_t i0, i1, k0, k1;
  i0 = i1 = k0 = k1 = 0;
  const Color *row_ptr = image->pixels + info_start * scan;
  for (size_t r = 0; r < info_height; ++r, row_ptr += scan) {
    const Color *i0_ptr = row_ptr;
    const Color *k0_ptr = i0_ptr + wide;
    const Color *i1_ptr = k0_ptr + wide;
    const Color *k1_ptr = i1_ptr + wide;
    for (size_t c = 0; c < wide; ++c, ++i0_ptr, ++k0_ptr, ++i1_ptr, ++k1_ptr) {
      uint_least32_t value;
      color_to_value(*i0_ptr, 0, BB_RESOLUTION, &value);
      i0 += value;
      color_to_value(*k0_ptr, 0, BB_RESOLUTION, &value);
      k0 += value;
      color_to_value(*i1_ptr, 0, BB_RESOLUTION, &value);
      i1 += value;
      color_to_value(*k1_ptr, 0, BB_RESOLUTION, &value);
      k1 += value;
    }
  }

  *i_min = (i0 / count) * res;
  *i_max = (i1 / count) * res;
  *k_min = (k0 / count) * res;
  *k_max = (k1 / count) * res;
}

voxelpicLevel *voxelpicLevelNew(size_t capacity) {
  Level *level = (Level *)malloc(sizeof(Level));
  if (level == NULL) {
    return NULL;
  }

  level->size = 0;
  level->capacity = capacity;
  level->voxels = NULL;
  level->depth = 0;
  level->side = 1;
  level->by_ref = false;
  if (capacity == 0) {
    return level;
  }

  level->voxels = (Voxel *)malloc(sizeof(Voxel) * capacity);
  if (level->voxels == NULL) {
    free(level);
    return NULL;
  }

  return (voxelpicLevel *)level;
}

void voxelpicLevelFree(voxelpicLevel *level_opaque) {
  Level *level = (Level *)level_opaque;
  if (level->by_ref) {
    printf("Attempt to free OcTree owned level object");
    return;
  }

  free(level->voxels);
  free(level);
}

static inline voxelpicEnum level_append(Level *level, uint_least16_t i,
                                        uint_least16_t j, uint_least16_t k,
                                        Color color) {
  voxelpicEnum ret = VPIC_OK;
  if (level->size == level->capacity) {
    ret = level_grow(level, level->capacity << 1);
    if (ret) {
      return ret;
    }
  }

  Voxel *voxel_ptr = level->voxels + level->size;
  voxel_ptr->pos[0] = i;
  voxel_ptr->pos[1] = j;
  voxel_ptr->pos[2] = k;
  voxel_ptr->pos[3] = 0;
  const float inv_side = 1.0f / (float)level->side;
  voxel_ptr->center.x = (float)(2 * i + 1) * inv_side - 1.0f;
  voxel_ptr->center.y = (float)(2 * j + 1) * inv_side - 1.0f;
  voxel_ptr->center.z = (float)(2 * k + 1) * inv_side - 1.0f;
  voxel_ptr->center.w = 1;
  voxel_ptr->color.r = color.r;
  voxel_ptr->color.g = color.g;
  voxel_ptr->color.b = color.b;
  voxel_ptr->color.a = color.a;
  level->size += 1;

  return ret;
}

voxelpicEnum voxelpicLevelDecode(const voxelpicImage *image,
                                 voxelpicLevel *level_opaque) {
  voxelpicEnum ret = VPIC_OK;
  Level *level = (Level *)level_opaque;
  const size_t depth = get_depth(image);
  if (depth == VPIC_INVALID_LEVEL) {
    return (voxelpicEnum)depth;
  }

  level->depth = depth;
  const size_t side = level->side = 1 << depth;
  const size_t wide = image->width / 4;
  const size_t scan = image->width;

  level->size = 0;
  if (level->capacity == 0) {
    level_grow(level, 1024 << depth);
  }

  uint_least32_t i_min_out, i_max, k_min_out, k_max;
  decode_bounding_box(image, side, wide, &i_min_out, &k_min_out, &i_max,
                      &k_max);
  const uint_least32_t i_min = i_min_out;
  const uint_least32_t k_min = k_min_out;
  const uint_least32_t i_range = i_max - i_min;
  const uint_least32_t k_range = k_max - k_min;

  uint_least32_t x_left_out, z_left_out;
  center(i_min, i_range, wide, side, &z_left_out);
  center(k_min, k_range, wide, side, &x_left_out);

  const uint_least32_t x_left = x_left_out;
  const uint_least32_t z_left = z_left_out;

  const Color *depth_ptr = image->pixels;
  const Color *color_ptr = image->pixels + side * scan;
  for (size_t r = 0; r < side; ++r, depth_ptr += scan, color_ptr += scan) {
    const Color *x_depth0_ptr = depth_ptr;
    const Color *x_color0_ptr = color_ptr;
    const Color *z_depth0_ptr = x_depth0_ptr + wide;
    const Color *z_color0_ptr = x_color0_ptr + wide;
    const Color *x_depth1_ptr = z_depth0_ptr + wide;
    const Color *x_color1_ptr = z_color0_ptr + wide;
    const Color *z_depth1_ptr = x_depth1_ptr + wide;
    const Color *z_color1_ptr = x_color1_ptr + wide;
    const voxelpicInt j = (voxelpicInt)(side - r - 1);
    for (uint_least16_t c = 0; c < wide; ++c, ++x_depth0_ptr, ++x_color0_ptr,
                        ++z_depth0_ptr, ++z_color0_ptr, ++x_depth1_ptr,
                        ++x_color1_ptr, ++z_depth1_ptr, ++z_color1_ptr) {
      voxelpicInt i0, k0, i1, k1;
      color_to_value(*x_depth0_ptr, i_min, i_range, &i0);
      color_to_value(*z_depth0_ptr, k_min, k_range, &k0);
      color_to_value(*x_depth1_ptr, i_min, i_range, &i1);
      color_to_value(*z_depth1_ptr, k_min, k_range, &k1);

      if (i0 != EMPTY) {
        ret = level_append(level, i0, j, c + x_left, *x_color0_ptr);
        if (ret) {
          return ret;
        }
      }

      if (k0 != EMPTY) {
        ret = level_append(level, c + z_left, j, k0, *z_color0_ptr);
        if (ret) {
          return ret;
        }
      }

      if (i1 != EMPTY) {
        ret = level_append(level, i1, j, c + x_left, *x_color1_ptr);
        if (ret) {
          return ret;
        }
      }

      if (k1 != EMPTY) {
        ret = level_append(level, c + z_left, j, k1, *z_color1_ptr);
        if (ret) {
          return ret;
        }
      }
    }
  }

  qsort(level->voxels, level->size, sizeof(Voxel), compare_voxels);

  Voxel *write_ptr = level->voxels;
  Voxel *read_ptr = level->voxels + 1;
  for (size_t i = 0; i < level->size; ++i, ++read_ptr) {
    if (read_ptr->index == write_ptr->index) {
      continue;
    }

    ++write_ptr;
    *write_ptr = *read_ptr;
  }

  level->size = write_ptr - level->voxels;
  return ret;
}

static int read_be(int_least32_t *value, FILE *fp) {
  uint8_t bytes[4];
  if (fread(bytes, sizeof(bytes[0]), 4, fp) < 4) {
    return -1;
  }

  *value = (int_least32_t)((bytes[0] << 24) | (bytes[1] << 16) |
                           (bytes[2] << 8) | (bytes[3]));
  return 1;
}

static int write_be(int_least32_t value, FILE *fp) {
  uint8_t bytes[4];
  bytes[0] = (uint8_t)((value >> 24) & 0xFF);
  bytes[1] = (uint8_t)((value >> 16) & 0xFF);
  bytes[2] = (uint8_t)((value >> 8) & 0xFF);
  bytes[3] = (uint8_t)(value & 0xFF);

  if (fwrite(bytes, sizeof(bytes[0]), 4, fp) < 4) {
    return -1;
  }

  return 1;
}

voxelpicEnum voxelpicLevelSave(const voxelpicLevel *level_opaque,
                               const char *path) {
  if (level_opaque == NULL) {
    return VPIC_BAD_POINTER;
  }

  voxelpicEnum rc = VPIC_OK;

  const Level *level = (const Level *)level_opaque;
  FILE *fp = fopen(path, "wb");
  if (fp == NULL) {
    perror("Error opening level file for writing");
    return VPIC_IO_ERROR;
  }

  int_least32_t value = (int_least32_t)level->depth;

  if (write_be(value, fp) < 1) {
    goto file_error;
  }

  value = (int_least32_t)level->size;

  if (write_be(value, fp) < 1) {
    goto file_error;
  }

  const Voxel *voxel_ptr = level->voxels;
  for (size_t i = 0; i < level->size; ++i, ++voxel_ptr) {
    if (fwrite(&voxel_ptr->pos, sizeof(voxel_ptr->pos[0]), 3, fp) < 3) {
      goto file_error;
    }

    if (fputc((uint_least8_t)voxel_ptr->color.r, fp) == EOF) {
      goto file_error;
    }

    if (fputc((uint_least8_t)voxel_ptr->color.g, fp) == EOF) {
      goto file_error;
    }

    if (fputc((uint_least8_t)voxel_ptr->color.b, fp) == EOF) {
      goto file_error;
    }
  }

  goto cleanup;

file_error:
  perror("Error writing to level file");
  rc = VPIC_IO_ERROR;

cleanup:
  fclose(fp);
  return rc;
}

voxelpicEnum voxelpicLevelLoad(const char *path, voxelpicLevel *level_opaque) {
  FILE *fp = fopen(path, "rb");
  if (fp == NULL) {
    perror("Error opening level file for reading");
    return VPIC_IO_ERROR;
  }

  voxelpicEnum rc = VPIC_OK;

  if (level_opaque == NULL) {
    return VPIC_BAD_POINTER;
  }

  Level *level = (Level *)level_opaque;

  int_least32_t value;

  if (read_be(&value, fp) < 1) {
    goto file_error;
  }

  level->depth = (size_t)value;
  level->side = 1 << level->depth;

  if (read_be(&value, fp) < 1) {
    goto file_error;
  }

  size_t size = (size_t)value;

  if (size > level->capacity) {
    level_grow(level, size);
  }

  level->size = 0;
  for (size_t i = 0; i < size; ++i) {
    uint_least16_t pos[3];
    Color color = {{0, 0, 0, 255}};

    if (fread(pos, sizeof(pos[0]), 3, fp) < 3) {
      goto file_error;
    }

    if (fread(&color, sizeof(color.r), 3, fp) < 3) {
      goto file_error;
    }

    level_append(level, pos[0], pos[1], pos[2], color);
  }

  goto cleanup;

file_error:
  perror("Error reading from level file");
  rc = VPIC_IO_ERROR;

cleanup:

  fclose(fp);
  return rc;
}

voxelpicEnum voxelpicPointCloudSave(const voxelpicPointCloud *cloud,
                                    const char *path) {
  if (cloud == NULL) {
    return VPIC_BAD_POINTER;
  }

  voxelpicEnum rc = VPIC_OK;

  FILE *fp = fopen(path, "wb");
  if (fp == NULL) {
    perror("Error opening cloud file for writing");
    return VPIC_IO_ERROR;
  }

  int value = (int_least32_t)cloud->size;

  if (write_be(value, fp) < 1) {
    goto file_error;
  }

  const voxelpicVec4 *pos_ptr = cloud->positions;
  const Color *clr_ptr = cloud->colors;
  for (size_t i = 0; i < cloud->size; ++i, ++pos_ptr, ++clr_ptr) {
    if (fwrite(pos_ptr, sizeof(pos_ptr->x), 3, fp) < 3) {
      goto file_error;
    }

    if (fputc(clr_ptr->r, fp) == EOF) {
      goto file_error;
    }

    if (fputc(clr_ptr->g, fp) == EOF) {
      goto file_error;
    }

    if (fputc(clr_ptr->b, fp) == EOF) {
      goto file_error;
    }
  }

  goto cleanup;

file_error:
  perror("Error writing to cloud file");
  rc = VPIC_IO_ERROR;

cleanup:
  fclose(fp);
  return rc;
}

voxelpicEnum voxelpicPointCloudLoad(const char *path,
                                    voxelpicPointCloud *cloud) {
  FILE *fp = fopen(path, "rb");
  if (fp == NULL) {
    perror("Error opening cloud file for reading");
    return VPIC_IO_ERROR;
  }

  voxelpicEnum rc = VPIC_OK;

  if (cloud == NULL) {
    return VPIC_BAD_POINTER;
  }

  int_least32_t value;
  if (read_be(&value, fp) < 1) {
    goto file_error;
  }

  size_t size = (size_t)value;

  if (size > cloud->capacity) {
    voxelpicEnum err = cloud_grow(cloud, size);
    if (err) {
      return err;
    }
  }

  cloud->size = size;
  Vec *pos_ptr = cloud->positions;
  Color *clr_ptr = cloud->colors;
  for (size_t i = 0; i < size; ++i, ++pos_ptr, ++clr_ptr) {
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
  perror("Error reading from cloud file");
  rc = VPIC_IO_ERROR;

cleanup:
  fclose(fp);
  return rc;
}

voxelpicEnum voxelpicDepthImageSize(size_t depth, size_t *width,
                                    size_t *height) {
  if (depth < 6) {
    return VPIC_INVALID_LEVEL;
  }

  size_t scale = (size_t)1 << (depth - 6);
  *height = ((135 * PIXELS_PER_BLOCK) * scale);
  *width = (16 * *height) / 9;
  return VPIC_OK;
}

float voxelpicDepthVoxelSize(size_t depth) {
  return 2.0f / (float)((size_t)1 << depth);
}