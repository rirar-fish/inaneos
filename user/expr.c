// integer expression eval
#include "expr.h"

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

long expr_eval(const char *s, int *ok) {
  long v;
  pos = s;
  v = parse_expr(ok);
  skip();
  if (*pos)
    *ok = 0;
  return v;
}
