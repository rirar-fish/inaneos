// vga.c - 80x25

#include "vga.h"
#include "io.h"

#define COLS 80
#define ROWS 25

static volatile unsigned short *const vga = (volatile unsigned short *)0xB8000;

static int row = 0;
static int col = 0;
static int wrap = 0;
static unsigned char attr = 0x0F;
static VgaColor saved_fg = VGA_COLOR_WHITE;
static VgaColor saved_bg = VGA_COLOR_BLACK;

// move cursor
static void update_cursor(void) {
  unsigned short pos;
  if (wrap)
    pos = (row + 1) * COLS;
  else
    pos = row * COLS + col;
  outb(0x3D4, 0x0F);
  outb(0x3D5, (unsigned char)(pos & 0xFF));
  outb(0x3D4, 0x0E);
  outb(0x3D5, (unsigned char)(pos >> 8));
}

// scroll up
static void scroll(void) {
  for (int i = 0; i < (ROWS - 1) * COLS; i++)
    vga[i] = vga[i + COLS];

  // clear last row
  for (int i = (ROWS - 1) * COLS; i < ROWS * COLS; i++)
    vga[i] = (unsigned short)((attr << 8) | ' ');

  // back to last row
  row = ROWS - 1;
  wrap = 0;
}

// TODO: add colors
void term_init(void) {
  attr = 0x0F;
  term_clear();
}

void term_clear(void) {
  for (int i = 0; i < ROWS * COLS; i++)
    vga[i] = (unsigned short)((attr << 8) | ' ');
  row = (col = 0);
  wrap = 0;
  update_cursor();
}

void term_putc(char c) {
  // pending wrap lands first
  if (wrap && c != '\n' && c != '\r') {
    wrap = 0;
    col = 0;
    row++;
    if (row >= ROWS)
      scroll();
  }
  switch (c) {
  case '\n':
    wrap = 0;
    col = 0;
    row++;
    break;
  case '\r':
    wrap = 0;
    col = 0;
    break;
  case '\b':
    if (wrap) {
      wrap = 0;
      col = COLS - 1;
    }
    if (col > 0) {
      col--;
      vga[row * COLS + col] = (unsigned short)((attr << 8) | ' ');
    }
    break;
  case '\t':
    col = (col + 4) & ~3;
    if (col >= COLS) {
      col = 0;
      row++;
    }
    break;
  default:
    vga[row * COLS + col] = (unsigned short)((attr << 8) | (unsigned char)c);
    col++;
    if (col >= COLS)
      wrap = 1;
  }

  // scroll if full
  if (row >= ROWS)
    scroll();

  // move cursor
  update_cursor();
}

void term_puts(const char *s) {
  for (; *s; s++)
    term_putc(*s);
}

void term_write(const char *s, unsigned long len) {
  for (unsigned long i = 0; i < len; i++)
    term_putc(s[i]);
}

void term_goto(int r, int c) {
  if (r < 0)
    r = 0;
  if (r >= ROWS)
    r = ROWS - 1;
  if (c < 0)
    c = 0;
  if (c >= COLS)
    c = COLS - 1;
  row = r;
  col = c;
  wrap = 0;
  update_cursor();
}

void term_set_color(VgaColor fg, VgaColor bg) {
  // FIXME: check bad color
  attr = (unsigned char)((bg << 4) | (fg & 0x0F));
}

void term_save_color() {
  saved_fg = (VgaColor)(attr & 0x0F);
  saved_bg = (VgaColor)((attr >> 4) & 0x0F);
}

void term_reset_color() { term_set_color(saved_fg, saved_bg); }