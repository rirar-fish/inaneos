#pragma once
#include <stdint.h>

// fat32 read-only
int fat_mount(uint64_t part_lba); // 0 ok
void fat_unmount(void);
int fat_mounted(void);
int fat_list(const char *path, char *out, unsigned long cap);
int fat_chdir(const char *path);
int fat_getcwd(char *out, unsigned long cap);
long fat_read(const char *path, char *out, unsigned long cap);
int fat_write(const char *path, const char *buf, unsigned long len);
int fat_mkdir(const char *path);
