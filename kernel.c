// main kernel
// module
#ifndef __wasm__
#include "idt.h"
#include "keyboard.h"
#endif

#include "io.h"
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
  uint32_t flags;       // 0
  uint32_t mem_lower;   // 4
  uint32_t mem_upper;   // 8
  uint32_t boot_device; // 12
  uint32_t cmdline;     // 16
  uint32_t mods_count;  // 20
  uint32_t mods_addr;   // 24

  // offset 28: union
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

  uint32_t mmap_length; // 44
  uint32_t mmap_addr;   // 48

  uint32_t drives_length;    // 52
  uint32_t drives_addr;      // 56
  uint32_t config_table;     // 60
  uint32_t boot_loader_name; // 64
  uint32_t apm_table;        // 68

  uint32_t vbe_control_info;  // 72
  uint32_t vbe_mode_info;     // 76
  uint16_t vbe_mode;          // 80
  uint16_t vbe_interface_seg; // 82
  uint16_t vbe_interface_off; // 84
  uint16_t vbe_interface_len; // 86

  uint64_t framebuffer_addr;   // 88
  uint32_t framebuffer_pitch;  // 96
  uint32_t framebuffer_width;  // 100
  uint32_t framebuffer_height; // 104
  uint8_t framebuffer_bpp;     // 108
  uint8_t framebuffer_type;    // 109
} multiboot_info;

#ifdef  __wasm__
__attribute__((export_name("kernel_main")))
void kernel_main(void){
    shell_run();
}
#else

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
        // This region can potentially be given to the PMM (Physical Memory
        // Manager).
        // TODO: PMM
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
  // TODO: idt + pic next
  // TODO: tiny shell
}
#endif
