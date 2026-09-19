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

#ifdef __wasm__

extern void js_putchar(char c);

static inline void term_init(void) {}
static inline void term_clear(void) {}

static inline void term_putc(char c) {
    js_putchar(c);
}

static inline void term_puts(const char *s) {
    while (*s) {
        js_putchar(*s++);
    }
}

static inline void term_set_color(VgaColor fg, VgaColor bg) {
    (void)fg;
    (void)bg;
}

static inline void term_save_color(void) {}
static inline void term_reset_color(void) {}

#else
/// Initializes the terminal
extern void term_init(void);

/// Clear the terminal
extern void term_clear(void);

/// Put a single character onto the terminal
extern void term_putc(char c);

/// Put a string (chars) onto the terminal
extern void term_puts(const char *s);

/// Set a new color for the terminal
extern void term_set_color(VgaColor fg, VgaColor bg);

/// Save the current color for the terminal
/// Note: Use this when you need to set the color back and forth
extern void term_save_color();

/// Reset the color based on the saved color
extern void term_reset_color();
#endif
