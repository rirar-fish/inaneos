// main kernel
// module

#include "idt.h"
#include "io.h"
#include "keyboard.h"
#include "shell.h"
#include "vga.h"
#include <stdint.h>

#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002
#define MULTIBOOT_INFO_MEM_MAP 0x00000040
#define MULTIBOOT_MEMORY_AVAILABLE 1

typedef struct {
  uint32_t size;
  uint64_t addr;
  uint64_t len;
  uint32_t type;
} __attribute__((packed)) multiboot_mmap_entry;

typedef struct {
  uint32_t flags;
  uint32_t mem_lower;
  uint32_t mem_upper;
  uint32_t boot_device;
  uint32_t cmdline;
  uint32_t mods_count;
  uint32_t mods_addr;

  union {
    struct {
      uint32_t tabsize;
      uint32_t strsize;
      uint32_t addr;
      uint32_t reserved;
    } aout_sym;

    struct {
      uint32_t num;
      uint32_t size;
      uint32_t addr;
      uint32_t shndx;
    } elf_sec;
  };

  uint32_t mmap_length;
  uint32_t mmap_addr;

  uint32_t drives_length;
  uint32_t drives_addr;
  uint32_t config_table;
  uint32_t boot_loader_name;
  uint32_t apm_table;

  uint32_t vbe_control_info;
  uint32_t vbe_mode_info;
  uint16_t vbe_mode;
  uint16_t vbe_interface_seg;
  uint16_t vbe_interface_off;
  uint16_t vbe_interface_len;

  uint64_t framebuffer_addr;
  uint32_t framebuffer_pitch;
  uint32_t framebuffer_width;
  uint32_t framebuffer_height;
  uint8_t framebuffer_bpp;
  uint8_t framebuffer_type;
} multiboot_info;

void kernel_main(unsigned int magic, unsigned int mbi_addr) {
  if (magic != MULTIBOOT_BOOTLOADER_MAGIC)
    return;

  multiboot_info *mbi = (multiboot_info *)(uintptr_t)mbi_addr;

  if (mbi->flags & MULTIBOOT_INFO_MEM_MAP) {
    uint8_t *current = (uint8_t *)(uintptr_t)mbi->mmap_addr;
    uint8_t *end = current + mbi->mmap_length;

    while (current < end) {
      multiboot_mmap_entry *entry = (multiboot_mmap_entry *)current;

      if (entry->type == MULTIBOOT_MEMORY_AVAILABLE) {
        // TODO: feed free mem to PMM
      }

      current += entry->size + sizeof(entry->size);
    }
  }

  term_init();
  term_puts("inaneos v0.0.3\n");

  idt_init();
  pic_init();
  sti();

  shell_run();
  // FIXME: handle bad magic
}
