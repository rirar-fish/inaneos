// keo editor
#include "fs.h"
#include "keyboard.h"
#include "syscall.h"

#define MAXL 256
#define MAXC 256
#define TXT_ROWS 23
#define ROW_STAT 23
#define ROW_CMD 24

static char lines[MAXL][MAXC];
static int lens[MAXL];
static int nlines = 1;
static int cx, cy, top;
static int dirty;
static char fname[128];
static int mode; // 0 normal, 1 insert, 2 cmd
static char cmd[128];
static int cmdlen;
static char msg[80];

static int min2(int a, int b) { return a < b ? a : b; }

static void clamp(void) {
  if (cy < 0)
    cy = 0;
  if (cy > nlines - 1)
    cy = nlines - 1;
  if (cx < 0)
    cx = 0;
  if (cx > lens[cy])
    cx = lens[cy];
  if (top > cy)
    top = cy;
  if (top < cy - (TXT_ROWS - 1))
    top = cy - (TXT_ROWS - 1);
  if (top < 0)
    top = 0;
}

static int emit_dec(char *b, int pos, long v) {
  char tmp[20];
  int i = 0;
  unsigned long u = v < 0 ? (unsigned long)(-(v + 1)) + 1 : (unsigned long)v;
  if (v < 0)
    b[pos++] = '-';
  if (!u)
    tmp[i++] = '0';
  while (u) {
    tmp[i++] = '0' + (u % 10);
    u /= 10;
  }
  while (i--)
    b[pos++] = tmp[i];
  return pos;
}

static void draw(void) {
  char row[80];
  int n, r;
  clamp();
  for (r = 0; r < TXT_ROWS; r++) {
    int li = top + r;
    n = 0;
    sys_setcolor(0x0F, 0x00);
    if (li < nlines) {
      int m = min2(lens[li], 80);
      for (int i = 0; i < m; i++)
        row[n++] = lines[li][i];
    } else {
      row[n++] = '~';
    }
    while (n < 80)
      row[n++] = ' ';
    sys_goto(r, 0);
    sys_write(row, 80);
  }
  sys_setcolor(0x00, 0x07);
  n = 0;
  const char *mn = mode == 0 ? "NORMAL" : mode == 1 ? "INSERT" : "CMD";
  row[n++] = 'k';
  row[n++] = 'e';
  row[n++] = 'o';
  row[n++] = ' ';
  for (int i = 0; fname[i] && n < 40; i++)
    row[n++] = fname[i];
  if (dirty && n < 80)
    row[n++] = '*';
  while (n < 52)
    row[n++] = ' ';
  n = emit_dec(row, n, cy + 1);
  row[n++] = ',';
  n = emit_dec(row, n, cx + 1);
  row[n++] = ' ';
  for (int i = 0; mn[i] && n < 80; i++)
    row[n++] = mn[i];
  while (n < 80)
    row[n++] = ' ';
  sys_goto(ROW_STAT, 0);
  sys_write(row, 80);
  sys_setcolor(0x0F, 0x00);
  n = 0;
  if (mode == 2) {
    row[n++] = ':';
    for (int i = 0; i < cmdlen && n < 80; i++)
      row[n++] = cmd[i];
  } else {
    for (int i = 0; msg[i] && n < 80; i++)
      row[n++] = msg[i];
  }
  while (n < 80)
    row[n++] = ' ';
  sys_goto(ROW_CMD, 0);
  sys_write(row, 80);
  if (mode == 2)
    sys_goto(ROW_CMD, min2(1 + cmdlen, 79));
  else
    sys_goto(cy - top, min2(cx, 79));
}

static int save(void) {
  static char buf[FILE_MAX];
  unsigned long pos = 0;
  long r;
  if (!fname[0])
    return -99;
  for (int i = 0; i < nlines; i++) {
    if (pos + (unsigned long)lens[i] + 1 > FILE_MAX)
      return FS_FULL;
    for (int j = 0; j < lens[i]; j++)
      buf[pos++] = lines[i][j];
    buf[pos++] = '\n';
  }
  r = sys_fwrite(fname, buf, pos);
  if (r == 0)
    dirty = 0;
  return (int)r;
}

static void load(void) {
  static char buf[FILE_MAX];
  long n = sys_fread(fname, buf, FILE_MAX);
  int li = 0;
  nlines = 1;
  lens[0] = 0;
  cx = cy = top = 0;
  dirty = 0;
  mode = 0;
  msg[0] = '\0';
  if (n <= 0)
    return;
  for (long i = 0; i < n; i++) {
    if (buf[i] == '\n') {
      if (li + 1 < MAXL) {
        li++;
        lens[li] = 0;
      }
    } else if (lens[li] < MAXC - 1) {
      lines[li][lens[li]++] = buf[i];
    }
  }
  nlines = li + 1;
  if (n > 0 && buf[n - 1] == '\n' && nlines > 1)
    nlines--; // trailing newline is terminator
}

static int same(const char *a, const char *b) {
  while (*a && *a == *b) {
    a++;
    b++;
  }
  return *a == *b;
}

static void set_msg(const char *s) {
  int i = 0;
  while (s[i] && i < 79) {
    msg[i] = s[i];
    i++;
  }
  msg[i] = '\0';
}

static void exec_cmd(void); // fwd

static long do_save(void) {
  long r = save();
  if (r == 0)
    sys_exit(); // auto close like nano flow
  else if (r == FS_FULL)
    set_msg("too big");
  else if (r == FS_RO)
    set_msg("read-only");
  else
    set_msg("cannot save");
  return r;
}

static void exec_cmd(void) {
  if (same(cmd, "w")) {
    do_save();
  } else if (cmd[0] == 'w' && cmd[1] == ' ') {
    int i = 0;
    while (cmd[2 + i] && i < 127) {
      fname[i] = cmd[2 + i];
      i++;
    }
    fname[i] = '\0';
    do_save();
  } else if (same(cmd, "q")) {
    if (dirty)
      set_msg("unsaved, use q!");
    else
      sys_exit();
  } else if (same(cmd, "q!")) {
    sys_exit();
  } else if (same(cmd, "wq")) {
    if (do_save() == 0)
      sys_exit();
  } else {
    set_msg("bad cmd");
  }
}

static void del_char(void) {
  if (cx < lens[cy]) {
    for (int i = cx; i + 1 < lens[cy]; i++)
      lines[cy][i] = lines[cy][i + 1];
    lens[cy]--;
    dirty = 1;
  }
}

static void del_line_at(int at) {
  for (int i = at; i + 1 < nlines; i++) {
    for (int j = 0; j < lens[i + 1]; j++)
      lines[i][j] = lines[i + 1][j];
    lens[i] = lens[i + 1];
  }
  nlines--;
}

static void del_line(void) {
  if (nlines <= 1) {
    lens[0] = 0;
    cx = 0;
    dirty = 1;
    return;
  }
  del_line_at(cy);
  dirty = 1;
}

static void open_line(int below) {
  if (nlines >= MAXL) {
    set_msg("full");
    return;
  }
  int at = below ? cy + 1 : cy;
  for (int i = nlines; i > at; i--) {
    for (int j = 0; j < lens[i - 1]; j++)
      lines[i][j] = lines[i - 1][j];
    lens[i] = lens[i - 1];
  }
  lens[at] = 0;
  nlines++;
  cy = at;
  cx = 0;
  dirty = 1;
  mode = 1;
}

static void normal_key(long c) {
  static int pend_d, pend_g;
  if (c == 'h' || c == KEY_LEFT) {
    if (cx > 0)
      cx--;
  } else if (c == 'l' || c == KEY_RIGHT) {
    if (cx < lens[cy])
      cx++;
  } else if (c == 'j' || c == KEY_DOWN) {
    if (cy < nlines - 1)
      cy++;
  } else if (c == 'k' || c == KEY_UP) {
    if (cy > 0)
      cy--;
  } else if (c == 'i') {
    mode = 1;
  } else if (c == 'a') {
    if (cx < lens[cy])
      cx++;
    mode = 1;
  } else if (c == 'A') {
    cx = lens[cy];
    mode = 1;
  } else if (c == 'o') {
    open_line(1);
  } else if (c == 'O') {
    open_line(0);
  } else if (c == 'x' || c == KEY_DEL) {
    del_char();
  } else if (c == 'd') {
    if (pend_d) {
      del_line();
      pend_d = 0;
    } else {
      pend_d = 1;
      return;
    }
  } else if (c == 'g') {
    if (pend_g) {
      cy = 0;
      pend_g = 0;
    } else {
      pend_g = 1;
      return;
    }
  } else if (c == 'G') {
    cy = nlines - 1;
  } else if (c == '0') {
    cx = 0;
  } else if (c == '$') {
    cx = lens[cy] ? lens[cy] - 1 : 0;
  } else if (c == ':') {
    mode = 2;
    cmdlen = 0;
    cmd[0] = '\0';
  }
  pend_d = (c == 'd') ? pend_d : 0;
  pend_g = (c == 'g') ? pend_g : 0;
}

static void insert_key(long c) {
  if (c == 27) {
    mode = 0;
    if (cx > lens[cy])
      cx = lens[cy];
    return;
  }
  if (c == KEY_UP || c == KEY_DOWN || c == KEY_LEFT || c == KEY_RIGHT) {
    normal_key(c);
    return;
  }
  if (c == '\n') {
    if (nlines >= MAXL) {
      set_msg("full");
      return;
    }
    for (int i = nlines; i > cy + 1; i--) {
      for (int j = 0; j < lens[i - 1]; j++)
        lines[i][j] = lines[i - 1][j];
      lens[i] = lens[i - 1];
    }
    for (int j = cx; j < lens[cy]; j++)
      lines[cy + 1][j - cx] = lines[cy][j];
    lens[cy + 1] = lens[cy] - cx;
    lens[cy] = cx;
    nlines++;
    cy++;
    cx = 0;
    dirty = 1;
    return;
  }
  if (c == '\b') {
    if (cx > 0) {
      cx--;
      del_char();
    } else if (cy > 0) {
      int pl = lens[cy - 1];
      if (pl + lens[cy] < MAXC) {
        for (int j = 0; j < lens[cy]; j++)
          lines[cy - 1][pl + j] = lines[cy][j];
        lens[cy - 1] = pl + lens[cy];
        del_line_at(cy);
        cy--;
        cx = pl;
        dirty = 1;
      }
    }
    return;
  }
  if (c >= 32 && c < 127 && lens[cy] < MAXC - 1) {
    for (int j = lens[cy]; j > cx; j--)
      lines[cy][j] = lines[cy][j - 1];
    lines[cy][cx++] = (char)c;
    lens[cy]++;
    dirty = 1;
  }
}

static void cmd_key(long c) {
  if (c == 27) {
    mode = 0;
    return;
  }
  if (c == '\n') {
    mode = 0;
    exec_cmd();
    return;
  }
  if (c == '\b') {
    if (cmdlen > 0) {
      cmdlen--;
      cmd[cmdlen] = '\0';
    }
    return;
  }
  if (c >= 32 && c < 127 && cmdlen < 127) {
    cmd[cmdlen++] = (char)c;
    cmd[cmdlen] = '\0';
  }
}

void user_main(int argc, char **argv) {
  if (argc < 2 || !argv[1] || !argv[1][0]) {
    sys_puts("usage: keo file\n");
    sys_exit();
  }
  int i = 0;
  while (argv[1][i] && i < 127) {
    fname[i] = argv[1][i];
    i++;
  }
  fname[i] = '\0';
  load();
  draw();
  for (;;) {
    long c = sys_getc();
    msg[0] = '\0';
    if (mode == 0)
      normal_key(c);
    else if (mode == 1)
      insert_key(c);
    else
      cmd_key(c);
    draw();
  }
}
