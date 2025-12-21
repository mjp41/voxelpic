#ifndef _TEST_H_
#define _TEST_H_

#include <stdint.h>
#include <stdio.h>

static inline int read_be(int_least32_t *value, FILE *fp) {
  uint8_t bytes[4];
  if (fread(bytes, sizeof(bytes[0]), 4, fp) < 4) {
    return -1;
  }

  *value = (int_least32_t)((bytes[0] << 24) | (bytes[1] << 16) |
                           (bytes[2] << 8) | (bytes[3]));
  return 1;
}

#endif