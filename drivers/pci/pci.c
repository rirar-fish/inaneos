#include "pci.h"
#include "io.h"
#include "vga.h"
#include <stdint.h>

static uint32_t make_address(uint8_t bus, uint8_t slot, uint8_t func,
                             uint8_t offset) {
  return (uint32_t)(0x80000000u | ((uint32_t)bus << 16) |
                    ((uint32_t)slot << 11) | ((uint32_t)func << 8) |
                    (offset & 0xFC)

  );
}

uint32_t config_read32(uint8_t bus, uint8_t slot, uint8_t func,
                       uint8_t offset) {
  uint32_t address = make_address(bus, slot, func, offset);
  outl(CONFIG_ADDRESS, address);
  return inl(CONFIG_DATA);
}

uint16_t config_read16(uint8_t bus, uint8_t slot, uint8_t func,
                       uint8_t offset) {
  uint32_t value = config_read32(bus, slot, func, offset);
  uint8_t shift = (offset & 2) * 8;

  return (uint16_t)((value >> shift) & 0xFFFF);
}

void config_write32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset,
                    uint32_t value) {
  uint32_t address = make_address(bus, slot, func, offset);

  outl(CONFIG_ADDRESS, address);
  outl(CONFIG_DATA, value);
}

void config_write16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset,
                    uint16_t value) {
  uint32_t current;
  uint32_t address = make_address(bus, slot, func, offset);
  uint8_t shift = (offset & 2) * 8;

  outl(CONFIG_ADDRESS, address);

  current = inl(CONFIG_DATA);

  current &= ~(0xFFFFu << shift);
  current |= ((uint32_t)value << shift);
  outl(CONFIG_ADDRESS, address);
  outl(CONFIG_DATA, current);
}

void enable_bus_mastering(uint8_t bus, uint8_t slot, uint8_t func) {
  uint16_t command;

  command = config_read16(bus, slot, func, PCI_COMMAND);
  command |= COMMAND_BUS_MASTER;
  config_write16(bus, slot, func, PCI_COMMAND, command);
}

int find_device(uint16_t vendor_id, uint16_t device_id, device_s *device) {
  for (uint16_t bus = 0; bus < 256; bus++) {
    for (uint8_t slot = 0; slot < 32; slot++) {
      for (uint8_t func = 0; func < 8; func++) {
        uint16_t vendor = config_read16(bus, slot, func, VENDOR_ID);

        if (vendor == 0xFFFF) {
          continue;
        }

        uint16_t found_device = config_read16(bus, slot, func, DEVICE_ID);

        if (vendor != vendor_id || found_device != device_id) {
          continue;
        }

        if (device != 0) {
          device->bus = bus;
          device->slot = slot;
          device->func = func;

          device->vendor_id = vendor;
          device->device_id = found_device;

          device->class_code = (uint8_t)config_read16(bus, slot, func, CLASS);

          device->subclass = (uint8_t)config_read16(bus, slot, func, SUBCLASS);

          for (int i = 0; i < 6; i++) {
            device->bar[i] = config_read32(bus, slot, func, BAR0 + (i * 4));
          }
        }

        return 0;
      }
    }
  }

  return -1;
}

static void print_hex8(uint8_t value) {
  const char *hex = "0123456789ABCDEF";

  char buf[3];
  buf[0] = hex[(value >> 4) & 0xF];
  buf[1] = hex[value & 0xF];
  buf[2] = '\0';

  term_puts(buf);
}

static void print_hex16(uint16_t value) {
  print_hex8((uint8_t)(value >> 8));
  print_hex8((uint8_t)value);
}

void pci_scan(void) {
  for (uint16_t bus = 0; bus < 256; bus++) {
    for (uint8_t slot = 0; slot < 32; slot++) {
      for (uint8_t func = 0; func < 8; func++) {
        uint16_t vendor = config_read16(bus, slot, func, VENDOR_ID);

        if (vendor == 0xFFFF) {
          continue;
        }

        uint16_t device = config_read16(bus, slot, func, DEVICE_ID);

        term_puts("PCI ");

        print_hex8((uint8_t)bus);
        term_puts(":");

        print_hex8(slot);
        term_puts(".");

        print_hex8(func);

        term_puts(" Vendor=");
        print_hex16(vendor);

        term_puts(" Device=");
        print_hex16(device);

        term_puts("\n");
      }
    }
  }

  term_puts("PCI scan complete.\n");
  term_clear();
}
