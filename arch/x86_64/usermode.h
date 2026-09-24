#pragma once
#include <stdint.h>

// drop to ring3
void jump_usermode(uint64_t rip, uint64_t rsp);
