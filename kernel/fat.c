// fat32 reader
#include "fat.h"
#include "ide.h"

#define ATTR_DIR 0x10
#define ATTR_VOL 0x08
#define ATTR_LFN 0x0F
#define EOC 0x0FFFFFF8
#define PATH_MAX 256

static uint8_t sec[512];
static uint64_t fat_start;
static uint64_t data_start;
static uint8_t sec_per_clus;
static uint8_t num_fats;
static uint32_t fat_sz;
static uint32_t root_clus;
static uint32_t cwd_clus;
static char cwd_path[PATH_MAX];
static int is_mounted;
static uint32_t alloc_hint = 2;

static uint16_t rd16(uint8_t *p) { return p[0] | ((uint16_t)p[1] << 8); }
static uint32_t rd32(uint8_t *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
         ((uint32_t)p[3] << 24);
}

static int slen(const char *s) {
  int n = 0;
  while (s[n])
    n++;
  return n;
}

static int name_same(const char *a, const char *b) {
  while (*a && *a == *b) {
    a++;
    b++;
  }
  return *a == *b;
}


static int name_match(const char *disk, const char *in) {
  while (*disk && *in) {
    char c = *in;
    if (c >= 'a' && c <= 'z')
      c -= 32;
    if (*disk != c)
      return 0;
    disk++;
    in++;
  }
  return *disk == *in;
}

// cluster -> lba
static uint64_t clus_lba(uint32_t c) {
  return data_start + (uint64_t)(c - 2) * sec_per_clus;
}

static uint32_t fat_next(uint32_t c) {
  uint64_t off = (uint64_t)c * 4;
  if (ide_read(fat_start + off / 512, sec) != 0)
    return EOC;
  return rd32(sec + off % 512) & 0x0FFFFFFF;
}

// absolute path into buf, 0 ok
static int abspath(const char *path, char *out) {
  int n = 0;
  if (path[0] == '/') {
    out[n++] = '/';
    path++;
  } else {
    for (int i = 0; cwd_path[i] && n + 1 < PATH_MAX; i++)
      out[n++] = cwd_path[i];
    if (n > 1)
      out[n++] = '/';
  }
  while (*path) {
    while (*path == '/')
      path++;
    if (!*path)
      break;
    char comp[13];
    int m = 0;
    while (path[m] && path[m] != '/' && m < 12)
      m++;
    if (m == 0 || (path[m] && path[m] != '/'))
      return -1;
    for (int i = 0; i < m; i++)
      comp[i] = path[i];
    comp[m] = '\0';
    path += m;
    if (name_same(comp, "."))
      continue;
    if (name_same(comp, "..")) {
      while (n > 1 && out[n - 1] != '/')
        n--;
      if (n > 1)
        n--;
      continue;
    }
    if (n > 1)
      out[n++] = '/';
    for (int i = 0; i < m; i++) {
      if (n + 1 >= PATH_MAX)
        return -1;
      out[n++] = comp[i];
    }
  }
  out[n] = '\0';
  return 0;
}

// 8.3 entry name into out
static int entry_name(uint8_t *e, char *out) {
  int n = 0;
  if (e[0] == 0x00 || e[0] == 0xE5)
    return -1;
  if ((e[11] & ATTR_LFN) == ATTR_LFN || (e[11] & ATTR_VOL))
    return -1;
  for (int i = 0; i < 8 && e[i] != ' '; i++)
    out[n++] = e[i];
  if (e[8] != ' ') {
    out[n++] = '.';
    for (int i = 8; i < 11 && e[i] != ' '; i++)
      out[n++] = e[i];
  }
  out[n] = '\0';
  if (name_same(out, ".") || name_same(out, ".."))
    return -1;
  return 0;
}

// walk absolute path
static int walk(const char *abs, uint32_t *clus, int *isdir, uint32_t *size) {
  uint32_t cur = root_clus;
  const char *p = abs;
  char comp[13];
  if (!*p || name_same(p, "/")) {
    *clus = cur;
    *isdir = 1;
    return 0;
  }
  while (*p) {
    int found = 0;
    int m = 0;
    while (*p == '/')
      p++;
    if (!*p)
      break;
    while (p[m] && p[m] != '/' && m < 12)
      m++;
    if (m == 0 || (p[m] && p[m] != '/'))
      return -1;
    for (int i = 0; i < m; i++)
      comp[i] = p[i];
    comp[m] = '\0';
    p += m;
    for (uint32_t c = cur; c < EOC; c = fat_next(c)) {
      for (uint8_t s = 0; s < sec_per_clus; s++) {
        if (ide_read(clus_lba(c) + s, sec) != 0)
          return -1;
        for (int i = 0; i < 16; i++) {
          uint8_t *e = sec + i * 32;
          char nm[13];
          if (e[0] == 0x00)
            return -1;
          if (entry_name(e, nm) != 0)
            continue;
          if (!name_match(nm, comp))
            continue;
          found = 1;
          cur = ((uint32_t)e[20] << 16) | ((uint32_t)e[21] << 24) |
                rd16(e + 26);
          if (e[11] & ATTR_DIR) {
            if (!*p || *p == '/')
              *isdir = 1;
            else
              return -1;
          } else {
            if (*p && *p != '/')
              return -1;
            *isdir = 0;
            if (size)
              *size = rd32(e + 28);
          }
          break;
        }
        if (found)
          break;
      }
      if (found)
        break;
    }
    if (!found)
      return -1;
    if (!*isdir && *p)
      return -1;
  }
  *clus = cur;
  return 0;
}



int fat_mount(uint64_t part_lba) {
  uint16_t bps;
  uint16_t reserved;
  uint8_t fats;
  if (ide_read(part_lba, sec) != 0)
    return -1;
  if (sec[510] != 0x55 || sec[511] != 0xAA)
    return -1;
  bps = rd16(sec + 11);
  sec_per_clus = sec[13];
  reserved = rd16(sec + 14);
  fats = sec[16];
  fat_sz = rd32(sec + 36);
  root_clus = rd32(sec + 44);
  if (bps != 512 || !sec_per_clus || !fats || !fat_sz || root_clus < 2)
    return -1;
  num_fats = fats;
  fat_start = part_lba + reserved;
  data_start = fat_start + (uint64_t)fats * fat_sz;
  cwd_clus = root_clus;
  cwd_path[0] = '/';
  cwd_path[1] = '\0';
  is_mounted = 1;
  return 0;
}

void fat_unmount(void) { is_mounted = 0; }
int fat_mounted(void) { return is_mounted; }

int fat_chdir(const char *path) {
  char abs[PATH_MAX];
  uint32_t c;
  int dir = 0;
  if (*path == '\0')
    path = "/";
  if (abspath(path, abs) != 0)
    return -1;
  if (walk(abs, &c, &dir, 0) != 0 || !dir)
    return -1;
  cwd_clus = c;
  for (int i = 0; abs[i]; i++)
    cwd_path[i] = abs[i];
  cwd_path[slen(abs)] = '\0';
  return 0;
}

int fat_getcwd(char *out, unsigned long cap) {
  int n = slen(cwd_path);
  if ((unsigned long)n + 1 > cap)
    return -1;
  for (int i = 0; i <= n; i++)
    out[i] = cwd_path[i];
  return n;
}

int fat_list(const char *path, char *out, unsigned long cap) {
  char abs[PATH_MAX];
  uint32_t c;
  int dir = 0;
  unsigned long pos = 0;
  if (abspath(path, abs) != 0)
    return -1;
  if (*path) {
    if (walk(abs, &c, &dir, 0) != 0)
      return -1;
  } else {
    c = cwd_clus;
    dir = 1;
  }
  if (!dir)
    return -1;
  for (uint32_t cl = c; cl < EOC; cl = fat_next(cl)) {
    for (uint8_t s = 0; s < sec_per_clus; s++) {
      if (ide_read(clus_lba(cl) + s, sec) != 0)
        return -1;
      for (int i = 0; i < 16; i++) {
        uint8_t *e = sec + i * 32;
        char nm[13];
        int nl;
        if (e[0] == 0x00)
          return (int)pos;
        if (entry_name(e, nm) != 0)
          continue;
        nl = slen(nm);
        if (pos + (unsigned long)nl + 2 > cap)
          return (int)pos;
        for (int j = 0; j < nl; j++)
          out[pos++] = nm[j];
        if (e[11] & ATTR_DIR)
          out[pos++] = '/';
        out[pos++] = '\n';
      }
    }
  }
  return (int)pos;
}


long fat_read(const char *path, char *out, unsigned long cap) {
  char abs[PATH_MAX];
  uint32_t c;
  int dir = 1;
  uint32_t size;
  unsigned long pos = 0;
  if (abspath(path, abs) != 0)
    return -1;
  if (walk(abs, &c, &dir, &size) != 0 || dir)
    return -1;
  if (size < cap)
    cap = size;
  for (uint32_t cl = c; pos < cap; cl = fat_next(cl)) {
    if (cl >= EOC)
      break;
    for (uint8_t s = 0; s < sec_per_clus && pos < cap; s++) {
      unsigned long n = 512;
      if (ide_read(clus_lba(cl) + s, sec) != 0)
        return -1;
      if (pos + n > cap)
        n = cap - pos;
      for (unsigned long i = 0; i < n; i++)
        out[pos++] = (char)sec[i];
    }
  }
  return (long)pos;
}

// write support

static void wr16(uint8_t *p, uint16_t v) {
  p[0] = v & 0xFF;
  p[1] = (v >> 8) & 0xFF;
}

static void wr32(uint8_t *p, uint32_t v) {
  p[0] = v & 0xFF;
  p[1] = (v >> 8) & 0xFF;
  p[2] = (v >> 16) & 0xFF;
  p[3] = (v >> 24) & 0xFF;
}

// set fat entry on all copies
static int fat_set(uint32_t c, uint32_t v) {
  uint64_t off = (uint64_t)c * 4;
  for (uint8_t k = 0; k < num_fats; k++) {
    uint64_t lba = fat_start + (uint64_t)k * fat_sz + off / 512;
    if (ide_read(lba, sec) != 0)
      return -1;
    wr32(sec + off % 512, v);
    if (ide_write(lba, sec) != 0)
      return -1;
  }
  return 0;
}

static uint32_t fat_max(void) { return fat_sz * 128; }

static uint32_t alloc_clus(void) {
  uint32_t max = fat_max();
  for (uint32_t i = 0; i < max; i++) {
    uint32_t c = 2 + (alloc_hint + i) % (max - 2 > 0 ? max - 2 : 1);
    uint64_t off;
    if (c >= max)
      continue;
    off = (uint64_t)c * 4;
    if (ide_read(fat_start + off / 512, sec) != 0)
      return 0;
    if (rd32(sec + off % 512) == 0) {
      if (fat_set(c, 0x0FFFFFFF) != 0)
        return 0;
      alloc_hint = c;
      return c;
    }
  }
  return 0;
}

static void free_chain(uint32_t c) {
  while (c >= 2 && c < EOC) {
    uint64_t off = (uint64_t)c * 4;
    uint32_t nx = EOC;
    if (ide_read(fat_start + off / 512, sec) == 0)
      nx = rd32(sec + off % 512) & 0x0FFFFFFF;
    fat_set(c, 0);
    c = nx;
  }
}

// name to 8.3 upper
static int to83(const char *in, char out[11]) {
  int bi = 0, ei = 0, i = 0;
  for (int k = 0; k < 11; k++)
    out[k] = ' ';
  while (in[i] && in[i] != '.' && bi < 8) {
    char c = in[i];
    if (c >= 'a' && c <= 'z')
      c -= 32;
    if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' ||
          c == '-'))
      return -1;
    out[bi++] = c;
    i++;
  }
  if (in[i] && in[i] != '.')
    return -1; // base too long
  if (in[i] == '.') {
    i++;
    while (in[i] && ei < 3) {
      char c = in[i];
      if (c >= 'a' && c <= 'z')
        c -= 32;
      if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' ||
            c == '-'))
        return -1;
      out[8 + ei++] = c;
      i++;
    }
    if (in[i])
      return -1; // ext too long
  }
  if (!bi)
    return -1;
  return 0;
}

static int entry_match83(uint8_t *e, char name83[11]) {
  for (int i = 0; i < 11; i++)
    if (e[i] != (uint8_t)name83[i])
      return 0;
  return 1;
}

// find entry in dir, 0 found with sector in sec[]
static int lookup(uint32_t dirclus, char name83[11], uint64_t *lba,
                  int *off, int *isdir, uint32_t *clus, uint32_t *size) {
  for (uint32_t c = dirclus; c < EOC; c = fat_next(c)) {
    for (uint8_t s = 0; s < sec_per_clus; s++) {
      uint64_t l = clus_lba(c) + s;
      if (ide_read(l, sec) != 0)
        return -1;
      for (int i = 0; i < 16; i++) {
        uint8_t *e = sec + i * 32;
        if (e[0] == 0x00)
          return -1;
        if (e[0] == 0xE5 || (e[11] & ATTR_LFN) == ATTR_LFN ||
            (e[11] & ATTR_VOL))
          continue;
        if (!entry_match83(e, name83))
          continue;
        *lba = l;
        *off = i * 32;
        *isdir = (e[11] & ATTR_DIR) ? 1 : 0;
        *clus = ((uint32_t)e[20] << 16) | ((uint32_t)e[21] << 24) |
                rd16(e + 26);
        *size = rd32(e + 28);
        return 0;
      }
    }
  }
  return -1;
}

// free slot in dir, sector in sec[], 0 ok (extends dir)
static int dir_slot(uint32_t dirclus, uint64_t *lba, int *off) {
  uint32_t prev = 0;
  for (uint32_t c = dirclus; c < EOC; c = fat_next(c)) {
    prev = c;
    for (uint8_t s = 0; s < sec_per_clus; s++) {
      uint64_t l = clus_lba(c) + s;
      if (ide_read(l, sec) != 0)
        return -1;
      for (int i = 0; i < 16; i++) {
        if (sec[i * 32] == 0x00 || sec[i * 32] == 0xE5) {
          *lba = l;
          *off = i * 32;
          return 0;
        }
      }
    }
  }
  if (!prev)
    return -1;
  uint32_t nc = alloc_clus();
  static uint8_t zero[512];
  if (!nc)
    return -1;
  for (uint8_t s = 0; s < sec_per_clus; s++)
    if (ide_write(clus_lba(nc) + s, zero) != 0)
      return -1;
  if (fat_set(prev, nc) != 0)
    return -1;
  *lba = clus_lba(nc);
  if (ide_read(*lba, sec) != 0)
    return -1;
  *off = 0;
  return 0;
}

static void fill_entry(uint8_t *e, char name83[11], uint8_t attr,
                       uint32_t clus, uint32_t size) {
  for (int i = 0; i < 11; i++)
    e[i] = name83[i];
  e[11] = attr;
  for (int i = 12; i < 20; i++)
    e[i] = 0;
  wr16(e + 20, (clus >> 16) & 0xFFFF);
  wr16(e + 22, 0x0021);
  wr16(e + 24, 0x0021);
  wr16(e + 26, clus & 0xFFFF);
  wr32(e + 28, size);
}

int fat_write(const char *path, const char *buf, unsigned long len) {
  char abs[PATH_MAX];
  char leaf[13];
  char name83[11];
  uint32_t pclus;
  int dir = 0, isdir = 0, off = 0;
  uint64_t lba = 0;
  uint32_t oldclus = 0, newclus = 0, tail = 0;
  uint32_t fsize = 0, fclus = 0;
  int found = 0;
  unsigned long pos = 0;
  int last = -1;
  if (abspath(path, abs) != 0)
    return -1;
  for (int i = 0; abs[i]; i++)
    if (abs[i] == '/')
      last = i;
  if (last < 0) // root itself
    return -1;
  {
    // parent abs without last comp
    char par[PATH_MAX];
    int i = 0;
    while (abs[last + 1 + i] && i < 12) {
      leaf[i] = abs[last + 1 + i];
      i++;
    }
    leaf[i] = '\0';
    if (abs[last + 1 + i])
      return -1; // leaf too long
    for (i = 0; i <= last && i < PATH_MAX - 1; i++)
      par[i] = abs[i];
    par[i] = '\0';
    if (walk(par[0] ? par : "/", &pclus, &dir, 0) != 0 || !dir)
      return -1;
  }
  if (to83(leaf, name83) != 0)
    return -1;
  if (lookup(pclus, name83, &lba, &off, &isdir, &fclus, &fsize) == 0) {
    if (isdir)
      return -1;
    found = 1;
    oldclus = fclus;
  }
  // alloc and write new chain
  if (len) {
    unsigned long left = len;
    while (left) {
      uint32_t c = alloc_clus();
      if (!c) {
        free_chain(newclus);
        return -1;
      }
      if (!newclus)
        newclus = c;
      else if (fat_set(tail, c) != 0) {
        free_chain(newclus);
        return -1;
      }
      tail = c;
      for (uint8_t s = 0; s < sec_per_clus && left; s++) {
        for (int i = 0; i < 512; i++)
          sec[i] = 0;
        for (int i = 0; i < 512 && left; i++, left--)
          sec[i] = buf[pos++];
        if (ide_write(clus_lba(c) + s, sec) != 0) {
          free_chain(newclus);
          return -1;
        }
      }
    }
  }
  // entry slot
  if (found) {
    if (ide_read(lba, sec) != 0) {
      free_chain(newclus);
      return -1;
    }
  } else if (dir_slot(pclus, &lba, &off) != 0) {
    free_chain(newclus);
    return -1;
  }
  fill_entry(sec + off, name83, 0x20, newclus, (uint32_t)len);
  if (ide_write(lba, sec) != 0) {
    free_chain(newclus);
    return -1;
  }
  if (found && oldclus >= 2)
    free_chain(oldclus);
  return 0;
}
