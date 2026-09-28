#pragma once

#include "pci.h"
#include <stdint.h>

#define NUM_TX_DESC 8
#define NUM_RX_DESC 8
#define PACKET_SIZE 2048
#define TXD_CMD_EOP 0X01
#define TXD_CMD_IFCS 0x02
#define TXD_CMD_RS 0x08
#define TXD_STAT_DD 0x01
#define RXD_STAT_DD 0x01
#define RXD_STAT_EOP 0x02

typedef struct {
  uint64_t addr;
  uint16_t length;
  uint8_t cso;
  uint8_t cmd;
  uint8_t status;
  uint8_t css;
  uint16_t special;

} e1000_tx_desc_t;

typedef struct {
  uint64_t addr;
  uint16_t length;
  uint16_t checksum;
  uint8_t status;
  uint8_t errors;
  uint16_t special;

} e1000_rx_desc_t;

typedef struct {
  uint32_t *mmio;
  uint8_t mac[6];

  uint8_t bus;
  uint8_t slot;
  uint8_t func;

  e1000_tx_desc_t tx_desc[NUM_TX_DESC];
  e1000_rx_desc_t rx_desc[NUM_RX_DESC];
  uint8_t tx_buffers[NUM_TX_DESC][PACKET_SIZE];
  uint8_t rx_buffers[NUM_RX_DESC][PACKET_SIZE];

  uint32_t tx_index;
  uint32_t rx_index;
} e1000_device_t;

int e1000_init(e1000_device_t *dev, device_s *pci_dev);

int e1000_send(e1000_device_t *dev, const uint8_t *data, uint16_t length);

int e1000_receive(e1000_device_t *dev, uint8_t *buffer, uint16_t buffer_size);