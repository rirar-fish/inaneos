#pragma once
#include <stdint.h>

// mbr primary partitions
#define PART_MAX 4

typedef struct {
  uint8_t type;
  uint64_t lba;
  uint64_t sectors;
} part_t;

int part_scan(void); // count or -1
int part_count(void);
const part_t *part_get(int idx);
