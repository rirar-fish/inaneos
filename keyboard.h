#pragma once

#ifndef __wasm__
int getchar(void);
void keyboard_handler(void);
void irq1_stub(void);
#endif
