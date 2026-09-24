// user shell
#include "fs.h"
#include "keyboard.h"
#include "syscall.h"

// tiny shell: help, echo, info, reboot, poweroff, ls, cd, mkdir, sin, mem, run, moon, cat, keo
static int same_condition(const char *a, const char *b) {
  while (*a && *a == *b) {
    a++;
    b++;
  }
  return *a == *b;
}

static int start(const char *s, const char *pre) {
  while (*pre)
    if (*s++ != *pre++)
      return 0;
  return 1;
}

static const char *skip_sp(const char *s) {
  while (*s == ' ' || *s == '\t')
    s++;
  return s;
}

static void trim_end(char *s) {
  int n = 0;
  while (s[n])
    n++;
  while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t')) {
    s[n - 1] = '\0';
    n--;
  }
}

static void put1(char c) { sys_write(&c, 1); }

#define HIST_N 16
static char hist[HIST_N][128];
static int hcount = 0;

static void hist_push(const char *line) {
  int i, h;
  if (!line[0])
    return;
  if (hcount > 0 && same_condition(hist[hcount - 1], line))
    return;
  if (hcount < HIST_N) {
    for (i = 0; line[i]; i++)
      hist[hcount][i] = line[i];
    hist[hcount][i] = '\0';
    hcount++;
    return;
  }
  for (h = 0; h < HIST_N - 1; h++) {
    for (i = 0; hist[h + 1][i]; i++)
      hist[h][i] = hist[h + 1][i];
    hist[h][i] = '\0';
  }
  for (i = 0; line[i]; i++)
    hist[HIST_N - 1][i] = line[i];
  hist[HIST_N - 1][i] = '\0';
}

static int parse_idx(const char *s, long *out) {
  long v = 0;
  int digits = 0;
  while (*s >= '0' && *s <= '9') {
    v = v * 10 + (*s - '0');
    s++;
    digits++;
  }
  if (!digits || *s)
    return -1;
  *out = v;
  return 0;
}

static void cmd_ls(char *arg) {
  static char out[2048];
  long n;
  arg = (char *)skip_sp(arg);
  trim_end(arg);
  n = sys_listdir(arg, out, sizeof(out));
  if (n < 0)
    sys_puts("bad path\n");
  else if (n > 0)
    sys_write(out, (unsigned long)n);
}

static void cmd_cd(char *arg) {
  arg = (char *)skip_sp(arg);
  trim_end(arg);
  if (sys_chdir(arg) != 0)
    sys_puts("bad path\n");
}

static void cmd_mkdir(char *arg) {
  long r;
  arg = (char *)skip_sp(arg);
  trim_end(arg);
  r = sys_mkdir(arg);
  if (r == FS_USAGE)
    sys_puts("usage: mkdir name\n");
  else if (r == FS_BADPATH)
    sys_puts("bad path\n");
  else if (r == FS_BADNAME)
    sys_puts("bad name\n");
  else if (r == FS_EXISTS)
    sys_puts("exists\n");
  else if (r == FS_FULL)
    sys_puts("full\n");
  else if (r == FS_RO)
    sys_puts("read-only\n");
}

static void running(char *line) {
  static char buf[256];
  long n;
  if (line[0] == '\0')
    return;

  if (same_condition(line, "help")) {
    sys_puts("Available Command:\n");
    sys_puts("help - Command List:\n");
    sys_puts("echo - Print the Text:\n");
    sys_puts("info - About:\n");
    sys_puts("reboot - Restart:\n");
    sys_puts("poweroff - Power off:\n");
    sys_puts("ls - List files:\n");
    sys_puts("cd - Change dir:\n");
    sys_puts("mkdir - Make dir:\n");
    sys_puts("sin - Run as superuser:\n");
    sys_puts("mem - Show memory:\n");
    sys_puts("run - Run program:\n");
    sys_puts("moon - Mount disk:\n");
    sys_puts("cat - Show file:\n");
    sys_puts("keo - Edit file:\n");
  } else if (same_condition(line, "echo")) {
    sys_puts("\n");
  } else if (start(line, "echo ")) {
    sys_puts(line + 5);
    sys_puts("\n");
  } else if (same_condition(line, "info")) {
    sys_puts("Inaneos beta 0.0 version\n");
  } else if (same_condition(line, "reboot")) {
    sys_reboot();
    for (;;)
      __asm__ volatile("hlt");
  } else if (same_condition(line, "poweroff") ||
             same_condition(line, "shutdown")) {
    sys_poweroff();
    for (;;)
      __asm__ volatile("hlt");
  } else if (same_condition(line, "ls")) {
    cmd_ls("");
  } else if (start(line, "ls ")) {
    cmd_ls(line + 3);
  } else if (same_condition(line, "cd")) {
    cmd_cd("");
  } else if (start(line, "cd ")) {
    cmd_cd(line + 3);
  } else if (same_condition(line, "mkdir")) {
    cmd_mkdir("");
  } else if (start(line, "mkdir ")) {
    cmd_mkdir(line + 6);
  } else if (same_condition(line, "sin")) {
    sys_puts("usage: sin command\n");
  } else if (start(line, "sin ")) {
    // run as superuser
    running(line + 4);
  } else if (same_condition(line, "mem")) {
    n = sys_meminfo(buf, sizeof(buf));
    if (n > 0)
      sys_write(buf, (unsigned long)n);
  } else if (same_condition(line, "run")) {
    n = sys_lsmod(buf, sizeof(buf));
    if (n > 0)
      sys_write(buf, (unsigned long)n);
  } else if (start(line, "run ")) {
    char *arg = (char *)skip_sp(line + 4);
    trim_end(arg);
    if (sys_run(arg, 0) != 0)
      sys_puts("no such program\n");
  } else if (same_condition(line, "keo")) {
    sys_puts("usage: keo file\n");
  } else if (start(line, "keo ")) {
    char *arg = (char *)skip_sp(line + 4);
    trim_end(arg);
    if (sys_run("keo", arg) != 0)
      sys_puts("no such program\n");
  } else if (same_condition(line, "moon")) {
    n = sys_partlist(buf, sizeof(buf));
    if (n > 0)
      sys_write(buf, (unsigned long)n);
    else
      sys_puts("no disk\n");
  } else if (start(line, "moon ")) {
    char *arg = (char *)skip_sp(line + 5);
    long idx;
    trim_end(arg);
    if (same_condition(arg, "-u")) {
      sys_mount(-1);
    } else if (parse_idx(arg, &idx) != 0 || sys_mount(idx) != 0) {
      sys_puts("cannot moon\n");
    }
  } else if (start(line, "cat ")) {
    static char big[2048];
    char *arg = (char *)skip_sp(line + 4);
    trim_end(arg);
    n = sys_fread(arg, big, sizeof(big));
    if (n < 0)
      sys_puts("cannot read\n");
    else if (n > 0)
      sys_write(big, (unsigned long)n);
  } else {
    sys_puts(line);
    sys_puts(": Command Not Found\n");
  }
  // TODO: add more cmds here
}

void user_main(int argc, char **argv) {
  static char line[128];
  static char cwd[128];

  // main loop: prompt, read, exec
  for (;;) {
    long n;
    sys_setcolor(0x0A, 0x00);
    sys_puts("root@inaneos:");
    n = sys_getcwd(cwd, sizeof(cwd));
    if (n > 0)
      sys_write(cwd, (unsigned long)n);
    sys_puts("# ");
    sys_setcolor(0x0F, 0x00);

    int i = 0;
    int hidx = -1;
    for (;;) {
      long k = sys_getc();
      if (k == '\n')
        break;
      if (k == KEY_UP || k == KEY_DOWN) {
        int nh = hidx;
        if (k == KEY_UP) {
          if (hcount == 0)
            continue;
          nh = (hidx < 0) ? hcount - 1 : (hidx > 0 ? hidx - 1 : hidx);
        } else {
          if (hidx < 0)
            continue;
          nh = (hidx == hcount - 1) ? -1 : hidx + 1;
        }
        while (i > 0) {
          put1('\b');
          i--;
        }
        hidx = nh;
        if (nh >= 0) {
          for (i = 0; hist[nh][i] && i < (int)sizeof(line) - 1; i++) {
            line[i] = hist[nh][i];
            put1(line[i]);
          }
        }
        continue;
      }
      hidx = -1;
      if (k == '\b') {
        if (i > 0) {
          i--;
          put1('\b');
        }
      } else if (k >= 32 && k < 127 && i < (int)sizeof(line) - 1) {
        line[i++] = (char)k;
        put1((char)k);
      }
    }
    put1('\n');
    line[i] = '\0';
    hist_push(line);
    running(line);
  }
}
