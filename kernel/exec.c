// program loader
#include "exec.h"
#include "elf.h"
#include "pmm.h"
#include "usermode.h"
#include "vmm.h"

#define MAX_MODS 8
#define MOD_NAME_LEN 32
#define USTACK_BASE 0x40000000UL

typedef struct {
  uint32_t mod_start;
  uint32_t mod_end;
  uint32_t cmdline;
  uint32_t reserved;
} mb_mod;

static struct {
  uint32_t start;
  uint32_t end;
  char name[MOD_NAME_LEN];
} mods[MAX_MODS];
static int nmods;

static int name_same(const char *a, const char *b) {
  while (*a && *a == *b) {
    a++;
    b++;
  }
  return *a == *b;
}

void exec_init_mods(uint32_t mods_addr, uint32_t count) {
  mb_mod *m = (mb_mod *)(uintptr_t)mods_addr;
  nmods = 0;
  for (uint32_t i = 0; i < count && nmods < MAX_MODS; i++) {
    const char *cmd = (const char *)(uintptr_t)m[i].cmdline;
    int n = 0;
    if (m[i].mod_end <= m[i].mod_start || m[i].mod_end > 0x40000000)
      continue;
    if (!cmd || !cmd[0]) // unnamed skipped
      continue;
    mods[nmods].start = m[i].mod_start;
    mods[nmods].end = m[i].mod_end;
    while (n + 1 < MOD_NAME_LEN && cmd[n]) {
      mods[nmods].name[n] = cmd[n];
      n++;
    }
    mods[nmods].name[n] = '\0';
    pmm_reserve(m[i].mod_start, m[i].mod_end);
    nmods++;
  }
}

int exec_find(const char *name) {
  for (int i = 0; i < nmods; i++)
    if (name_same(mods[i].name, name))
      return i;
  return -1;
}

int exec_lsmod(char *buf, unsigned long cap) {
  unsigned long pos = 0;
  for (int i = 0; i < nmods; i++) {
    int n = 0;
    while (mods[i].name[n])
      n++;
    if (pos + (unsigned long)n + 1 > cap)
      break;
    for (int j = 0; j < n; j++)
      buf[pos++] = mods[i].name[j];
    buf[pos++] = '\n';
  }
  return (int)pos;
}

int enter_program(int idx, const char *arg) {
  uint64_t entry;
  uint64_t s1, s2;
  uint64_t sp, name_ptr, arg_ptr = 0;
  uint64_t *slots;
  int argc = 1, nlen = 0, alen = 0, i;
  if (idx < 0 || idx >= nmods)
    return -1;
  vmm_unmap_range(USER_MIN, USTACK_TOP);
  if (elf_load((const uint8_t *)(uintptr_t)mods[idx].start,
               mods[idx].end - mods[idx].start, &entry) != 0)
    return -1;
  s1 = pmm_alloc_frame();
  s2 = pmm_alloc_frame();
  if (!s1 || !s2)
    return -1;
  vmm_map(USTACK_BASE, s1, PTE_P | PTE_W | PTE_U);
  vmm_map(USTACK_BASE + 0x1000, s2, PTE_P | PTE_W | PTE_U);
  // argv on fresh stack
  while (mods[idx].name[nlen])
    nlen++;
  if (arg)
    while (arg[alen])
      alen++;
  sp = USTACK_TOP;
  sp -= (uint64_t)((nlen + 8) & ~7);
  name_ptr = sp;
  for (i = 0; i <= nlen; i++)
    ((char *)sp)[i] = mods[idx].name[i];
  if (arg && arg[0]) {
    argc = 2;
    sp -= (uint64_t)((alen + 8) & ~7);
    arg_ptr = sp;
    for (i = 0; i <= alen; i++)
      ((char *)sp)[i] = arg[i];
  }
  sp -= (uint64_t)(argc + 2) * 8;
  sp &= ~15UL;
  slots = (uint64_t *)sp;
  slots[0] = (uint64_t)argc;
  slots[1] = name_ptr;
  slots[2] = arg_ptr;
  if (argc < 2)
    slots[2] = 0;
  slots[1 + argc] = 0;
  jump_usermode(entry, sp);
  return -1;
}
