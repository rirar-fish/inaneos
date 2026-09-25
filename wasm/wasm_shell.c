// wasm entry: browser preview of inaneos shell (calc REPL).
// Reuses user/expr.c so the browser runs the same evaluator as calc.
// JS API (same as resolve branch): kernel_main(), shell_handle_key(c).
// JS import: env.js_putchar(c), env.memory (see web/index.html).
#include "expr.h"

// provided by JS
extern void js_putchar(int c);

static void put_s(const char *s) {
  while (*s)
    js_putchar(*s++);
}

static void put_n(long v) {
  char b[24];
  int i = 0;
  unsigned long u;
  if (v < 0) {
    js_putchar('-');
    u = (unsigned long)(-(v + 1)) + 1;
  } else {
    u = (unsigned long)v;
  }
  if (u == 0) {
    js_putchar('0');
    return;
  }
  while (u && i < (int)sizeof(b)) {
    b[i++] = (char)('0' + u % 10);
    u /= 10;
  }
  while (i--)
    js_putchar(b[i]);
}

static char line[128];
static int len = 0;

static void prompt(void) {
  put_s("calc> ");
}

__attribute__((export_name("kernel_main"))) void kernel_main(void) {
  len = 0;
  put_s("inaneos wasm preview\n");
  put_s("type an integer expression, e.g. 2+3*4\n");
  prompt();
}

__attribute__((export_name("shell_handle_key"))) void shell_handle_key(int c) {
  if (c == '\r')
    c = '\n';
  if (c == '\n') {
    int ok = 1;
    long v;
    js_putchar('\n');
    line[len] = '\0';
    if (len > 0) {
      v = expr_eval(line, &ok);
      if (ok) {
        put_s("= ");
        put_n(v);
        js_putchar('\n');
      } else {
        put_s("err: bad expression\n");
      }
    }
    len = 0;
    prompt();
    return;
  }
  if (c == 8 || c == 127) {
    if (len > 0) {
      len--;
      js_putchar(8);
    }
    return;
  }
  if (c >= 32 && c < 127 && len < (int)sizeof(line) - 1) {
    line[len++] = (char)c;
    js_putchar(c);
  }
}
