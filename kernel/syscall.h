#pragma once
#include <stdint.h>

// syscall numbers
#define SYS_PUTS 0
#define SYS_GETC 1
#define SYS_INFO 2
#define SYS_REBOOT 3
#define SYS_POWEROFF 4
#define SYS_EXIT 5
#define SYS_WRITE 6
#define SYS_CHDIR 7
#define SYS_MKDIR 8
#define SYS_LISTDIR 9
#define SYS_GETCWD 10
#define SYS_SETCOLOR 11
#define SYS_MEMINFO 12
#define SYS_RUN 13
#define SYS_LSMOD 14
#define SYS_PARTLIST 15
#define SYS_MOUNT 16
#define SYS_FREAD 17
#define SYS_GOTO 18
#define SYS_FWRITE 19

// err codes
#define E_INVAL -22
#define E_NOSYS -38

// max user bytes per call
#define SYS_IO_MAX 4096

// stub returns via sysret, kernel must not use wrappers

// regs from stub
typedef struct {
  uint64_t rax;
  uint64_t rdi;
  uint64_t rsi;
  uint64_t rdx;
  uint64_t r10;
  uint64_t r8;
  uint64_t r9;
  uint64_t rip;
  uint64_t rflags;
  uint64_t rbx;
  uint64_t rbp;
  uint64_t r12;
  uint64_t r13;
  uint64_t r14;
  uint64_t r15;
} syscall_frame;

void syscall_init(void);
void syscall_dispatcher(syscall_frame *f);
__attribute__((noreturn)) void cpu_reboot(void);
__attribute__((noreturn)) void cpu_poweroff(void);

// stub pushes 15 regs
_Static_assert(sizeof(syscall_frame) == 15 * 8, "frame mismatch");

// user wrappers
static inline long sys_call3(long n, long a1, long a2, long a3) {
  long ret;
  __asm__ volatile("syscall"
                   : "=a"(ret)
                   : "a"(n), "D"(a1), "S"(a2), "d"(a3)
                   : "rcx", "r11", "memory");
  return ret;
}

static inline long sys_call2(long n, long a1, long a2) {
  return sys_call3(n, a1, a2, 0);
}

static inline long sys_call1(long n, long a1) {
  return sys_call2(n, a1, 0);
}

static inline long sys_call0(long n) { return sys_call1(n, 0); }

static inline long sys_puts(const char *s) {
  return sys_call1(SYS_PUTS, (long)s);
}

static inline long sys_getc(void) { return sys_call0(SYS_GETC); }
static inline long sys_info(void) { return sys_call0(SYS_INFO); }
static inline long sys_reboot(void) { return sys_call0(SYS_REBOOT); }
static inline long sys_poweroff(void) { return sys_call0(SYS_POWEROFF); }
static inline long sys_exit(void) { return sys_call0(SYS_EXIT); }
static inline long sys_write(const char *s, unsigned long len) {
  return sys_call2(SYS_WRITE, (long)s, (long)len);
}
static inline long sys_chdir(const char *p) {
  return sys_call1(SYS_CHDIR, (long)p);
}
static inline long sys_mkdir(const char *p) {
  return sys_call1(SYS_MKDIR, (long)p);
}
static inline long sys_listdir(const char *p, char *buf, unsigned long cap) {
  return sys_call3(SYS_LISTDIR, (long)p, (long)buf, (long)cap);
}
static inline long sys_getcwd(char *buf, unsigned long cap) {
  return sys_call2(SYS_GETCWD, (long)buf, (long)cap);
}
static inline long sys_setcolor(long fg, long bg) {
  return sys_call2(SYS_SETCOLOR, fg, bg);
}
static inline long sys_meminfo(char *buf, unsigned long cap) {
  return sys_call2(SYS_MEMINFO, (long)buf, (long)cap);
}
static inline long sys_run(const char *name, const char *arg) {
  return sys_call2(SYS_RUN, (long)name, (long)arg);
}
static inline long sys_lsmod(char *buf, unsigned long cap) {
  return sys_call2(SYS_LSMOD, (long)buf, (long)cap);
}
static inline long sys_partlist(char *buf, unsigned long cap) {
  return sys_call2(SYS_PARTLIST, (long)buf, (long)cap);
}
static inline long sys_mount(long idx) {
  return sys_call1(SYS_MOUNT, idx);
}
static inline long sys_fread(const char *p, char *buf, unsigned long cap) {
  return sys_call3(SYS_FREAD, (long)p, (long)buf, (long)cap);
}
static inline long sys_goto(int row, int col) {
  return sys_call2(SYS_GOTO, row, col);
}
static inline long sys_fwrite(const char *p, const char *buf,
                              unsigned long len) {
  return sys_call3(SYS_FWRITE, (long)p, (long)buf, (long)len);
}
