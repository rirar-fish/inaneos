#include "shell.h"
#include "io.h"
#include "keyboard.h"
#include "vga.h"

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
    outb(0x64, 0xFE);
    for (;;)
      __asm__ volatile("hlt");
  } else {
    // TODO: add more cmds
    term_puts(line);
    term_puts(": Command Not Found\n");
  }
}

void shell_run(void) {
  char line[128];
  // FIXME: check long input

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
