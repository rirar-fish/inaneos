#pragma once

// ram fs server
#define FS_OK 0
#define FS_BADPATH -1
#define FS_EXISTS -2
#define FS_FULL -3
#define FS_USAGE -4
#define FS_BADNAME -5
#define FS_RO -6

#define FILE_MAX 4096

void fs_init(void);
int fs_mkdir(const char *path);
int fs_chdir(const char *path);
int fs_listdir(const char *path, char *out, unsigned long cap);
int fs_getcwd(char *out, unsigned long cap);
long fs_fread_ram(const char *path, char *out, unsigned long cap);
int fs_fwrite(const char *path, const char *buf, unsigned long len);
long fs_fread_ram(const char *path, char *out, unsigned long cap);
int fs_fwrite(const char *path, const char *buf, unsigned long len);
int fs_mount_part(int idx); // moon backend
void fs_unmount(void);
int fs_is_mounted(void);
int fs_mounted_idx(void); // -1 if none
