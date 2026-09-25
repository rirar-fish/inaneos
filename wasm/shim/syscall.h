#pragma once

// syscall ABI for wasm: same signatures as kernel/syscall.h wrappers,
// implemented by wasm/sys_shim.c against the browser environment.
long sys_puts(const char *s);
long sys_getc(void);
long sys_info(void);
long sys_reboot(void);
long sys_poweroff(void);
long sys_exit(void);
long sys_write(const char *s, unsigned long len);
long sys_chdir(const char *p);
long sys_mkdir(const char *p);
long sys_listdir(const char *p, char *buf, unsigned long cap);
long sys_getcwd(char *buf, unsigned long cap);
long sys_setcolor(long fg, long bg);
long sys_meminfo(char *buf, unsigned long cap);
long sys_run(const char *name, const char *arg);
long sys_lsmod(char *buf, unsigned long cap);
long sys_partlist(char *buf, unsigned long cap);
long sys_mount(long idx);
long sys_fread(const char *p, char *buf, unsigned long cap);
long sys_goto(int row, int col);
long sys_fwrite(const char *p, const char *buf, unsigned long len);
