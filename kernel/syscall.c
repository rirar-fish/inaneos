// syscall dispatch
#include "syscall.h"
#include "exec.h"
#include "fat.h"
#include "fs.h"
#include "part.h"
#include "io.h"
#include "keyboard.h"
#include "msr.h"
#include "pmm.h"
#include "vga.h"

#define MSR_EFER 0xC0000080
#define MSR_STAR 0xC0000081
#define MSR_LSTAR 0xC0000082
#define MSR_FMASK 0xC0000084

extern void syscall_entry(void);

// bounded user string copy
static long copy_str(char *dst, const char *src, unsigned long cap) {
  unsigned long n = 0;
  if (!src || cap == 0)
    return E_INVAL;
  while (n + 1 < cap && src[n]) {
    dst[n] = src[n];
    n++;
  }
  if (src[n]) // no NUL in cap
    return E_INVAL;
  dst[n] = '\0';
  return (long)n;
}

// emit helpers, -1 on overflow
static unsigned long emit_str(char *buf, unsigned long pos, unsigned long cap,
                              const char *s) {
  while (*s) {
    if (pos >= cap)
      return (unsigned long)-1;
    buf[pos++] = *s++;
  }
  return pos;
}

static unsigned long emit_dec(char *buf, unsigned long pos, unsigned long cap,
                              unsigned long v) {
  char tmp[20];
  int i = 0;
  if (!v)
    tmp[i++] = '0';
  while (v) {
    tmp[i++] = '0' + (v % 10);
    v /= 10;
  }
  while (i--) {
    if (pos >= cap)
      return (unsigned long)-1;
    buf[pos++] = tmp[i];
  }
  return pos;
}

static unsigned long emit_hex(char *buf, unsigned long pos, unsigned long cap,
                              unsigned int v) {
  const char *h = "0123456789ABCDEF";
  if (pos + 2 > cap)
    return (unsigned long)-1;
  buf[pos++] = h[(v >> 4) & 0xF];
  buf[pos++] = h[v & 0xF];
  return pos;
}

void syscall_dispatcher(syscall_frame *f) {
  switch (f->rax) {
  case SYS_PUTS: {
    const char *s = (const char *)(uintptr_t)f->rdi;
    uint64_t n = 0;
    if (!s) {
      f->rax = (uint64_t)E_INVAL;
      break;
    }
    while (n < SYS_IO_MAX && s[n])
      n++;
    if (n == SYS_IO_MAX) {
      f->rax = (uint64_t)E_INVAL;
      break;
    }
    term_write(s, n);
    f->rax = 0;
    break;
  }
  case SYS_WRITE: {
    const char *s = (const char *)(uintptr_t)f->rdi;
    uint64_t n = f->rsi;
    if (!s || n > SYS_IO_MAX) {
      f->rax = (uint64_t)E_INVAL;
      break;
    }
    term_write(s, n);
    f->rax = n;
    break;
  }
  case SYS_GETC: {
    // entry masks IF, unblock irq while waiting
    int c;
    __asm__ volatile("sti");
    c = getchar();
    __asm__ volatile("cli");
    f->rax = (uint64_t)c;
    break;
  }
  case SYS_INFO:
    term_puts("inaneos beta 0.0.1\n");
    f->rax = 0;
    break;
  case SYS_REBOOT:
    cpu_reboot();
    break;
  case SYS_POWEROFF:
    cpu_poweroff();
    break;
  case SYS_EXIT:
    if (enter_program(0, 0) != 0) {
      term_puts("init lost\n");
      __asm__ volatile("cli");
      for (;;)
        __asm__ volatile("hlt");
    }
    break;
  case SYS_RUN: {
    char name[32];
    char arg[128];
    const char *ap = 0;
    long r = copy_str(name, (const char *)(uintptr_t)f->rdi, sizeof(name));
    int idx;
    if (r < 0) {
      f->rax = (uint64_t)E_INVAL;
      break;
    }
    if (f->rsi) {
      r = copy_str(arg, (const char *)(uintptr_t)f->rsi, sizeof(arg));
      if (r < 0) {
        f->rax = (uint64_t)E_INVAL;
        break;
      }
      ap = arg;
    }
    idx = exec_find(name);
    if (idx < 0) {
      f->rax = (uint64_t)E_INVAL;
      break;
    }
    if (enter_program(idx, ap) != 0 && enter_program(0, 0) != 0) {
      term_puts("init lost\n");
      __asm__ volatile("cli");
      for (;;)
        __asm__ volatile("hlt");
    }
    break;
  }
  case SYS_LSMOD: {
    char *buf = (char *)(uintptr_t)f->rdi;
    unsigned long cap = f->rsi;
    if (!buf || cap > 256) {
      f->rax = (uint64_t)E_INVAL;
      break;
    }
    f->rax = (uint64_t)exec_lsmod(buf, cap);
    break;
  }
  case SYS_PARTLIST: {
    char *buf = (char *)(uintptr_t)f->rdi;
    unsigned long cap = f->rsi;
    unsigned long pos = 0;
    if (!buf || cap > 512) {
      f->rax = (uint64_t)E_INVAL;
      break;
    }
    for (int i = 0; i < part_count(); i++) {
      const part_t *p = part_get(i);
      pos = emit_dec(buf, pos, cap, (unsigned long)i);
      pos = emit_str(buf, pos, cap, (int)i == fs_mounted_idx() ? "* " : " ");
      pos = emit_hex(buf, pos, cap, p->type);
      pos = emit_str(buf, pos, cap, " ");
      pos = emit_dec(buf, pos, cap, p->lba);
      pos = emit_str(buf, pos, cap, " ");
      pos = emit_dec(buf, pos, cap, p->sectors);
      pos = emit_str(buf, pos, cap, "\n");
      if (pos == (unsigned long)-1)
        break;
    }
    f->rax = pos == (unsigned long)-1 ? (uint64_t)E_INVAL : pos;
    break;
  }
  case SYS_MOUNT: {
    long idx = (long)f->rdi;
    if (idx < 0) {
      fs_unmount();
      f->rax = 0;
    } else {
      f->rax = (uint64_t)fs_mount_part((int)idx);
    }
    break;
  }
  case SYS_FREAD: {
    char path[128];
    char *buf = (char *)(uintptr_t)f->rsi;
    unsigned long cap = f->rdx;
    long r = copy_str(path, (const char *)(uintptr_t)f->rdi, sizeof(path));
    if (r < 0 || !buf || cap > 8192) {
      f->rax = (uint64_t)E_INVAL;
      break;
    }
    if (fat_mounted())
      f->rax = (uint64_t)fat_read(path, buf, cap);
    else
      f->rax = (uint64_t)fs_fread_ram(path, buf, cap);
    break;
  }
  case SYS_GOTO:
    term_goto((int)f->rdi, (int)f->rsi);
    f->rax = 0;
    break;
  case SYS_FWRITE: {
    char path[128];
    const char *buf = (const char *)(uintptr_t)f->rsi;
    unsigned long len = f->rdx;
    long r = copy_str(path, (const char *)(uintptr_t)f->rdi, sizeof(path));
    if (r < 0 || !buf || len > FILE_MAX) {
      f->rax = (uint64_t)E_INVAL;
      break;
    }
    f->rax = (uint64_t)fs_fwrite(path, buf, len);
    break;
  }
  case SYS_CHDIR: {
    char path[128];
    long r = copy_str(path, (const char *)(uintptr_t)f->rdi, sizeof(path));
    f->rax = (uint64_t)(r < 0 ? r : fs_chdir(path));
    break;
  }
  case SYS_MKDIR: {
    char path[128];
    long r = copy_str(path, (const char *)(uintptr_t)f->rdi, sizeof(path));
    f->rax = (uint64_t)(r < 0 ? r : fs_mkdir(path));
    break;
  }
  case SYS_LISTDIR: {
    char path[128];
    char *buf = (char *)(uintptr_t)f->rsi;
    unsigned long cap = f->rdx;
    long r = copy_str(path, (const char *)(uintptr_t)f->rdi, sizeof(path));
    if (r < 0 || !buf || cap > 8192) {
      f->rax = (uint64_t)E_INVAL;
      break;
    }
    f->rax = (uint64_t)fs_listdir(path, buf, cap);
    break;
  }
  case SYS_GETCWD: {
    char *buf = (char *)(uintptr_t)f->rdi;
    unsigned long cap = f->rsi;
    if (!buf || cap > 512 || cap < 2) {
      f->rax = (uint64_t)E_INVAL;
      break;
    }
    f->rax = (uint64_t)fs_getcwd(buf, cap);
    break;
  }
  case SYS_SETCOLOR:
    term_set_color((int)f->rdi, (int)f->rsi);
    f->rax = 0;
    break;
  case SYS_MEMINFO: {
    char *buf = (char *)(uintptr_t)f->rdi;
    unsigned long cap = f->rsi;
    if (!buf || cap > 256 || cap < 32) {
      f->rax = (uint64_t)E_INVAL;
      break;
    }
    unsigned long pos = 0;
    pos = emit_str(buf, pos, cap, "free: ");
    pos = emit_dec(buf, pos, cap, pmm_free_kb());
    pos = emit_str(buf, pos, cap, " KB total: ");
    pos = emit_dec(buf, pos, cap, pmm_total_kb());
    pos = emit_str(buf, pos, cap, " KB\n");
    f->rax = pos == (unsigned long)-1 ? (uint64_t)E_INVAL : pos;
    break;
  }
  default:
    f->rax = (uint64_t)E_NOSYS;
    break;
  }
}

// enable syscall
void syscall_init(void) {
  uint64_t efer = rdmsr(MSR_EFER);
  wrmsr(MSR_EFER, efer | 1);
  wrmsr(MSR_LSTAR, (uint64_t)syscall_entry);
  wrmsr(MSR_FMASK, 0x200);
  wrmsr(MSR_STAR, ((uint64_t)0x10 << 48) | ((uint64_t)0x08 << 32));
}

// reset cascade
__attribute__((noreturn)) void cpu_reboot(void) {
  int i = 100;
  while (i-- && (inb(0x64) & 0x02))
    ;
  outb(0x64, 0xFE);
  outb(0xCF9, 0x02);
  io_wait();
  outb(0xCF9, 0x06);
  struct {
    uint16_t limit;
    uint64_t base;
  } __attribute__((packed)) null_idt = {0, 0};
  __asm__ volatile("lidt %0" ::"m"(null_idt));
  __asm__ volatile("int $0x03");
  for (;;)
    __asm__ volatile("hlt");
}

// qemu off, else halt
__attribute__((noreturn)) void cpu_poweroff(void) {
  outw(0x604, 0x2000);
  term_puts("safe to power off\n");
  __asm__ volatile("cli");
  for (;;)
    __asm__ volatile("hlt");
}
