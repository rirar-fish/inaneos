// ata pio driver
#include "ide.h"
#include "io.h"

#define BASE 0x1F0
#define CTL 0x3F6
#define REG_DATA 0
#define REG_COUNT 2
#define REG_LBA0 3
#define REG_LBA1 4
#define REG_LBA2 5
#define REG_DRIVE 6
#define REG_CMD 7
#define ST_BSY 0x80
#define ST_DRDY 0x40
#define ST_DRQ 0x08
#define ST_ERR 0x01
#define CMD_IDENTIFY 0xEC
#define CMD_READ_EXT 0x24
#define CMD_WRITE_EXT 0x34
#define CMD_FLUSH_EXT 0xEA

static uint64_t total;
static int present;

static void delay400(void) {
  inb(CTL);
  inb(CTL);
  inb(CTL);
  inb(CTL);
}

static int wait_clear(unsigned char mask, int timeout) {
  while ((inb(BASE + REG_CMD) & mask) && timeout--)
    ;
  return timeout > 0 ? 0 : -1;
}

static int wait_set(unsigned char mask, int timeout) {
  while (!(inb(BASE + REG_CMD) & mask) && timeout--)
    ;
  return timeout > 0 ? 0 : -1;
}

int ide_init(void) {
  uint16_t id[256];
  if (inb(BASE + REG_CMD) == 0xFF)
    return -1; // floating bus
  outb(BASE + REG_DRIVE, 0xE0);
  delay400();
  outb(BASE + REG_CMD, CMD_IDENTIFY);
  delay400();
  if (inb(BASE + REG_CMD) == 0)
    return -1; // no drive
  if (wait_clear(ST_BSY, 100000) != 0)
    return -1;
  if (inb(BASE + REG_CMD) & ST_ERR)
    return -1;
  if (wait_set(ST_DRQ, 100000) != 0)
    return -1;
  for (int i = 0; i < 256; i++)
    id[i] = inw(BASE + REG_DATA);
  if (!(id[83] & (1 << 10)))
    return -1; // need lba48
  total = (uint64_t)id[100] | ((uint64_t)id[101] << 16) |
          ((uint64_t)id[102] << 32) | ((uint64_t)id[103] << 48);
  present = 1;
  return 0;
}

int ide_read(uint64_t lba, void *buf) {
  uint16_t *w = (uint16_t *)buf;
  if (!present || lba >= total)
    return -1;
  if (wait_clear(ST_BSY, 100000) != 0)
    return -1;
  outb(BASE + REG_DRIVE, 0xE0);
  outb(BASE + REG_COUNT, 0);
  outb(BASE + REG_LBA0, (lba >> 24) & 0xFF);
  outb(BASE + REG_LBA1, (lba >> 32) & 0xFF);
  outb(BASE + REG_LBA2, (lba >> 40) & 0xFF);
  outb(BASE + REG_COUNT, 1);
  outb(BASE + REG_LBA0, lba & 0xFF);
  outb(BASE + REG_LBA1, (lba >> 8) & 0xFF);
  outb(BASE + REG_LBA2, (lba >> 16) & 0xFF);
  outb(BASE + REG_CMD, CMD_READ_EXT);
  if (wait_clear(ST_BSY, 100000) != 0)
    return -1;
  if (wait_set(ST_DRQ, 100000) != 0)
    return -1;
  for (int i = 0; i < 256; i++)
    w[i] = inw(BASE + REG_DATA);
  delay400();
  return 0;
}

int ide_write(uint64_t lba, const void *buf) {
  const uint16_t *w = (const uint16_t *)buf;
  if (!present || lba >= total)
    return -1;
  if (wait_clear(ST_BSY, 100000) != 0)
    return -1;
  outb(BASE + REG_DRIVE, 0xE0);
  outb(BASE + REG_COUNT, 0);
  outb(BASE + REG_LBA0, (lba >> 24) & 0xFF);
  outb(BASE + REG_LBA1, (lba >> 32) & 0xFF);
  outb(BASE + REG_LBA2, (lba >> 40) & 0xFF);
  outb(BASE + REG_COUNT, 1);
  outb(BASE + REG_LBA0, lba & 0xFF);
  outb(BASE + REG_LBA1, (lba >> 8) & 0xFF);
  outb(BASE + REG_LBA2, (lba >> 16) & 0xFF);
  outb(BASE + REG_CMD, CMD_WRITE_EXT);
  if (wait_clear(ST_BSY, 100000) != 0)
    return -1;
  if (wait_set(ST_DRQ, 100000) != 0)
    return -1;
  for (int i = 0; i < 256; i++)
    outw(BASE + REG_DATA, w[i]);
  outb(BASE + REG_CMD, CMD_FLUSH_EXT);
  if (wait_clear(ST_BSY, 100000) != 0)
    return -1;
  delay400();
  return 0;
}

uint64_t ide_sectors(void) { return total; }
