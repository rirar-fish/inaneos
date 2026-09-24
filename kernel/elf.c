// elf loader
#include "elf.h"
#include "exec.h"
#include "pmm.h"
#include "vmm.h"

#define USER_MAX 0x40000000UL
#define PT_LOAD 1
#define PF_W 2

typedef struct {
  uint8_t ident[16];
  uint16_t type;
  uint16_t machine;
  uint32_t version;
  uint64_t entry;
  uint64_t phoff;
  uint64_t shoff;
  uint32_t flags;
  uint16_t ehsize;
  uint16_t phentsize;
  uint16_t phnum;
  uint16_t shentsize;
  uint16_t shnum;
  uint16_t shstrndx;
} elf64_ehdr;

typedef struct {
  uint32_t type;
  uint32_t flags;
  uint64_t off;
  uint64_t vaddr;
  uint64_t paddr;
  uint64_t filesz;
  uint64_t memsz;
  uint64_t align;
} elf64_phdr;

int elf_load(const uint8_t *img, unsigned long len, uint64_t *entry) {
  if (len < sizeof(elf64_ehdr))
    return -1;
  elf64_ehdr *e = (elf64_ehdr *)img;
  if (e->ident[0] != 0x7F || e->ident[1] != 'E' || e->ident[2] != 'L' ||
      e->ident[3] != 'F')
    return -1;
  if (e->ident[4] != 2 || e->ident[5] != 1 || e->machine != 0x3E)
    return -1;
  if (e->phnum && e->phentsize < sizeof(elf64_phdr))
    return -1;
  if (e->phoff + (uint64_t)e->phnum * e->phentsize > len)
    return -1;
  for (int i = 0; i < e->phnum; i++) {
    elf64_phdr *p =
        (elf64_phdr *)(img + e->phoff + (uint64_t)i * e->phentsize);
    uint64_t w = (p->flags & PF_W) ? PTE_W : 0;
    if (p->type != PT_LOAD || !p->memsz)
      continue;
    if (p->off + p->filesz > len || p->memsz < p->filesz)
      return -1;
    if (p->vaddr < USER_MIN || p->vaddr + p->memsz > USER_MAX)
      return -1;
    for (uint64_t va = p->vaddr & ~0xFFFUL; va < p->vaddr + p->memsz;
         va += 0x1000) {
      uint64_t f = pmm_alloc_frame();
      if (!f)
        return -1;
      vmm_map(va, f, PTE_P | PTE_U | w);
    }
    for (uint64_t o = 0; o < p->filesz; o++) {
      uint64_t phys = vmm_phys(p->vaddr + o);
      if (!phys)
        return -1;
      *(uint8_t *)(uintptr_t)phys = img[p->off + o];
    }
    for (uint64_t o = p->filesz; o < p->memsz; o++) {
      uint64_t phys = vmm_phys(p->vaddr + o);
      if (!phys)
        return -1;
      *(uint8_t *)(uintptr_t)phys = 0;
    }
  }
  if (e->entry < USER_MIN || e->entry >= USER_MAX)
    return -1;
  *entry = e->entry;
  return 0;
}
