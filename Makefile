# flags
CFLAGSx86 = -m64 -ffreestanding -fno-stack-protector -fno-pic -fno-pie -O2 -Wall -g -MMD -MP -mgeneral-regs-only
CFLAGSwasm = --target=wasm64 -nostdlib -O2 -Wall -g -MMD -MP

LDFLAGS_wasm = -Wl,--no-entry -Wl,--export-all -Wl,--allow-undefined -Wl,--import-memory

# build dir controller
SRCS_x86_C = kernel.c vga.c keyboard.c shell.c
SRCS_x86_S = boot.S
OBJS_x86 = $(addprefix build/x86/, $(SRCS_x86_C:.c=.o) $(SRCS_x86_S:.S=.o))

SRCS_wasm = kernel.c shell.c
OBJS_wasm = $(addprefix build/wasm/, $(SRCS_wasm:.c=.o))

.PHONY: all x86 x86_64 wasm runx86 runwasm clean

# total compile
all: x86 wasm
x86: inaneos-x86.elf
wasm: inaneos.wasm

# ext
inaneos-x86.elf: $(OBJS_x86) linker.ld
	ld -m elf_x86_64 -T linker.ld -o $@ $(OBJS_x86)
build/x86/%.o: %.c
	@mkdir -p build/x86
	gcc $(CFLAGSx86) -c $< -o $@
build/x86/%.o: %.S
	@mkdir -p build/x86
	gcc $(CFLAGSx86) -c $< -o $@
inaneos-x86.iso: inaneos-x86.elf grub.cfg
	mkdir -p isodir/boot/grub
	cp inaneos-x86.elf isodir/boot/
	cp grub.cfg isodir/boot/grub/grub.cfg
	grub-mkrescue -o $@ isodir

inaneos.wasm: $(OBJS_wasm)
	clang $(CFLAGSwasm) $(LDFLAGS_wasm) -o $@ $(OBJS_wasm)
build/wasm/%.o: %.c
	@mkdir -p build/wasm
	clang $(CFLAGSwasm) -c $< -o $@

runx86: inaneos-x86.iso
	qemu-system-x86_64 -cdrom inaneos-x86.iso
runwasm: inaneos.wasm
	python3 -m http.server 8080

clean:
	rm -rf *.o build inaneos-x86.elf inaneos.wasm inaneos-x86.iso isodir

-include $(OBJS_x86:.o=.d)
-include $(OBJS_wasm:.o=.d)
