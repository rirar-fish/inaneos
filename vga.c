// vga.c - 80x25
#include "vga.h"
#include "io.h"

#define COLS 80
#define ROWS 25

static volatile unsigned short *const vga = (volatile unsigned short *)0xB8000;

static int row = 0;
static int col = 0;
static unsigned char attr = 0x0F;
static VgaColor saved_fg = VGA_COLOR_WHITE;
static VgaColor saved_bg = VGA_COLOR_BLACK;

/// Update the cursor based on the new position
static void update_cursor(void) {
  unsigned short pos = row * COLS + col;
  outb(0x3D4, 0x0F);
  outb(0x3D5, (unsigned char)(pos & 0xFF));
  outb(0x3D4, 0x0E);
  outb(0x3D5, (unsigned char)(pos >> 8));
}

/// Scroll the VGA buffer up by one row.
static void scroll(void) {
  for (int i = 0; i < (ROWS - 1) * COLS; i++)
    vga[i] = vga[i + COLS];

  // clear the last row and fill it with spaces using the current attribute.
  for (int i = (ROWS - 1) * COLS; i < ROWS * COLS; i++)
    vga[i] = (unsigned short)((attr << 8) | ' ');

  // move the cursor to the beginning of the last row.
  row = ROWS - 1;
}

void term_init(void) {
  attr = 0x0F;
  term_clear();
}

void term_clear(void) {
  for (int i = 0; i < ROWS * COLS; i++)
    vga[i] = (unsigned short)((attr << 8) | ' ');
  row = (col = 0);
  update_cursor();
}

void term_putc(char c) {
  switch (c) {
  case '\n':
    col = 0;
    row++;
    break;
  case '\r':
    col = 0;
    break;
  case '\b':
    if (col > 0) {
      col--;
      vga[row * COLS + col] = (unsigned short)((attr << 8) | ' ');
    }
    break;
  case '\t':
    col = (col + 4) & ~3;
    break;
  default:
    vga[row * COLS + col] = (unsigned short)((attr << 8) | (unsigned char)c);
    col++;

    // wraps to another row if the column is exceeding the COLS
    if (col >= COLS) {
      col = 0;
      row++;
    }
  }

  // scroll down by 1 row if the current row is exceeding the ROWS
  if (row >= ROWS)
    scroll();

  // update the cursor by the new position
  update_cursor();
}

void term_puts(const char *s) {
  for (; *s; s++)
    term_putc(*s);
}

void term_set_color(VgaColor fg, VgaColor bg) {
  attr = (unsigned char)((bg << 4) | (fg & 0x0F));
}

void term_save_color() {
  saved_fg = (VgaColor)(attr & 0x0F);
  saved_bg = (VgaColor)((attr >> 4) & 0x0F);
}

void term_reset_color() { term_set_color(saved_fg, saved_bg); }
