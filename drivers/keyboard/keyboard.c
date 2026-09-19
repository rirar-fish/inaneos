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

static const char hex_digits[] = "0123456789ABCDEF";

static void serial_putc(char c) {
  while (!(inb(0x3FD) & 0x20));
  outb(0x3F8, c);
}

static void print_hex_byte(unsigned char b) {
  serial_putc(hex_digits[(b >> 4) & 0xF]);
  serial_putc(hex_digits[b & 0xF]);
  serial_putc(' ');
}

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
  char c = buf[tail];
  tail = (tail + 1) % BUFSIZE;
  return c;
}

void keyboard_handler(void) {
  unsigned char sc = inb(0x60);
  // TODO: picks shift keys
  print_hex_byte(sc);
  if (!(sc & 0x80)) {
    char c = kbd_map[sc];
    if (c)
      push(c);
  }
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
