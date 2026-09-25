#pragma once

// 80x25 text grid for the wasm build. Behavior ported 1:1 from
// drivers/vga/vga.c; the cells live in wasm linear memory so the
// browser renders the very same bytes QEMU would put at 0xB8000.
void term_init(void);
void term_clear(void);
void term_putc(char c);
void term_puts(const char *s);
void term_write(const char *s, unsigned long len);
void term_goto(int r, int c);
void term_set_color(int fg, int bg);

// render API for JS (see web/term.js)
unsigned short *term_cells(void); // ROWS*COLS cells, low byte char, high byte attr
int term_cursor(void);            // linear cursor position
int term_cols(void);
int term_rows(void);
