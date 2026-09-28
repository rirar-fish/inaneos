#include "e1000.h"
#include "pci.h"
#include "vga.h"
#include <stdint.h>

#define TDBAL 0x3800
#define TDBAH 0x3804
#define TDLEN 0x3808
#define TDH 0x3810
#define TDT 0x3818
#define TCTL 0x0400
#define RDBAL 0x2800
#define RDBAH 0x2804
#define RDLEN 0x2808
#define RDH 0x2810
#define RDT 0x2818
#define RCTL 0x0100

#define STATUS 0x06
#define CTRL 0x0000

#define RCTL_EN (1 << 1)
#define RCTL_BAM (1 << 15)
#define RCTL_BSIZE_2048 0
#define RCTL_SECRC (1 << 26)

#define TCTL_EN (1 << 1)
#define TCTL_PSP (1 << 3)

static void setup_tx(e1000_device_t *dev);
static void setup_rx(e1000_device_t *dev);
static void mmio_write(e1000_device_t *dev, uint32_t offset, uint32_t value);
static uint32_t mmio_read(e1000_device_t *dev, uint32_t offset);

static void print_hex32(uint32_t value) {
  const char *hex = "0123456789ABCDEF";
  char buf[9];

  for (int i = 0; i < 8; i++) {
    buf[7 - i] = hex[value & 0xF];
    value >>= 4;
  }

  buf[8] = '\0';
  term_puts(buf);
}

static void mmio_write(e1000_device_t *dev, uint32_t offset, uint32_t value) {
  dev->mmio[offset / 4] = value;
}

static uint32_t mmio_read(e1000_device_t *dev, uint32_t offset) {
  return dev->mmio[offset / 4];
}

int e1000_init(e1000_device_t *dev, device_s *pci_dev) {
  if (dev == 0 || pci_dev == 0) {
    return -1;
  }

  dev->tx_index = 0;
  dev->rx_index = 0;

  dev->bus = pci_dev->bus;
  dev->slot = pci_dev->slot;
  dev->func = pci_dev->func;

  dev->mmio = (uint32_t *)(uintptr_t)(pci_dev->bar[0] & 0xFFFFFFF0);

  term_puts("E1000 INIT\n");
  term_puts("Vendor: ");
  print_hex32(pci_dev->vendor_id);
  term_puts("\n");

  term_puts("Device: ");
  print_hex32(pci_dev->device_id);
  term_puts("\n");

  term_puts("BAR0: ");
  print_hex32(pci_dev->bar[0]);
  term_puts("\n");

  enable_bus_mastering(pci_dev->bus, pci_dev->slot, pci_dev->func);

  term_puts("Bus Mastering Enabled\n");
  setup_tx(dev);
  setup_rx(dev);

  term_puts("RX RDH: ");
  print_hex32(mmio_read(dev, RDH));
  term_puts("\n");

  term_puts("RX RDT: ");
  print_hex32(mmio_read(dev, RDT));
  term_puts("\n");

  term_puts("RX RCTL: ");
  print_hex32(mmio_read(dev, RCTL));
  term_puts("\n");
  term_puts("E1000: TX/RX Ready!\n");

  return 0;
}

static void setup_tx(e1000_device_t *dev) {
  for (uint32_t i = 0; i < NUM_TX_DESC; i++) {
    dev->tx_desc[i].addr = (uint64_t)(uintptr_t)&dev->tx_buffers[i][0];

    dev->tx_desc[i].length = 0;
    dev->tx_desc[i].cso = 0;
    dev->tx_desc[i].cmd = 0;
    dev->tx_desc[i].css = 0;
    dev->tx_desc[i].cmd = 0;
    dev->tx_desc[i].status = TXD_STAT_DD;
  }

  uint64_t addr = (uint64_t)(uintptr_t)&dev->tx_desc[0];

  mmio_write(dev, TDBAL, (uint32_t)(addr & 0xFFFFFFFF));
  mmio_write(dev, TDBAH, (uint32_t)(addr >> 32));

  mmio_write(dev, TDLEN, NUM_TX_DESC * sizeof(e1000_tx_desc_t));

  mmio_write(dev, TDH, 0);
  mmio_write(dev, TDT, 0);

  mmio_write(dev, TCTL, TCTL_EN | TCTL_PSP);
}

static void setup_rx(e1000_device_t *dev) {
  dev->rx_index = 0;

  for (uint32_t i = 0; i < NUM_RX_DESC; i++) {
    dev->rx_desc[i].addr = (uint64_t)(uintptr_t)&dev->rx_buffers[i][0];

    dev->rx_desc[i].length = 0;
    dev->rx_desc[i].status = 0;
    dev->rx_desc[i].checksum = 0;
    dev->rx_desc[i].errors = 0;
    dev->rx_desc[i].special = 0;
  }

  uint64_t addr = (uint64_t)(uintptr_t)&dev->rx_desc[0];

  mmio_write(dev, RDBAL, (uint32_t)addr & 0xFFFFFFFF);
  mmio_write(dev, RDBAH, (uint32_t)(addr >> 32));

  mmio_write(dev, RDLEN, NUM_RX_DESC * sizeof(e1000_rx_desc_t));
  mmio_write(dev, RDH, 0);

  mmio_write(dev, RDT, NUM_RX_DESC - 1);

  uint32_t rctl = RCTL_EN | RCTL_BAM | RCTL_SECRC;

  mmio_write(dev, RCTL, rctl);
}

int e1000_send(e1000_device_t *dev, const uint8_t *data, uint16_t length) {
  if (dev == 0 || data == 0) {
    return -1;
  }

  if (length == 0 || length > PACKET_SIZE) {
    return -1;
  }

  uint32_t index = dev->tx_index % NUM_TX_DESC;
  e1000_tx_desc_t *desc = &dev->tx_desc[index];

  if (!(desc->status & TXD_STAT_DD)) {
    return -1;
  }

  for (uint16_t i = 0; i < length; i++) {
    dev->tx_buffers[index][i] = data[i];
  }

  desc->length = length;
  desc->cso = 0;
  desc->css = 0;
  desc->special = 0;

  desc->cmd = TXD_CMD_EOP | TXD_CMD_IFCS | TXD_CMD_RS;

  desc->status = 0;

  uint32_t next = (index + 1) % NUM_TX_DESC;
  mmio_write(dev, TDT, next);

  dev->tx_index = next;

  return 0;
}

int e1000_receive(e1000_device_t *dev, uint8_t *buffer, uint16_t buffer_size) {
  if (dev == 0 || buffer == 0) {
    return -1;
  }

  uint32_t index = dev->rx_index % NUM_RX_DESC;
  e1000_rx_desc_t *desc = &dev->rx_desc[index];

  if (!(desc->status & RXD_STAT_DD)) {
    return 0;
  }

  uint16_t length = desc->length;

  if (length > buffer_size) {
    length = buffer_size;
  }

  for (uint16_t i = 0; i < length; i++) {
    buffer[i] = dev->rx_buffers[index][i];
  }

  desc->status = 0;

  mmio_write(dev, RDT, index);

  dev->rx_index = (index + 1) % NUM_RX_DESC;

  return (int)length;
}
