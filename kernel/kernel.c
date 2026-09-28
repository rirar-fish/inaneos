// main kernel
// module

#include "e1000.h"
#include "exec.h"
#include "fs.h"
#include "gdt.h"
#include "idt.h"
#include "io.h"
#include "keyboard.h"
#include "nic.h"
#include "part.h"
#include "pci.h"
#include "pmm.h"
#include "syscall.h"
#include "vga.h"
#include <stdint.h>

#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002
#define MULTIBOOT_INFO_MEM_MAP 0x00000040

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

static const uint8_t test_packet[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x52,
                                      0x54, 0x00, 0x12, 0x34, 0x56, 0x08, 0x00};

void kernel_main(unsigned int magic, unsigned int mbi_addr) {
  if (magic != MULTIBOOT_BOOTLOADER_MAGIC)
    return;

  multiboot_info *mbi = (multiboot_info *)(uintptr_t)mbi_addr;

  // feed mem map to PMM
  if (mbi->flags & MULTIBOOT_INFO_MEM_MAP)
    pmm_init(mbi->mmap_addr, mbi->mmap_length);

  term_init();

  pci_scan();
  device_s nic;
  gdt_install();

  idt_init();
  pic_init();
  syscall_init();
  fs_init();
  part_scan();
  if (part_count() > 0)
    fs_mount_part(0); // disk first if present
  sti();

  if (find_nic(&nic) == 0) {
    term_puts("NIC FOUND\n");

    e1000_device_t e1000;

    if (e1000_init(&e1000, &nic) == 0) {
      term_puts("E1000 INIT OK\n");

      // TODO: Implement ARP request

      if (e1000_send(&e1000, test_packet, sizeof(test_packet)) == 0) {
        term_puts("E1000 TX OK\n");

      } else {
        term_puts("E1000 trx failed\n");
      }

      uint8_t rx_buffer[2048];
      int received = e1000_receive(&e1000, rx_buffer, sizeof(rx_buffer));
      if (received > 0) {
        term_puts("E1000 RX: Packet Received\n");
      } else {
        term_puts("E1000 RX: No Packet\n");
      }

    } else {
      term_puts("E1000 INIT FAILED\n");
    }

  } else {
    term_puts("NIC NOT FOUND\n");
  }

  // load init program
  if (mbi->mods_count < 1) {
    term_puts("no init module\n");
    for (;;) {
      __asm__ volatile("hlt");
    }
  }
  exec_init_mods(mbi->mods_addr, mbi->mods_count);
  if (enter_program(0, 0) != 0) {
    term_puts("bad init elf\n");
    for (;;) {
      __asm__ volatile("hlt");
    }
  }
  for (;;) {
    __asm__ volatile("hlt");
  }
  // FIXME: handle bad magic
}
