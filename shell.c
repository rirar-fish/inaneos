#include "shell.h"
#include "io.h"
#include "vga.h"

#ifndef __wasm__
#include "keyboard.h"
#endif

static int same_condition(const char *a, const char *b) {
  while (*a && *a == *b) {
    a++;
    b++;
  }
  return *a == *b;
}

static int start(const char *s, const char *pre) {
  while (*pre)
    if (*s++ != *pre++)
      return 0;
  return 1;
}

static void running(const char *line) {
  if (line[0] == '\0')
    return;

  if (same_condition(line, "help")) {
    term_puts("Available Command:\n");
    term_puts("help - Command List:\n");
    term_puts("echo - Print the Text:\n");
    term_puts("info - About:\n");
    term_puts("reboot - Restart:\n");

  } else if (same_condition(line, "echo")) {
    term_puts("\n");
  } else if (start(line, "echo ")) {
    term_puts(line + 5);
    term_puts("\n");
  } else if (same_condition(line, "info")) {
    term_puts("Inaneos beta 0.0 version\n");
  } else if (same_condition(line, "reboot")) {
#ifndef  __wasm__
    term_puts("rebooting wasm env... \n");
#else
    outb(0x64, 0xFE);
    for (;;)
      halt();
#endif
  } else {
    term_puts(line);
    term_puts(": Command Not Found\n");
  }
}

#ifndef __wasm__

static char wasm_line[128];
static int wasm_idx = 0;

static void print_promt(void) {
    term_set_color(0x0A, 0x00);
    term_puts("$ ");
    term_set_color(0x0F, 0x00);
}

void shell_run(void) {
    wasm_idx = 0;
    print_promt();
}

void shell_handle_key(char c) {
    if (c == '\n' || c == '\r') {
        term_putc('\n');
        wasm_line[wasm_idx] = '\0';
        running(wasm_line);
        wasm_idx=0;
        print_promt();
    } else if (c == '\b') {
        if (wasm_idx > 0) {
            wasm_idx--;
            term_putc('\b');
        }
    } else if (c >= 32 && c < 127 && wasm_idx < (int)sizeof(wasm_line) - 1) {
        wasm_line[wasm_idx++] = c;
    }
}

#else
void shell_run(void) {
  char line[128];

  for (;;) {
    term_set_color(0x0A, 0x00);
    term_puts("$ ");
    term_set_color(0x0F, 0x00);

    int i = 0;
    for (;;) {
      char c = (char)getchar();
      if (c == '\n')
        break;
      if (c == '\b') {
        if (i > 0) {
          i--;
          term_putc('\b');
        }
      } else if (c >= 32 && c < 127 && i < (int)sizeof(line) - 1) {
        line[i++] = c;
        term_putc(c);
      }
    }
    term_putc('\n');
    line[i] = '\0';
    running(line);
  }
}

#endif
