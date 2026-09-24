// calc tool
#include "expr.h"
#include "syscall.h"

static const char *pos;

static void skip(void) {
  while (*pos == ' ' || *pos == '\t')
    pos++;
}

static long parse_expr(int *ok);

static long parse_num(int *ok) {
  long v = 0;
  int digits = 0;
  skip();
  while (*pos >= '0' && *pos <= '9') {
    v = v * 10 + (*pos - '0');
    pos++;
    digits++;
  }
  if (!digits)
    *ok = 0;
  return v;
}

static long parse_factor(int *ok) {
  long v;
  skip();
  if (*pos == '(') {
    pos++;
    v = parse_expr(ok);
    skip();
    if (*pos != ')')
      *ok = 0;
    else
      pos++;
    return v;
  }
  if (*pos == '-') {
    pos++;
    return -parse_factor(ok);
  }
  return parse_num(ok);
}

static long parse_term(int *ok) {
  long v = parse_factor(ok);
  for (;;) {
    char op;
    long r;
    skip();
    op = *pos;
    if (op != '*' && op != '/' && op != '%')
      return v;
    pos++;
    r = parse_factor(ok);
    if (!*ok)
      return 0;
    if (op == '*')
      v *= r;
    else if (r == 0)
      *ok = 0;
    else if (op == '/')
      v /= r;
    else
      v %= r;
  }
}

static long parse_expr(int *ok) {
  long v = parse_term(ok);
  for (;;) {
    char op;
    long r;
    skip();
    op = *pos;
    if (op != '+' && op != '-')
      return v;
    pos++;
    r = parse_term(ok);
    if (!*ok)
      return 0;
    v = (op == '+') ? v + r : v - r;
  }
}

static void put_dec(long v) {
  char tmp[20];
  int i = 0;
  unsigned long u;
  if (v < 0) {
    sys_write("-", 1);
    u = (unsigned long)(-(v + 1)) + 1;
  } else {
    u = (unsigned long)v;
  }
  if (!u)
    tmp[i++] = '0';
  while (u) {
    tmp[i++] = '0' + (u % 10);
    u /= 10;
  }
  while (i--) {
    char c = tmp[i];
    sys_write(&c, 1);
  }
  sys_write("\n", 1);
}

static int same(const char *a, const char *b) {
  while (*a && *a == *b) {
    a++;
    b++;
  }
  return *a == *b;
}

void user_main(int argc, char **argv) {
  static char line[128];
  sys_puts("calc, exit to quit\n");
  for (;;) {
    int i = 0;
    int ok = 1;
    long v;
    sys_puts("calc> ");
    for (;;) {
      char c = (char)sys_getc();
      if (c == '\n')
        break;
      if (c == '\b') {
        if (i > 0) {
          i--;
          sys_write("\b", 1);
        }
      } else if (c >= 32 && c < 127 && i < 127) {
        line[i++] = c;
        sys_write(&c, 1);
      }
    }
    sys_write("\n", 1);
    line[i] = '\0';
    if (same(line, "exit"))
      sys_exit();
    v = expr_eval(line, &ok);
    if (!ok)
      sys_puts("err\n");
    else
      put_dec(v);
  }
}
