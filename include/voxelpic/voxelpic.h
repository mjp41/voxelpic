#ifndef _VOXELPIC_H_
#define _VOXELPIC_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define VOXELPIC_VERSION "0.1.0"

/// @brief 4D vector type
typedef union voxelpic_vec4_s {
  float values[4];
  struct {
    float x;
    float y;
    float z;
    float w;
  };
} voxelpicVec4;

/// @brief 32-bit color type
typedef union voxelpic_color_s {
  struct {
    uint_least8_t r;
    uint_least8_t g;
    uint_least8_t b;
    uint_least8_t a;
  };
  uint_least32_t value;
} voxelpicColor;

typedef struct voxelpic_pointcloud_s {
  size_t size;
  size_t capacity;
  voxelpicVec4 *positions;
  voxelpicColor *colors;
} voxelpicPointCloud;

/// @brief Image type
typedef struct voxelpic_image_s {
  size_t width;
  size_t height;
  voxelpicColor *pixels;
} voxelpicImage;

/// @brief Opaque octree type
typedef void voxelpicOcTree;

/// @brief Opaque octree level type
typedef void voxelpicLevel;

/// @brief Return codes for VoxelPic functions
typedef uint_least32_t voxelpicEnum;

#define VPIC_OK 0
#define VPIC_ERROR 1001
#define VPIC_INVALID_LEVEL 1002
#define VPIC_BAD_POINTER 1003
#define VPIC_TOO_SMALL 1004
#define VPIC_OUT_OF_MEMORY 1005
#define VPIC_OUT_OF_RANGE 1006
#define VPIC_IO_ERROR 1007
#define VPIC_UNINITIALIZED 1008

#if defined(_WIN32) && defined(VOXELPIC_SHARED)
/// @brief Macro for exporting functions from a DLL on Windows
#define VPIC_API(x) __declspec(dllexport) x __cdecl
#else
/// @brief Macro for exporting functions from a shared library on other
/// platforms
#define VPIC_API(x) x
#endif

#ifdef __cplusplus
extern "C" {
#endif

/// @brief Compare two voxelpicVec4 objects
VPIC_API(int)
voxelpicVec4Compare(voxelpicVec4 *lhs, voxelpicVec4 *rhs);

/// @brief Compare two voxelpicVec4 objects using only the first three values
VPIC_API(int)
voxelpicVec3Compare(voxelpicVec4 *lhs, voxelpicVec4 *rhs);

/// @brief Create a new point cloud with the specified capacity.
/// If capacity is zero, an empty point cloud is created.
/// @param capacity The initial capacity of the point cloud.
/// @note The caller is responsible for freeing the point cloud using
/// voxelpicPointCloudFree().
/// @return Pointer to the new point cloud, or NULL on error.
VPIC_API(voxelpicPointCloud *)
voxelpicPointCloudNew(size_t capacity);

/// @brief Free the memory associated with a point cloud.
/// @param cloud Pointer to the point cloud to free.
VPIC_API(void)
voxelpicPointCloudFree(voxelpicPointCloud *cloud);

/// @brief Save the point cloud to a file.
/// @param cloud Pointer to the point cloud to save.
/// @param path Path to the file to save the point cloud to.
/// @return VPIC_OK on success, or an error code on failure.
VPIC_API(voxelpicEnum)
voxelpicPointCloudSave(const voxelpicPointCloud *cloud, const char *path);

/// @brief Load a point cloud from a file.
/// @param path Path to the file to load the point cloud from.
/// @param cloud Pointer to the point cloud to load the data into.
/// @note If the point cloud already contains data, it will be overwritten.
/// and if the point cloud does not have enough capacity, it will be
/// reallocated.
/// @return VPIC_OK on success, or an error code on failure.
VPIC_API(voxelpicEnum)
voxelpicPointCloudLoad(const char *path, voxelpicPointCloud *cloud);

/// @brief Create a new octree with the specified minimum and maximum depth.
/// @param min_depth The minimum depth of the octree.
/// @param max_depth The maximum depth of the octree.
/// @note The caller is responsible for freeing the octree using
/// voxelpicOcTreeFree(). Memory will be allocated for each level of the octree.
/// The memory required depends on the size of the cloud, not the
/// maximum depth.
/// @return Pointer to the new octree, or NULL on error.
VPIC_API(voxelpicOcTree *)
voxelpicOcTreeNew(size_t min_deptb, size_t max_depth);

/// @brief Build the octree from the given point cloud.
/// @note If the layers in the octree already contain data, this will be
/// overwritten. IF the layers do not have enough capacity, they will be
/// reallocated.
/// @param octree Pointer to the octree to build.
/// @param point_cloud Pointer to the point cloud to build the octree from.
/// @return VPIC_OK on success, or an error code on failure.
VPIC_API(voxelpicEnum)
voxelpicOcTreeBuild(voxelpicOcTree *, const voxelpicPointCloud *point_cloud);

/// @brief Free the memory associated with an octree.
/// @param octree Pointer to the octree to free.
VPIC_API(void)
voxelpicOcTreeFree(voxelpicOcTree *octree);

/// @brief Get the minimum addressable depth of the octree.
/// @param octree Pointer to the octree.
/// @return The minimum depth of the octree.
VPIC_API(size_t)
voxelpicOcTreeMinDepth(const voxelpicOcTree *octree);

/// @brief Get the maximum addressable depth of the octree.
/// @param octree Pointer to the octree.
/// @return The maximum depth of the octree.
VPIC_API(size_t)
voxelpicOcTreeMaxDepth(const voxelpicOcTree *octree);

/// @brief Get a borrowed reference to a level in the octree.
/// @param octree Pointer to the octree.
/// @param index Index of the level to get. If it is less than the minimum depth
/// or greater than the maximum depth, an error will be returned.
/// @param level Pointer to a voxelpicLevel pointer that will be set to point
/// to the requested level.
/// @note The caller should not free the returned level.
/// @return VPIC_OK on success, or an error code on failure.
VPIC_API(voxelpicEnum)
voxelpicOcTreeLevel(const voxelpicOcTree *octree, size_t index,
                    voxelpicLevel **level);

/// @brief Calculate the image size required to encode a level at the given
/// depth.
/// @param depth The depth of the level.
/// @param width Pointer to a size_t that will be set to the required image
/// width.
/// @param height Pointer to a size_t that will be set to the required image
/// height.
/// @return VPIC_OK on success, or an error code on failure.
VPIC_API(voxelpicEnum)
voxelpicDepthImageSize(size_t depth, size_t *width, size_t *height);

/// @brief Calculate the voxel size at the given depth.
/// The value returned is the length of one side of a voxel at the given depth.
/// @param depth The depth of the level.
/// @return The size of a voxel at the given depth.
VPIC_API(float)
voxelpicDepthVoxelSize(size_t depth);

/// @brief Create a new level with the specified capacity.
/// If capacity is zero, an empty level is created.
/// @param capacity The initial capacity of the level.
/// @note The caller is responsible for freeing the level using
/// voxelpicLevelFree().
/// @return Pointer to the new level, or NULL on error.
VPIC_API(voxelpicLevel *)
voxelpicLevelNew(size_t capacity);

/// @brief Free the memory associated with a level.
/// @param level Pointer to the level to free.
/// @note This does not need to be called on levels obtained using the
/// voxelpicOcTreeLevel() function, as those levels are owned by the octree.
VPIC_API(void)
voxelpicLevelFree(voxelpicLevel *level);

/// @brief Get the number of voxels in the level.
/// @param level Pointer to the level.
/// @return The number of voxels in the level.
VPIC_API(size_t)
voxelpicLevelSize(const voxelpicLevel *level);

/// @brief Get the depth of the level.
/// The depth is a value indicating where this level is located in the octree,
/// and thus the resolution of the voxels in the level.
/// @param level Pointer to the level.
/// @return The depth of the level.
VPIC_API(size_t)
voxelpicLevelDepth(const voxelpicLevel *level);

/// @brief Convert the level to a point cloud.
/// Each voxel in the level will be converted to a point located at the voxel's
/// center, with the voxel's color.
/// @param level Pointer to the level to convert.
/// @param cloud Pointer to the point cloud to store the result in.
/// @param truncate If true, only as many points as will fit in the point
/// cloud's capacity will be written. If false, and the point cloud's capacity
/// is less than the number of voxels in the level, an error will be returned.
/// @return VPIC_OK on success, or an error code on failure.
VPIC_API(voxelpicEnum)
voxelpicLevelToCloud(const voxelpicLevel *level, voxelpicPointCloud *cloud,
                     bool truncate);

/// @brief Encode the level into an image.
/// @param level Pointer to the level to encode.
/// @param image Pointer to the image to store the result in. The image must be
/// preallocated to the correct size for the level. Use voxelpicLevelImageSize()
/// to determine the required size.
/// @return VPIC_OK on success, or an error code on failure.
VPIC_API(voxelpicEnum)
voxelpicLevelEncode(const voxelpicLevel *level, voxelpicImage *image);

/// @brief Decode a level from an image.
/// @param image Pointer to the image to decode the level from.
/// @param level Pointer to the level to store the result in. The level must be
/// preallocated. If it is not large enough to hold the decoded voxels, it will
/// be reallocated.
/// @return VPIC_OK on success, or an error code on failure.
VPIC_API(voxelpicEnum)
voxelpicLevelDecode(const voxelpicImage *image, voxelpicLevel *level);

/// @brief Calculate the image size required to encode the given level.
/// @param level Pointer to the level.
/// @param width Pointer to a size_t that will be set to the required image
/// width.
/// @param height Pointer to a size_t that will be set to the required image
/// height.
/// @return VPIC_OK on success, or an error code on failure.
VPIC_API(voxelpicEnum)
voxelpicLevelImageSize(const voxelpicLevel *image, size_t *width,
                       size_t *height);

/// @brief Save the level to a file.
/// @param level Pointer to the level to save.
/// @param path Path to the file to save the level to.
/// @return VPIC_OK on success, or an error code on failure.
VPIC_API(voxelpicEnum)
voxelpicLevelSave(const voxelpicLevel *level, const char *path);

/// @brief Load a level from a file.
/// @param path Path to the file to load the level from.
/// @param level Pointer to the level to store the result in. The level must be
/// preallocated. If it is not large enough to hold the loaded voxels, it will
/// be reallocated.
/// @return VPIC_OK on success, or an error code on failure.
VPIC_API(voxelpicEnum)
voxelpicLevelLoad(const char *path, voxelpicLevel *level);

/// @brief Create a new image.
/// @param width The width of the image.
/// @param height The height of the image.
/// @return Pointer to the newly created image, or NULL on failure.
VPIC_API(voxelpicImage *)
voxelpicImageNew(size_t width, size_t height);

/// @brief Free an image.
/// @param image Pointer to the image to free.
VPIC_API(void)
voxelpicImageFree(voxelpicImage *image);

/// @brief Get a human-readable string for the given error code.
/// @param errorCode The error code to get the string for.
/// @return Pointer to a string describing the error code.
VPIC_API(const char *)
voxelpicError(voxelpicEnum errorCode);

#define VPIC_MAX_ENCODE_VALUE 1273

typedef uint_least32_t voxelpicInt;

/// @brief Convert an array of integer values to colors.
/// A value from 0 to VPIC_MAX_ENCODE_VALUE will be mapped to a color in a
/// manner which is robust to compression artifacts. The scheme followed is
/// a modified version of the hue-codec described here:
/// https://github.com/jdtremaine/hue-codec/
/// @param values Pointer to the array of integer values.
/// @param min Pointer to the minimum value. If *min == *max, the minimum
/// and maximum values will be computed from the values array.
/// @param max Pointer to the maximum value. If *min == *max, the minimum
/// and maximum values will be computed from the values array.
/// @param colors Pointer to the array of colors to store the result in.
/// @param count The number of values/colors in the arrays.
/// @return VPIC_OK on success, or an error code on failure.
VPIC_API(voxelpicEnum)
voxelpicValueToColor(const voxelpicInt *values, voxelpicInt *min,
                     voxelpicInt *max, voxelpicColor *colors, size_t count);

/// @brief Convert an array of colors to integer values.
/// A color encoded using the scheme described in voxelpicValueToColor()
/// will be converted back to an integer value.
/// @param colors Pointer to the array of colors.
/// @param min The minimum value used during encoding.
/// @param max The maximum value used during encoding.
/// @param values Pointer to the array of integer values to store the result in.
/// @param count The number of colors/values in the arrays.
/// @return VPIC_OK on success, or an error code on failure.
VPIC_API(voxelpicEnum)
voxelpicColorToValue(const voxelpicColor *colors, voxelpicInt min,
                     voxelpicInt max, voxelpicInt *values, size_t count);

#ifdef __cplusplus
}
#endif

#endif