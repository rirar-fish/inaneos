// mbr partitions
#include "part.h"
#include "ide.h"

static part_t parts[PART_MAX];
static int nparts;
static uint8_t sec[512];

static uint32_t rd32(uint8_t *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
         ((uint32_t)p[3] << 24);
}

int part_scan(void) {
  uint8_t *e;
  nparts = 0;
  if (ide_init() != 0)
    return -1;
  if (ide_read(0, sec) != 0)
    return -1;
  if (sec[510] != 0x55 || sec[511] != 0xAA)
    return 0; // no mbr
  for (int i = 0; i < PART_MAX; i++) {
    uint8_t type;
    e = sec + 0x1BE + i * 16;
    type = e[4];
    if (!type) // empty, extended skipped
      continue;
    parts[nparts].type = type;
    parts[nparts].lba = rd32(e + 8);
    parts[nparts].sectors = rd32(e + 12);
    nparts++;
  }
  return nparts;
}

int part_count(void) { return nparts; }

const part_t *part_get(int idx) {
  if (idx < 0 || idx >= nparts)
    return 0;
  return &parts[idx];
}
