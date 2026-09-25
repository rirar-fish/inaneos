// term grid: logic port of drivers/vga/vga.c (update_cursor is a no-op,
// the browser reads the cursor via term_cursor()).
#include "term_grid.h"

#define COLS 80
#define ROWS 25

static unsigned short cells[ROWS * COLS];
static int row = 0;
static int col = 0;
static int wrap = 0;
static unsigned char attr = 0x0F;

void term_init(void) {
  attr = 0x0F;
  term_clear();
}

void term_clear(void) {
  for (int i = 0; i < ROWS * COLS; i++)
    cells[i] = (unsigned short)((attr << 8) | ' ');
  row = (col = 0);
  wrap = 0;
}

void term_putc(char c) {
  // pending wrap lands first
  if (wrap && c != '\n' && c != '\r') {
    wrap = 0;
    col = 0;
    row++;
    if (row >= ROWS) {
      for (int i = 0; i < (ROWS - 1) * COLS; i++)
        cells[i] = cells[i + COLS];
      for (int i = (ROWS - 1) * COLS; i < ROWS * COLS; i++)
        cells[i] = (unsigned short)((attr << 8) | ' ');
      row = ROWS - 1;
      wrap = 0;
    }
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
      cells[row * COLS + col] = (unsigned short)((attr << 8) | ' ');
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
    cells[row * COLS + col] = (unsigned short)((attr << 8) | (unsigned char)c);
    col++;
    if (col >= COLS)
      wrap = 1;
  }

  // scroll if full
  if (row >= ROWS) {
    for (int i = 0; i < (ROWS - 1) * COLS; i++)
      cells[i] = cells[i + COLS];
    for (int i = (ROWS - 1) * COLS; i < ROWS * COLS; i++)
      cells[i] = (unsigned short)((attr << 8) | ' ');
    row = ROWS - 1;
    wrap = 0;
  }
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
}

void term_set_color(int fg, int bg) { attr = (unsigned char)((bg << 4) | (fg & 0x0F)); }

unsigned short *term_cells(void) { return cells; }

int term_cursor(void) {
  if (wrap)
    return (row + 1) * COLS;
  return row * COLS + col;
}

int term_cols(void) { return COLS; }
int term_rows(void) { return ROWS; }
