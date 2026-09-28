#include "ethernet.h"
#include "vga.h"
#include <stdint.h>

static uint16_t swap16(uint16_t value) {
  return (uint16_t)((value >> 8) | (value << 8));
}

int ethernet_send(e1000_device_t *dev, const uint8_t *dest, uint16_t type,
                  const uint8_t *payload, uint16_t payload_len) {
  if (dev == 0 || dest == 0 || payload == 0) {
    return -1;
  }

  if (payload_len > 1500) {
    return -1;
  }

  uint8_t frame[1514];

  ethernet_header_t *header = (ethernet_header_t *)frame;

  for (int i = 0; i < 6; i++) {
    header->dest[i] = dest[i];
    header->src[i] = dev->mac[i];
  }
  header->type = swap16(type);

  for (uint16_t i = 0; i < payload_len; i++) {
    frame[ETH_HEADER_LEN + i] = payload[i];
  }
  uint16_t frame_len = ETH_HEADER_LEN + payload_len;

  return e1000_send(dev, frame, frame_len);
}

int ethernet_receive(e1000_device_t *dev, uint8_t *buffer,
                     uint16_t buffer_size) {
  return e1000_receive(dev, buffer, buffer_size);
}