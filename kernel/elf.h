#pragma once
#include <stdint.h>

// load user elf, returns entry
int elf_load(const uint8_t *img, unsigned long len, uint64_t *entry);
