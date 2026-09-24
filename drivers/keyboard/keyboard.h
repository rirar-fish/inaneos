#pragma once

// extended keys >= 0x80
#define KEY_UP 0x80
#define KEY_DOWN 0x81
#define KEY_LEFT 0x82
#define KEY_RIGHT 0x83
#define KEY_DEL 0x84

int getchar(void);
int kbd_decode(unsigned char sc, int *e0s, int *shifts, int *capsv);
void keyboard_handler(void);
void irq1_stub(void);
