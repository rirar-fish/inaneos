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

---


### Dependencies just to try:

-> qemu
-> xorriso
-> gcc
-> grub-mkrescue tool
-> make


---


## INSTALATION

- install inaneos in [release](https://github.com/reyzzzl/inaneos/releases/tag/beta-0.0.2)
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

---

## Disk system (moon)


we make the moon, this like mount in linux if you wanna persistent file

moon is a type partion MBR 1 byte partition format content marker and displayed in hex, but the problem rn is inaneos stay in type c = 0c, which is FAT32 LBA. and the mkdir jst readonly too bc i havent added the FAT driver yet. 


<img width="800" height="450" alt="2026-09-2803-36-57-ezgif com-video-to-gif-converter" src="https://github.com/user-attachments/assets/1a2d775a-e03d-405d-accc-fb6b8e9bb6d7" />


```
moon // this for checking avaibility disk
moon o // so this for moon partition o to the disk system
moon -u // unmount and back to ram file system
```

---
## Keo(text editor)

keo is native text editor in inaneos and have ui like neovim, but this is from scratch, and basically we stay the command in keo same like nvim for easily new user 

<img width="800" height="450" alt="2026-09-2803-51-30-ezgif com-video-to-gif-converter" src="https://github.com/user-attachments/assets/b09cef31-ca40-40c5-b5e3-c616b1495bc0" />

command keo:
```
h j k l // navigation
i // start typing
: // mode cmd
```

mode cmd:
```
:w // save and auto exit
:w namefile // save as and exit
:q // exit unsaved
:q! // force exit no save
:wq // save and exit
```

---

## next update

- memory manager
- scheduler
- any drivers
