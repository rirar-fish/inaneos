#include "shell.h"
#include "io.h"
#include "keyboard.h"
#include "vga.h"

#define MAX_USERS 8
#define MAX_LEN 32

#define FS_MAX 64
#define NAME_MAX 32
#define MAX_FILE_SIZE 512

typedef struct {
  char username[MAX_LEN];
  char password[MAX_LEN];
  int used;
} user_t;

static user_t users[MAX_USERS];
static int user_count = 0;
static int is_logged = 0;
static char current_user[MAX_LEN];

typedef struct {
  char name[NAME_MAX];
  char content[MAX_FILE_SIZE];
  int parent;
  int is_dir;
  int used;
  int mode;
} fs_node_t;

static fs_node_t nodes[FS_MAX];

static int cwd = 0;

static int same_condition(const char *a, const char *b) {
  while (*a && *b) {
    if (*a != *b)
      return 0;

    a++;
    b++;
  }

  return *a == '\0' && *b == '\0';
}

static int starts_with(const char *s, const char *pre) {
  while (*pre)
    if (*s++ != *pre++)
      return 0;
  return 1;
}

static int slen(const char *s) {
  int n = 0;
  while (s[n])
    n++;
  return n;
}

static const char *skip_sp(const char *s) {
  while (*s == ' ' || *s == '\t')
    s++;
  return s;
}

static void trim_end(char *s) {
  int n = slen(s);
  while (n > 0) {
    char c = s[n - 1];

    if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
      s[n - 1] = '\0';
      n--;
    } else {
      break;
    }
  }
}

static void str_copy(char *dst, const char *src, int max) {
  int i = 0;

  if (max <= 0)
    return;

  while (src[i] && i < max - 1) {
    dst[i] = src[i];
    i++;
  }

  dst[i] = '\0';
}

static void name_cp(char *dst, const char *src) {
  str_copy(dst, src, NAME_MAX);
}

static void read_line(char *buf, int max) {
  int i = 0;

  if (max <= 0)
    return;

  for (;;) {
    char c = (char)getchar();

    if (c == '\n' || c == '\r') {
      buf[i] = '\0';
      term_putc('\n');
      return;
    }

    if (c == '\b') {
      if (i > 0) {
        i--;
        term_putc('\b');
      }
      continue;
    }

    if (c >= 32 && c < 127) {
      if (i < max - 1) {
        buf[i++] = c;
        term_putc(c);
      }
    }
  }
}

static void read_password(char *buf, int max) {
  int i = 0;
  for (;;) {
    char c = (char)getchar();
    if (c == '\n' || c == '\r') {
      break;
    } else if (c == '\b') {
      if (i > 0) {
        i--;
        term_putc('\b');
      }
    } else if (c >= 32 && c < 127 && i < max - 1) {
      buf[i++] = c;
      term_putc('*');
    }
  }
  term_putc('\n');
  buf[i] = '\0';
}

static int find_user(const char *username) {
  for (int i = 0; i < user_count; i++) {
    if (users[i].used && same_condition(users[i].username, username)) {
      return i;
    }
  }
  return -1;
}

static void cmd_register(void) {
  if (user_count >= MAX_USERS) {
    term_puts("Register Failed: User limited reached, try again");
    term_puts("\n");

    return;
  }

  char uname[MAX_LEN];
  char pass[MAX_LEN];
  char pass2[MAX_LEN];

  term_puts("Username: ");
  read_line(uname, MAX_LEN);

  if (uname[0] == '\0') {
    term_puts("The username cannot be empty");
    term_puts("\n");

    return;
  }
  if (find_user(uname) != -1) {
    term_puts("Register Failed: Username already exits, try again");
    term_puts("\n");

    return;
  }

  term_puts("Password: ");
  read_password(pass, MAX_LEN);

  term_puts("Confirm Password: ");
  read_password(pass2, MAX_LEN);

  if (!same_condition(pass, pass2)) {
    term_puts("Register failed: passwords doesn't match");
    term_puts("\n");

    return;
  }

  str_copy(users[user_count].username, uname, MAX_LEN);
  str_copy(users[user_count].password, pass, MAX_LEN);
  users[user_count].used = 1;
  user_count++;

  term_puts("Register success, You can login now");
  term_puts("\n");
}

static void cmd_login(void) {
  if (is_logged) {
    term_puts("Already login as");
    term_puts(current_user);
    term_puts("\n");
    return;
  }

  char uname[MAX_LEN];
  char pass[MAX_LEN];

  term_puts("Username: ");
  read_line(uname, MAX_LEN);

  term_puts("Password: ");
  read_line(pass, MAX_LEN);

  int idx = find_user(uname);
  if (idx == -1 || !same_condition(users[idx].password, pass)) {
    term_puts("Login failed: invalid username or password");
    term_puts("\n");
    ;
    return;
  }
  is_logged = 1;
  str_copy(current_user, uname, MAX_LEN);

  term_puts("Login success, welcome,");
  term_puts(current_user);
  term_puts("\n");
}

static void cmd_logout() {
  if (is_logged) {
    term_puts("Bye, ");
    term_puts(current_user);
    term_puts("\n");

    is_logged = 0;
    current_user[0] = '\0';
  } else {
    term_puts("No user is logged in\n");
    term_puts("\n");
  }
}

static void fs_init(void) {
  for (int i = 0; i < FS_MAX; i++) {
    nodes[i].used = 0;
    nodes[i].content[0] = '\0';
  }

  nodes[0].name[0] = '\0';
  nodes[0].parent = -1;
  nodes[0].is_dir = 1;
  nodes[0].used = 1;
  nodes[0].mode = 0755;

  cwd = 0;
}

static int free_slot(void) {
  for (int i = 0; i < FS_MAX; i++) {
    if (!nodes[i].used)
      return i;
  }

  return -1;
}

static int find_kid(int dir, const char *name) {
  for (int i = 0; i < FS_MAX; i++) {
    if (!nodes[i].used || nodes[i].parent != dir)
      continue;
    if (same_condition(nodes[i].name, name))
      return i;
  }
  return -1;
}

static int next_part(const char *p, char *out) {
  int i = 0;
  while (*p == '/')
    p++;
  while (p[i] && p[i] != '/' && i < NAME_MAX - 1) {
    out[i] = p[i];
    i++;
  }
  out[i] = '\0';
  return i;
}

// resolve path with ., ..
static int resolve(const char *path, int *out) {
  int cur = (*path == '/') ? 0 : cwd;
  const char *p = path;
  char part[NAME_MAX];

  if (*p == '\0' || same_condition(p, ".")) {
    *out = cur;
    return 0;
  }
  while (*p) {
    while (*p == '/')
      p++;
    if (*p == '\0')
      break;
    int n = next_part(p, part);
    p += n;
    while (*p == '/')
      p++;
    if (same_condition(part, "."))
      continue;
    if (same_condition(part, "..")) {
      if (nodes[cur].parent >= 0)
        cur = nodes[cur].parent;
      continue;
    }
    int k = find_kid(cur, part);
    if (k < 0)
      return -1;
    if (*p && !nodes[k].is_dir)
      return -1;
    cur = k;
  }
  *out = cur;
  return 0;
}

static int resolve_dir(const char *path, int *out) {
  int d;
  if (resolve(path, &d) != 0 || !nodes[d].is_dir)
    return -1;
  *out = d;
  return 0;
}

static void get_path(int idx, char *out, int max) {
  int chain[FS_MAX];
  int count = 0;
  int cur = idx;
  int pos = 0;

  if (max <= 0)
    return;

  if (idx == 0) {
    str_copy(out, "/", max);
    return;
  }

  while (cur != 0 && cur >= 0 && count < FS_MAX) {
    chain[count++] = cur;
    cur = nodes[cur].parent;
  }

  if (pos < max - 1)
    out[pos++] = '/';

  for (int i = count - 1; i >= 0; i--) {
    int j = 0;

    while (nodes[chain[i]].name[j] && pos < max - 1) {
      out[pos++] = nodes[chain[i]].name[j++];
    }

    if (i > 0 && pos < max - 1)
      out[pos++] = '/';
  }

  out[pos] = '\0';
}

static void cmd_pwd(void) {
  char path[256];

  get_path(cwd, path, sizeof(path));

  term_puts(path);
  term_puts("\n");
}

static void cmd_ls(char *arg) {
  arg = (char *)skip_sp(arg);
  trim_end(arg);

  int dir = cwd;

  if (*arg) {
    if (resolve_dir(arg, &dir) != 0) {
      term_puts("ls: bad path\n");
      return;
    }
  }

  int found = 0;

  for (int i = 0; i < FS_MAX; i++) {
    if (!nodes[i].used)
      continue;

    if (nodes[i].parent != dir)
      continue;

    term_puts(nodes[i].name);

    if (nodes[i].is_dir)
      term_puts("/");

    term_puts("\n");

    found = 1;
  }

  if (!found) {
  }
}

static void cmd_cd(char *arg) {
  arg = (char *)skip_sp(arg);
  trim_end(arg);

  int dir;

  if (*arg == '\0') {
    cwd = 0;
    return;
  }

  if (resolve_dir(arg, &dir) != 0) {
    term_puts("cd: bad path\n");
    return;
  }

  cwd = dir;
}

static void cmd_mkdir(char *arg) {
  char leaf[NAME_MAX];

  int pdir;

  arg = (char *)skip_sp(arg);
  trim_end(arg);

  if (*arg == '\0') {
    term_puts("mkdir: missing operand\n");
    return;
  }

  if (slen(arg) >= 128) {
    term_puts("mkdir: path too long\n");
    return;
  }

  int last = -1;

  for (int i = 0; arg[i]; i++) {
    if (arg[i] == '/')
      last = i;
  }

  if (last >= 0) {
    char pbuf[128];

    int i = 0;

    while (i < last && i < 127) {
      pbuf[i] = arg[i];
      i++;
    }

    pbuf[i] = '\0';

    if (pbuf[0] == '\0') {
      str_copy(pbuf, "/", sizeof(pbuf));
    }

    if (resolve_dir(pbuf, &pdir) != 0) {
      term_puts("mkdir: bad path\n");
      return;
    }

    next_part(arg + last + 1, leaf);
  } else {
    pdir = cwd;
    next_part(arg, leaf);
  }

  if (leaf[0] == '\0' || same_condition(leaf, ".") ||
      same_condition(leaf, "..")) {

    term_puts("mkdir: bad name\n");
    return;
  }

  if (find_kid(pdir, leaf) >= 0) {
    term_puts("mkdir: already exists\n");
    return;
  }

  int fr = free_slot();

  if (fr < 0) {
    term_puts("mkdir: filesystem full\n");
    return;
  }

  name_cp(nodes[fr].name, leaf);

  nodes[fr].parent = pdir;
  nodes[fr].is_dir = 1;
  nodes[fr].used = 1;
  nodes[fr].mode = 0755;
  nodes[fr].content[0] = '\0';

  term_puts("Directory created\n");
}

static void cmd_touch(const char *filename) {
  char path[128];
  char leaf[NAME_MAX];

  int pdir;

  filename = skip_sp(filename);

  if (*filename == '\0') {
    term_puts("touch: missing filename\n");
    return;
  }

  str_copy(path, filename, sizeof(path));
  trim_end(path);

  int existing;

  if (resolve(path, &existing) == 0) {
    if (nodes[existing].is_dir) {
      term_puts("touch: target is a directory\n");
    } else {
      term_puts("touch: file already exists\n");
    }

    return;
  }

  int last = -1;

  for (int i = 0; path[i]; i++) {
    if (path[i] == '/')
      last = i;
  }

  if (last >= 0) {
    char parent_path[128];

    int i = 0;

    while (i < last && i < 127) {
      parent_path[i] = path[i];
      i++;
    }

    parent_path[i] = '\0';

    if (parent_path[0] == '\0') {
      str_copy(parent_path, "/", sizeof(parent_path));
    }

    if (resolve_dir(parent_path, &pdir) != 0) {
      term_puts("touch: bad path\n");
      return;
    }

    next_part(path + last + 1, leaf);
  } else {
    pdir = cwd;
    next_part(path, leaf);
  }

  if (leaf[0] == '\0') {
    term_puts("touch: bad filename\n");
    return;
  }

  if (find_kid(pdir, leaf) >= 0) {
    term_puts("touch: target already exists\n");
    return;
  }

  int fr = free_slot();

  if (fr < 0) {
    term_puts("touch: filesystem full\n");
    return;
  }

  name_cp(nodes[fr].name, leaf);

  nodes[fr].parent = pdir;
  nodes[fr].is_dir = 0;
  nodes[fr].used = 1;
  nodes[fr].mode = 0644;
  nodes[fr].content[0] = '\0';

  term_puts("File created\n");
}

static void cmd_cat(const char *filename) {
  filename = skip_sp(filename);

  if (*filename == '\0') {
    term_puts("Vat: missing filename\n");
    return;
  }

  char path[128];

  str_copy(path, filename, sizeof(path));
  trim_end(path);

  int idx;

  if (resolve(path, &idx) != 0) {
    term_puts("Cat: file not found\n");
    return;
  }

  if (nodes[idx].is_dir) {
    term_puts("Cat: target is a directory\n");
    return;
  }

  term_puts(nodes[idx].content);
  int len = slen(nodes[idx].content);

  if (len > 0 && nodes[idx].content[len - 1] != '\n') {
    term_puts("\n");
  }
}

static void cmd_move(char *args) {
  args = (char *)skip_sp(args);
  trim_end(args);

  if (*args == '\0') {
    term_puts("move: usage: move source destination\n");
    return;
  }

  char source[128];
  char destination[128];

  int i = 0;

  while (args[i] && args[i] != ' ' && args[i] != '\t' && i < 127) {

    source[i] = args[i];
    i++;
  }

  source[i] = '\0';

  args += i;

  args = (char *)skip_sp(args);

  if (*args == '\0') {
    term_puts("move: missing destination\n");
    return;
  }

  str_copy(destination, args, sizeof(destination));

  trim_end(destination);

  int src;

  if (resolve(source, &src) != 0) {
    term_puts("move: source not found\n");
    return;
  }

  if (nodes[src].is_dir) {
    term_puts("move: moving directories is not supported\n");
    return;
  }

  int dest_dir;

  if (resolve_dir(destination, &dest_dir) != 0) {
    term_puts("move: destination directory not found\n");
    return;
  }

  if (find_kid(dest_dir, nodes[src].name) >= 0) {

    term_puts("move: file already exists there\n");
    return;
  }

  nodes[src].parent = dest_dir;

  term_puts("File moved\n");
}

static void cmd_echo(const char *text) {
  text = skip_sp(text);

  term_puts(text);
  term_puts("\n");
}

static void cmd_clear(void) { term_clear(); }

static void cmd_help(void) {
  term_puts("Available commands:\n");
  term_puts("  help                 - show this command list\n");
  term_puts("  ls [path]            - list directory contents\n");
  term_puts("  cd [path]            - change directory\n");
  term_puts("  pwd                  - show current directory\n");
  term_puts("  mkdir <name>         - create directory\n");
  term_puts("  touch <file>         - create file\n");
  term_puts("  cat <file>           - show file contents\n");
  term_puts("  move <file> <dir>    - move file\n");
  term_puts("  echo <text>          - print text\n");
  term_puts("  clear                - clear screen\n");
  term_puts("  register             - create user\n");
  term_puts("  login                - login user\n");
  term_puts("  logout               - logout current user\n");
  term_puts("  info                 - show OS information\n");
  term_puts("  reboot               - restart system\n");
}

static void running(char *line) {
  if (line[0] == '\0')
    return;

  if (same_condition(line, "help")) {
    cmd_help();
  }

  else if (same_condition(line, "ls")) {
    cmd_ls("");
  }

  else if (starts_with(line, "ls ")) {
    cmd_ls(line + 3);
  }

  else if (same_condition(line, "cd")) {
    cmd_cd("");
  }

  else if (starts_with(line, "cd ")) {
    cmd_cd(line + 3);
  }

  else if (same_condition(line, "pwd")) {
    cmd_pwd();
  }

  else if (same_condition(line, "mkdir")) {
    cmd_mkdir("");
  }

  else if (starts_with(line, "mkdir ")) {
    cmd_mkdir(line + 6);
  }

  else if (same_condition(line, "touch")) {
    cmd_touch("");
  }

  else if (starts_with(line, "touch ")) {
    cmd_touch(line + 6);
  }

  else if (same_condition(line, "cat")) {
    cmd_cat("");
  }

  else if (starts_with(line, "cat ")) {
    cmd_cat(line + 4);
  }

  else if (same_condition(line, "move")) {
    cmd_move("");
  }

  else if (starts_with(line, "move ")) {
    cmd_move(line + 5);
  }

  else if (same_condition(line, "echo")) {
    cmd_echo("");
  }

  else if (starts_with(line, "echo ")) {
    cmd_echo(line + 5);
  } else if (same_condition(line, "register")) {
    cmd_register();
  } else if (same_condition(line, "login")) {
    cmd_login();
  } else if (same_condition(line, "logout")) {
    cmd_logout();
  } else if (same_condition(line, "clear")) {
    cmd_clear();
  } else if (same_condition(line, "info")) {
    term_puts("Inaneos beta 0.0\n");
    term_puts("RAM filesystem shell\n");
  } else if (same_condition(line, "reboot")) {
    term_puts("Rebooting...\n");

    outb(0x64, 0xFE);

    for (;;)
      __asm__ volatile("hlt");
  } else {
    term_puts(line);
    term_puts(": Command Not Found\n");
  }
}

void shell_run(void) {
  char line[128];
  fs_init();

  // main loop: prompt, read, exec
  for (;;) {

    term_set_color(0x0A, 0x00);

    if (is_logged)
      term_puts(current_user);
    else
      term_puts("root");

    term_puts("@inaneos:");

    term_set_color(0x0F, 0x00);

    char path[256];
    get_path(cwd, path, sizeof(path));
    term_puts(path);

    term_set_color(0x0A, 0x00);
    term_puts("# ");

    term_set_color(0x0F, 0x00);

    int i = 0;

    for (;;) {
      char c = (char)getchar();

      if (c == '\n' || c == '\r') {
        break;
      }

      if (c == '\b') {
        if (i > 0) {
          i--;
          term_putc('\b');
        }

        continue;
      }

      if (c >= 32 && c < 127 && i < (int)sizeof(line) - 1) {
        line[i++] = c;
        term_putc(c);
      }
    }

    line[i] = '\0';

    term_putc('\n');

    running(line);
  }
}
