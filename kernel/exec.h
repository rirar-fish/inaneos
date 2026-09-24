#pragma once
#include <stdint.h>

#define USER_MIN 0x400000UL
#define USTACK_TOP 0x40002000UL

// module registry + loader
void exec_init_mods(uint32_t mods_addr, uint32_t count);
int exec_find(const char *name);
int exec_lsmod(char *buf, unsigned long cap);
int enter_program(int idx, const char *arg); // jumps, -1 on fail
