#include "stdio.h"

#include "voxelpic/voxelpic.h"

int range_explicit(voxelpicInt min, voxelpicInt max) {
  voxelpicInt input_values[2048];
  voxelpicColor pixels[2048];
  voxelpicInt output_values[2048];

  voxelpicInt value = min;
  size_t num_values = (size_t)(max - min);
  for (size_t i = 0; i < num_values; ++i, ++value) {
    input_values[i] = value;
  }

  voxelpicEnum ret =
      voxelpicValueToColor(input_values, &min, &max, pixels, num_values);
  if (ret != VPIC_OK) {
    printf("Error converting values to colors\n");
    return 1;
  }

  ret = voxelpicColorToValue(pixels, min, max, output_values, num_values);
  if (ret != VPIC_OK) {
    printf("Error converting colors to values\n");
    return 1;
  }

  value = min;
  for (size_t i = 0; i < num_values; ++i, ++value) {
    if (output_values[i] != value) {
      printf("Invalid encoding: %d != %d\n", output_values[i], value);
      return 1;
    }
  }

  return 0;
}

int range_implicit(voxelpicInt min, voxelpicInt max) {
  voxelpicInt input_values[2048];
  voxelpicColor pixels[2048];
  voxelpicInt output_values[2048];

  size_t num_values = (size_t)(max - min);
  voxelpicInt value = min;
  for (size_t i = 0; i < num_values; ++i, ++value) {
    input_values[i] = value;
  }

  voxelpicInt test_min = 0;
  voxelpicInt test_max = 0;
  voxelpicEnum ret = voxelpicValueToColor(input_values, &test_min, &test_max,
                                          pixels, num_values);
  if (ret != VPIC_OK) {
    printf("Error converting values to colors\n");
    return 1;
  }

  if (test_min != min) {
    printf("Invalid minimum: %d != %d\n", test_min, min);
    return 1;
  }

  if (test_max != max) {
    printf("Invalid maximum: %d != %d\n", test_max, max);
    return 1;
  }

  ret = voxelpicColorToValue(pixels, min, max, output_values, num_values);
  if (ret != VPIC_OK) {
    printf("Error converting colors to values\n");
    return 1;
  }

  value = min;
  for (size_t i = 0; i < num_values; ++i, ++value) {
    if (output_values[i] != value + min) {
      printf("Invalid encoding: %d != %d\n", output_values[i], value);
      return 1;
    }
  }

  return 0;
}

int main() {
  if (range_explicit(0, VPIC_MAX_ENCODE_VALUE + 1)) {
    return 1;
  }

  if (range_implicit(0, VPIC_MAX_ENCODE_VALUE + 1)) {
    return 1;
  }

  for (voxelpicInt min = 0; min < 64; ++min) {
    for (voxelpicInt max = 192; max < 256; ++max) {
      if (range_explicit(min, max)) {
        printf("Error encoding (explicit range) [%d, %d)\n", min, max);
        return 1;
      }

      if (range_explicit(min, max)) {
        printf("Error encoding (implicit range) [%d, %d)\n", min, max);
        return 1;
      }
    }
  }

  return 0;
}