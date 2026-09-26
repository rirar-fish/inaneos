#pragma once
#include <stdint.h>

#define CONFIG_ADDRESS 0xCF8
#define CONFIG_DATA 0xCFC

#define VENDOR_ID 0x00
#define DEVICE_ID 0x02
#define PCI_COMMAND 0x04
#define STATUS 0x06
#define CLASS 0x0B
#define SUBCLASS 0x0A
#define HEADER_TYPE 0X0E
#define BAR0 0x10

#define COMMAND_BUS_MASTER 0x0004

typedef struct {
  uint8_t bus;
  uint8_t slot;
  uint8_t func;
  uint16_t vendor_id;
  uint16_t device_id;
  uint8_t class_code;
  uint8_t subclass;
  uint32_t bar[6];
} device_s;

uint32_t config_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);

uint16_t config_read16(

    uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset

);

void config_write32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset,
                    uint32_t value);

void config_write16(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset,
                    uint16_t value

);
int find_device(uint16_t vendor_id, uint16_t device_id, device_s *device);

void enable_bus_mastering(uint8_t bus, uint8_t slot, uint8_t func);

void pci_scan(void);