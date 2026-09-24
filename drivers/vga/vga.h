// vga.h - screen api

#pragma once

typedef enum {
    VGA_COLOR_BLACK         = 0,
    VGA_COLOR_BLUE          = 1,
    VGA_COLOR_GREEN         = 2,
    VGA_COLOR_CYAN          = 3,
    VGA_COLOR_RED           = 4,
    VGA_COLOR_MAGENTA       = 5,
    VGA_COLOR_BROWN         = 6,
    VGA_COLOR_LIGHT_GREY    = 7,
    VGA_COLOR_DARK_GREY     = 8,
    VGA_COLOR_LIGHT_BLUE    = 9,
    VGA_COLOR_LIGHT_GREEN   = 10,
    VGA_COLOR_LIGHT_CYAN    = 11,
    VGA_COLOR_LIGHT_RED     = 12,
    VGA_COLOR_LIGHT_MAGENTA = 13,
    VGA_COLOR_LIGHT_BROWN   = 14,
    VGA_COLOR_WHITE         = 15
} VgaColor;

// init screen
extern void term_init(void);

// clear screen
extern void term_clear(void);

// put char
extern void term_putc(char c);

// put string
extern void term_puts(const char *s);

// put bytes
extern void term_write(const char *s, unsigned long len);

// move cursor
extern void term_goto(int row, int col);

// set color
extern void term_set_color(VgaColor fg, VgaColor bg);

// save color
// TODO: stack saved colors
extern void term_save_color();

// load color
// FIXME: check empty save
extern void term_reset_color();