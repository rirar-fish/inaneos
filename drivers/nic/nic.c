#include "nic.h"
#include "pci.h"
#include <stdint.h>

int find_nic(device_s *device) {
  for (uint16_t bus = 0; bus < 256; bus++) {
    for (uint8_t slot = 0; slot < 32; slot++) {
      for (uint8_t func = 0; func < 8; func++) {
        uint16_t vendor = config_read16(bus, slot, func, VENDOR_ID);

        if (vendor == 0xFFFF) {
          continue;
        }

        uint32_t class_reg = config_read32(bus, slot, func, 0x08);
        uint8_t subclass = (class_reg >> 16) & 0xFF;
        uint8_t class_code = (class_reg >> 24) & 0xFF;

        if (class_code != 0x02 || subclass != 0x00) {
          continue;
        }

        if (device != 0) {
          device->bus = bus;
          device->slot = slot;
          device->func = func;
          device->vendor_id = vendor;
          device->device_id = config_read16(bus, slot, func, DEVICE_ID);
          device->class_code = class_code;
          for (int i = 0; i < 6; i++) {
            device->bar[i] = config_read32(bus, slot, func, BAR0 + (i + 4));
          }
        }
        return 0;
      }
    }
  }
  return -1;
}