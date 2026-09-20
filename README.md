# INANE OS

![Status Version](https://img.shields.io/badge/Status-Beta-blue.svg)
![os](https://img.shields.io/badge/Type-Os-red.svg)
![arch](https://img.shields.io/badge/Arch-x86_64-pink.svg)
![build](https://img.shields.io/badge/Build-Makefile-orange.svg)


**OS FROM SCRATCH ARCH X86_64-MULTIBOOT-LAB OS**
  
> lab purpose os build
in case like we need direct hardware access
</p>

<p align="center">
<img width="800" height="450" alt="2026-09-2001-40-55-ezgif com-video-to-gif-converter" src="https://github.com/user-attachments/assets/b7ea2b67-bb85-429d-b030-d0fa0f929b45" />
</p>

## What Is This?

Inaneos is a os and kernel from scratch type monolitics x86_64  in qemu with grub bootloader via multiboot. Languange we use is a C and Assembly, mybe jst for right now

---
## Structure module:
```
boot/grub/grub.cfg
arch/x86_64/boot.S  //entry start
arch/x86_64/linker.ld  // layout
arch/x86_64/io.h  // outb io_wait
arch/x86_64/idt.c/h  // IDT + IPC
drivers/vga/vga.c/h  // term 80x25 in 0xB8000
drivers/keyboard/*.c/h  // irq1
kernel/kernel.c  //kernel main
kernel/shell.c/h  // shell run(fs ram only)
```
### Dependencies just to try:
---
-> qemu
-> xorriso
-> gcc
-> grub-mkrescue tool
-> make

### INSTALATION
---
- install inaneos in [release](https://github.com/reyzzzl/inaneos/releases/tag/beta-0.0.1)
---

how to use:
```
make run
```

<img width="700" alt="image" src="https://github.com/user-attachments/assets/a85db9c4-64a3-455a-8e69-8f220739bd51" />

> description:
to build and run the file instantly
```
make all
```

> description:
just compile or build the file

```
make clean
```

> description:
just delete non programmed file or compiled file
