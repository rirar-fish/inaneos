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
  if (next == tail)
    return;
  buf[head] = c;
  head = next;
}

int getchar(void) {
  while (tail == head)
    halt();
  char c = buf[tail];
  tail = (tail + 1) % BUFSIZE;
  return c;
}

void keyboard_handler(void) {
  unsigned char sc = inb(0x60);
  print_hex_byte(sc);
  if (!(sc & 0x80)) {
    char c = kbd_map[sc];
    if (c)
      push(c);
  }
  outb(0x20, 0x20);
}

__attribute__((naked)) void irq1_stub(void) {
  __asm__ volatile("pusha\n"
                   "call keyboard_handler\n"
                   "popa\n"
                   "iret\n");
}
