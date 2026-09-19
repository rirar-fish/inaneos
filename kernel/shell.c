#include "shell.h"
#include "keyboard.h"
#include "vga.h"

// tiny shell: ls, cd, mkdir only
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

static int slen(const char *s) {
  int n = 0;
  while (s[n])
    n++;
  return n;
}

static const char *skip_sp(const char *s) {
  while (*s == ' ' || *s == '\t')
    s++;
  return s;
}

static void trim_end(char *s) {
  int n = slen(s);
  while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t')) {
    s[n - 1] = '\0';
    n--;
  }
}

#define FS_MAX 64
#define NAME_MAX 32
// FIXME: ram only, no disk

static struct {
  char name[NAME_MAX];
  int parent;
  int is_dir;
  int used;
  int mode;
} nodes[FS_MAX];

static int cwd;

static void name_cp(char *d, const char *s) {
  int i = 0;
  while (s[i] && i < NAME_MAX - 1) {
    d[i] = s[i];
    i++;
  }
  d[i] = '\0';
}

static int find_kid(int dir, const char *name) {
  for (int i = 0; i < FS_MAX; i++) {
    if (!nodes[i].used || nodes[i].parent != dir)
      continue;
    if (same_condition(nodes[i].name, name))
      return i;
  }
  return -1;
}

static int next_part(const char *p, char *out) {
  int i = 0;
  while (*p == '/')
    p++;
  while (p[i] && p[i] != '/' && i < NAME_MAX - 1) {
    out[i] = p[i];
    i++;
  }
  out[i] = '\0';
  return i;
}

// resolve path with ., ..
static int resolve(const char *path, int *out) {
  int cur = (*path == '/') ? 0 : cwd;
  const char *p = path;
  char part[NAME_MAX];

  if (*p == '\0' || same_condition(p, ".")) {
    *out = cur;
    return 0;
  }

  while (*p) {
    while (*p == '/')
      p++;
    if (*p == '\0')
      break;
    int n = next_part(p, part);
    p += n;
    while (*p == '/')
      p++;
    if (same_condition(part, "."))
      continue;
    if (same_condition(part, "..")) {
      if (nodes[cur].parent >= 0)
        cur = nodes[cur].parent;
      continue;
    }
    int k = find_kid(cur, part);
    if (k < 0)
      return -1;
    if (*p && !nodes[k].is_dir)
      return -1;
    cur = k;
  }
  *out = cur;
  return 0;
}

static int resolve_dir(const char *path, int *out) {
  int d;
  if (resolve(path, &d) != 0 || !nodes[d].is_dir)
    return -1;
  *out = d;
  return 0;
}

static int free_slot(void) {
  for (int i = 0; i < FS_MAX; i++)
    if (!nodes[i].used)
      return i;
  return -1;
}

static void print_path(int idx) {
  if (idx != 0) {
    print_path(nodes[idx].parent);
    term_puts("/");
    term_puts(nodes[idx].name);
  } else {
    term_puts("");
  }
}

static void cmd_ls(char *arg) {
  arg = (char *)skip_sp(arg);
  trim_end(arg);
  int dir = cwd;
  if (*arg && resolve_dir(arg, &dir) != 0) {
    term_puts("bad path\n");
    return;
  }
  for (int i = 0; i < FS_MAX; i++) {
    if (!nodes[i].used || nodes[i].parent != dir)
      continue;
    term_puts(nodes[i].name);
    if (nodes[i].is_dir)
      term_puts("/");
    term_puts("\n");
  }
}

static void cmd_cd(char *arg) {
  arg = (char *)skip_sp(arg);
  trim_end(arg);
  int dir;
  if (*arg == '\0') {
    cwd = 0;
    return;
  }
  if (resolve_dir(arg, &dir) != 0) {
    term_puts("bad path\n");
    return;
  }
  cwd = dir;
}

static void cmd_mkdir(char *arg) {
  char leaf[NAME_MAX];
  int pdir;
  arg = (char *)skip_sp(arg);
  trim_end(arg);
  if (*arg == '\0') {
    term_puts("usage: mkdir name\n");
    return;
  }
  if (slen(arg) >= 128) {
    term_puts("too long\n");
    return;
  }
  int last = -1;
  for (int i = 0; arg[i]; i++)
    if (arg[i] == '/')
      last = i;
  if (last >= 0) {
    char pbuf[128];
    int i = 0;
    while (i < last && i < 127) {
      pbuf[i] = arg[i];
      i++;
    }
    pbuf[i] = '\0';
    if (resolve_dir(pbuf[0] ? pbuf : "/", &pdir) != 0) {
      term_puts("bad path\n");
      return;
    }
    next_part(arg + last + 1, leaf);
  } else {
    pdir = cwd;
    next_part(arg, leaf);
  }
  if (leaf[0] == '\0' || same_condition(leaf, ".") ||
      same_condition(leaf, "..")) {
    term_puts("bad name\n");
    return;
  }
  if (find_kid(pdir, leaf) >= 0) {
    term_puts("exists\n");
    return;
  }
  int fr = free_slot();
  if (fr < 0) {
    term_puts("full\n");
    return;
  }
  name_cp(nodes[fr].name, leaf);
  nodes[fr].parent = pdir;
  nodes[fr].is_dir = 1;
  nodes[fr].used = 1;
  nodes[fr].mode = 0755;
}

static void fs_init(void) {
  for (int i = 0; i < FS_MAX; i++)
    nodes[i].used = 0;
  nodes[0].name[0] = '\0';
  nodes[0].parent = -1;
  nodes[0].is_dir = 1;
  nodes[0].used = 1;
  nodes[0].mode = 0755;
  cwd = 0;
}

static void running(char *line) {
  if (line[0] == '\0')
    return;

  if (same_condition(line, "ls")) {
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
  } else {
    term_puts(line);
    term_puts(": Command Not Found\n");
  }
  // TODO: add more cmds here
}

void shell_run(void) {
  char line[128];
  fs_init();

  // main loop: prompt, read, exec
  for (;;) {
    term_set_color(0x0A, 0x00);
    term_puts("root@inaneos:");
    if (cwd == 0)
      term_puts("/");
    else
      print_path(cwd);
    term_puts("# ");
    term_set_color(0x0F, 0x00);

    int i = 0;
    for (;;) {
      char c = (char)getchar();
      if (c == '\n')
        break;
      if (c == '\b') {
        if (i > 0) {
          i--;
          term_putc('\b');
        }
      } else if (c >= 32 && c < 127 && i < (int)sizeof(line) - 1) {
        line[i++] = c;
        term_putc(c);
      }
    }
    term_putc('\n');
    line[i] = '\0';
    running(line);
  }
}
