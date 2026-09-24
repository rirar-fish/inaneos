// ram fs server
#include "fs.h"
#include "fat.h"
#include "part.h"

#define FS_MAX 64
#define NAME_MAX 32
// FIXME: ram only, no disk

static struct {
  char name[NAME_MAX];
  int parent;
  int is_dir;
  int used;
  int mode;
  char data[FILE_MAX];
  unsigned datalen;
} nodes[FS_MAX];

static int cwd;

static int mounted;
static int mounted_idx = -1;

int fs_mount_part(int idx) {
  const part_t *p = part_get(idx);
  if (!p || fat_mount(p->lba) != 0)
    return FS_BADPATH;
  mounted = 1;
  mounted_idx = idx;
  return FS_OK;
}

void fs_unmount(void) {
  mounted = 0;
  mounted_idx = -1;
  fat_unmount();
}

int fs_is_mounted(void) { return mounted; }
int fs_mounted_idx(void) { return mounted_idx; }

static int same_condition(const char *a, const char *b) {
  while (*a && *a == *b) {
    a++;
    b++;
  }
  return *a == *b;
}

static int slen(const char *s) {
  int n = 0;
  while (s[n])
    n++;
  return n;
}

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

// split into parent dir + leaf, 0 ok
static int split_parent(const char *arg, int *pdir, char *leaf) {
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
    if (resolve_dir(pbuf[0] ? pbuf : "/", pdir) != 0)
      return -1;
    next_part(arg + last + 1, leaf);
  } else {
    *pdir = cwd;
    next_part(arg, leaf);
  }
  return 0;
}

void fs_init(void) {
  for (int i = 0; i < FS_MAX; i++)
    nodes[i].used = 0;
  nodes[0].name[0] = '\0';
  nodes[0].parent = -1;
  nodes[0].is_dir = 1;
  nodes[0].used = 1;
  nodes[0].mode = 0755;
  cwd = 0;
}

int fs_mkdir(const char *arg) {
  char leaf[NAME_MAX];
  int pdir;
  if (mounted)
    return FS_RO;
  if (*arg == '\0')
    return FS_USAGE;
  if (split_parent(arg, &pdir, leaf) != 0)
    return FS_BADPATH;
  if (leaf[0] == '\0' || same_condition(leaf, ".") ||
      same_condition(leaf, ".."))
    return FS_BADNAME;
  if (find_kid(pdir, leaf) >= 0)
    return FS_EXISTS;
  int fr = free_slot();
  if (fr < 0)
    return FS_FULL;
  name_cp(nodes[fr].name, leaf);
  nodes[fr].parent = pdir;
  nodes[fr].is_dir = 1;
  nodes[fr].used = 1;
  nodes[fr].mode = 0755;
  nodes[fr].datalen = 0;
  return FS_OK;
}

int fs_fwrite(const char *path, const char *buf, unsigned long len) {
  char leaf[NAME_MAX];
  int pdir, k;
  if (mounted)
    return fat_write(path, buf, len);
  if (*path == '\0')
    return FS_BADPATH;
  if (len > FILE_MAX)
    return FS_FULL;
  if (split_parent(path, &pdir, leaf) != 0)
    return FS_BADPATH;
  if (leaf[0] == '\0' || same_condition(leaf, ".") ||
      same_condition(leaf, ".."))
    return FS_BADNAME;
  k = find_kid(pdir, leaf);
  if (k >= 0) {
    if (nodes[k].is_dir)
      return FS_BADPATH;
  } else {
    k = free_slot();
    if (k < 0)
      return FS_FULL;
    name_cp(nodes[k].name, leaf);
    nodes[k].parent = pdir;
    nodes[k].is_dir = 0;
    nodes[k].used = 1;
    nodes[k].mode = 0644;
  }
  for (unsigned long i = 0; i < len; i++)
    nodes[k].data[i] = buf[i];
  nodes[k].datalen = (unsigned)len;
  return FS_OK;
}

long fs_fread_ram(const char *path, char *out, unsigned long cap) {
  int idx;
  unsigned n;
  if (resolve(path, &idx) != 0 || nodes[idx].is_dir)
    return -1;
  n = nodes[idx].datalen;
  if (n > cap)
    n = cap;
  for (unsigned i = 0; i < n; i++)
    out[i] = nodes[idx].data[i];
  return (long)n;
}

int fs_chdir(const char *path) {
  int dir;
  if (mounted)
    return fat_chdir(path);
  if (*path == '\0') {
    cwd = 0;
    return FS_OK;
  }
  if (resolve_dir(path, &dir) != 0)
    return FS_BADPATH;
  cwd = dir;
  return FS_OK;
}

int fs_listdir(const char *path, char *out, unsigned long cap) {
  int dir = cwd;
  unsigned long pos = 0;
  if (mounted)
    return fat_list(path, out, cap);
  if (*path && resolve_dir(path, &dir) != 0)
    return FS_BADPATH;
  for (int i = 0; i < FS_MAX; i++) {
    int nl;
    if (!nodes[i].used || nodes[i].parent != dir)
      continue;
    nl = slen(nodes[i].name);
    if (pos + (unsigned long)nl + 2 > cap)
      break;
    for (int j = 0; j < nl; j++)
      out[pos++] = nodes[i].name[j];
    if (nodes[i].is_dir)
      out[pos++] = '/';
    out[pos++] = '\n';
  }
  return (int)pos;
}

int fs_getcwd(char *out, unsigned long cap) {
  int stack[FS_MAX];
  int depth = 0;
  int cur = cwd;
  unsigned long pos = 0;
  if (mounted)
    return fat_getcwd(out, cap);
  if (cap < 2)
    return FS_BADPATH;
  if (cur == 0) {
    out[0] = '/';
    out[1] = '\0';
    return 1;
  }
  while (cur != 0 && depth < FS_MAX) {
    stack[depth++] = cur;
    cur = nodes[cur].parent;
  }
  for (int i = depth - 1; i >= 0; i--) {
    int nl = slen(nodes[stack[i]].name);
    if (pos + (unsigned long)nl + 2 > cap)
      return FS_BADPATH;
    out[pos++] = '/';
    for (int j = 0; j < nl; j++)
      out[pos++] = nodes[stack[i]].name[j];
  }
  out[pos] = '\0';
  return (int)pos;
}
