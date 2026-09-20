// io.h
#pragma once
#include <stdint.h>

#ifdef __wasm__

static inline void outb(unsigned short port, unsigned char val) {
    (void)port;
    (void)val;
}

static inline unsigned char inb(unsigned short port) {
    (void)port;
    return 0;
}

static inline void sti(void) {}
static inline void cli(void) {}
static inline void io_wait(void) {}
static inline void halt(void) {}
#else

static inline void outb(unsigned short port, unsigned char val) {
  __asm__ volatile("outb %0, %1" ::"a"(val), "Nd"(port));
}

static inline unsigned char inb(unsigned short port) {
  unsigned char ret;
  __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
  return ret;
}

static inline void sti() { __asm__ volatile("sti"); }
static inline void cli() { __asm__ volatile("cli"); }

static inline void io_wait() {
  // regardless the value
  // it will create a tiny delay to wait
  // the io work to be finished
  __asm__ volatile("outb %%al, $0x80" : : "a"(0));
}

static inline void halt(void) {
    __asm__ volatile("hlt");
}

#endif
