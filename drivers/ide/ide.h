#pragma once
#include <stdint.h>

// ata pio primary master
int ide_init(void); // 0 ok
int ide_read(uint64_t lba, void *buf); // 512B, 0 ok
int ide_write(uint64_t lba, const void *buf); // 512B, 0 ok
uint64_t ide_sectors(void);
