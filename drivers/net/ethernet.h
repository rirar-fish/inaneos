#pragma once

#include <stdint.h>

#include "e1000.h"

#define ETH_ADDR_LEN 6
#define ETH_HEADER_LEN 14
#define ETH_TYPE_IPV4 0x8000
#define ETH_TYPE_ARP 0x0806

typedef struct {
  uint8_t dest[6];
  uint8_t src[6];
  uint16_t type;
} __attribute__((packed)) ethernet_header_t;

int ethernet_send(e1000_device_t *dev, const uint8_t *dest, uint16_t type,
                  const uint8_t *payload, uint16_t payload_len);

int ethernet_receive(e1000_device_t *dev, uint8_t *buffer,
                     uint16_t buffer_size);
