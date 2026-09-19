#include "idt.h"
#include "keyboard.h"
#include "vga.h"
#include "io.h"
#include <stdint.h>

struct gate {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t zero;
} __attribute__((packed));

static struct gate idt[256];

static unsigned short current_cs(void) {
    unsigned short cs;
    __asm__ volatile("mov %%cs, %0" : "=r"(cs));
    return cs;
}

void idt_set_gate(int n, uint64_t handler) {
    idt[n].offset_low = handler & 0xFFFF;
    idt[n].selector    = current_cs(); // grub seg
    idt[n].ist         = 0;
    idt[n].type_attr   = 0x8E;
    idt[n].offset_mid  = (handler >> 16) & 0xFFFF;
    idt[n].offset_high = (handler >> 32) & 0xFFFFFFFF;
    idt[n].zero        = 0;
}

// cpu fault
// TODO: print fault number
void panic(void) {
    term_set_color(0x0C, 0x00);
    term_puts("\n*** EXCEPTION! CPU menemukan kondisi fatal ***\n");
    for (;;) __asm__ volatile("hlt");
}

__attribute__((naked))
void exception_stub(void) {
    __asm__ volatile(
        "push %rax\n"
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
        "call panic\n"
        "1: hlt\n"
        "jmp 1b"
    );
}

void idt_init(void) {
    struct { uint16_t limit; uint64_t base; } __attribute__((packed))
        p = { sizeof(idt) - 1, (uint64_t)idt };

    for (int i = 0; i < 32; i++)
        idt_set_gate(i, (uint64_t)exception_stub);

    // FIXME: add more IRQs
    idt_set_gate(0x21, (uint64_t)irq1_stub);

    __asm__ volatile("lidt %0" :: "m"(p));
}

void pic_init(void) {
    outb(0x20, 0x11); io_wait(); outb(0xA0, 0x11); io_wait();
    outb(0x21, 0x20); io_wait(); outb(0xA1, 0x28); io_wait();
    outb(0x21, 0x04); io_wait(); outb(0xA1, 0x02); io_wait();
    outb(0x21, 0x01); io_wait(); outb(0xA1, 0x01); io_wait();
    outb(0x21, 0xFD); io_wait();
    outb(0xA1, 0xFF); io_wait();
}
