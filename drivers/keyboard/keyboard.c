// modules

#include "keyboard.h"
#include "io.h"
#include "vga.h"

static const char kbd_map[128] = {
    0,   27,  '1',  '2',  '3',  '4', '5', '6',  '7', '8', '9', '0',
    '-', '=', '\b', '\t', 'q',  'w', 'e', 'r',  't', 'y', 'u', 'i',
    'o', 'p', '[',  ']',  '\n', 0,   'a', 's',  'd', 'f', 'g', 'h',
    'j', 'k', 'l',  ';',  '\'', '`', 0,   '\\', 'z', 'x', 'c', 'v',
    'b', 'n', 'm',  ',',  '.',  '/', 0,   '*',  0,   ' ', 0,
};

// shifted layer, us layout
static const char shift_map[128] = {
    0,   27,  '!',  '@',  '#',  '$', '%', '^',  '&', '*', '(', ')',
    '_', '+', '\b', '\t', 'Q',  'W', 'E', 'R',  'T', 'Y', 'U', 'I',
    'O', 'P', '{',  '}',  '\n', 0,   'A',  'S',  'D', 'F', 'G', 'H',
    'J', 'K', 'L',  ':',  '"',  '~', 0,   '|',  'Z', 'X', 'C', 'V',
    'B', 'N', 'M',  '<',  '>',  '?', 0,   '*',  0,   ' ', 0,
};

#define BUFSIZE 32
static volatile char buf[BUFSIZE];
static volatile int head = 0, tail = 0;

static void push(char c) {
  int next = (head + 1) % BUFSIZE;
  // FIXME: drop when full
  if (next == tail)
    return;
  buf[head] = c;
  head = next;
}

int getchar(void) {
  while (tail == head)
    __asm__ volatile("hlt");
  unsigned char c = buf[tail];
  tail = (tail + 1) % BUFSIZE;
  return c;
}

static int e0;
static int shift;
static int caps;

// pure decode, host-testable
// out: 0 drop, else char (or KEY_*)
int kbd_decode(unsigned char sc, int *e0s, int *shifts, int *capsv) {
  if (sc == 0xE0) {
    *e0s = 1;
    return 0;
  }
  if (sc == 0x2A || sc == 0x36) {
    *shifts = 1;
    return 0;
  }
  if (sc == 0xAA || sc == 0xB6) {
    *shifts = 0;
    return 0;
  }
  if (sc == 0x3A) { // caps toggle
    *capsv = !*capsv;
    return 0;
  }
  if (*e0s) {
    *e0s = 0;
    if (sc & 0x80)
      return 0;
    if (sc == 0x48)
      return KEY_UP;
    if (sc == 0x50)
      return KEY_DOWN;
    if (sc == 0x4B)
      return KEY_LEFT;
    if (sc == 0x4D)
      return KEY_RIGHT;
    if (sc == 0x53)
      return KEY_DEL;
    return 0;
  }
  if (sc >= 128 || (sc & 0x80))
    return 0;
  {
    char base = kbd_map[sc];
    if (!base)
      return 0;
    if (base >= 'a' && base <= 'z')
      return ((*shifts != 0) != (*capsv != 0)) ? base - 32 : base;
    return *shifts ? shift_map[sc] : base;
  }
}

void keyboard_handler(void) {
  unsigned char sc = inb(0x60);
  int c = kbd_decode(sc, &e0, &shift, &caps);
  if (c)
    push((char)c);
  outb(0x20, 0x20);
}

__attribute__((naked)) void irq1_stub(void) {
  __asm__ volatile("push %rax\n"
                   "push %rcx\n"
                   "push %rdx\n"
                   "push %rbx\n"
                   "push %rbp\n"
                   "push %rsi\n"
                   "push %rdi\n"
                   "push %r8\n"
                   "push %r9\n"
                   "push %r10\n"
                   "push %r11\n"
                   "push %r12\n"
                   "push %r13\n"
                   "push %r14\n"
                   "push %r15\n"
                   "call keyboard_handler\n"
                   "pop %r15\n"
                   "pop %r14\n"
                   "pop %r13\n"
                   "pop %r12\n"
                   "pop %r11\n"
                   "pop %r10\n"
                   "pop %r9\n"
                   "pop %r8\n"
                   "pop %rdi\n"
                   "pop %rsi\n"
                   "pop %rbp\n"
                   "pop %rbx\n"
                   "pop %rdx\n"
                   "pop %rcx\n"
                   "pop %rax\n"
                   "iretq\n");
}
