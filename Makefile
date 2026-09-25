CC = gcc
LD = ld

# one dir = one module
MODULES = arch/x86_64 drivers/vga drivers/keyboard drivers/ide kernel
INCLUDES = $(addprefix -I,$(MODULES))

CFLAGS = -m64 -ffreestanding -fno-stack-protector -fno-pic -fno-pie -O2 -Wall -Wshadow -g -MMD -MP -mgeneral-regs-only -mno-red-zone $(INCLUDES)

# collect sources
SRCS_C = $(shell find $(MODULES) -name '*.c')
SRCS_S = $(shell find arch -name '*.S')
OBJS = $(patsubst %.c,build/obj/%.o,$(SRCS_C)) $(patsubst %.S,build/obj/%.o,$(SRCS_S))
DEPS = $(OBJS:.o=.d)

LINKER = arch/x86_64/linker.ld
KERNEL_ELF = build/kernel.elf
GRUB_CFG = boot/grub/grub.cfg
ISODIR = build/isodir

# user program, own image
USER_CFLAGS = -m64 -ffreestanding -fno-stack-protector -fno-pic -fno-pie -O2 -Wall -Wshadow -g -MMD -MP -mgeneral-regs-only -mno-red-zone -Iuser -Ikernel -Idrivers/keyboard
USER_SRCS_C = $(shell find user -name '*.c')
USER_SRCS_S = $(shell find user -name '*.S')
USER_OBJS = $(patsubst %.c,build/obj-user/%.o,$(USER_SRCS_C)) $(patsubst %.S,build/obj-user/%.o,$(USER_SRCS_S))
USER_DEPS = $(USER_OBJS:.o=.d)
# per-image links below share USER_OBJS build rules
USER_ELF = build/user.elf
CALC_ELF = build/calc.elf
KEO_ELF = build/keo.elf
USER_LINKER = user/user.ld
USER_COMMON = build/obj-user/user/_start.o

all: os.iso

$(KERNEL_ELF): $(OBJS) $(LINKER)
	ld -m elf_x86_64 -T $(LINKER) -o $@ $(OBJS)

$(USER_ELF): build/obj-user/user/shell.o $(USER_COMMON) $(USER_LINKER)
	ld -m elf_x86_64 -static -z noexecstack -T $(USER_LINKER) -o $@ build/obj-user/user/shell.o $(USER_COMMON)

$(CALC_ELF): build/obj-user/user/calc.o build/obj-user/user/expr.o $(USER_COMMON) $(USER_LINKER)
	ld -m elf_x86_64 -static -z noexecstack -T $(USER_LINKER) -o $@ build/obj-user/user/calc.o build/obj-user/user/expr.o $(USER_COMMON)

$(KEO_ELF): build/obj-user/user/keo.o $(USER_COMMON) $(USER_LINKER)
	ld -m elf_x86_64 -static -z noexecstack -T $(USER_LINKER) -o $@ build/obj-user/user/keo.o $(USER_COMMON)

# wasm browser preview (branch feat/wasm), opt-in, does not affect os.iso.
# Builds the REAL userspace (shell/calc/keo/expr) plus the REAL
# kernel fs/fat/part against a browser shim: same prompt, same
# commands, same outputs as QEMU. JS harness in web/.
WASM_SHIM = wasm/shim
WASM_CC = clang
WASM_CFLAGS = --target=wasm32 -nostdlib -ffreestanding -matomics -O2 -Wall -Wextra \
	-g -MMD -MP \
	-I$(WASM_SHIM) -Iuser -Ikernel -Idrivers/keyboard -Idrivers/ide
WASM_LDFLAGS = -Wl,--no-entry -Wl,--export-all -Wl,--allow-undefined \
	-Wl,--import-memory -Wl,--shared-memory \
	-Wl,--initial-memory=16777216 -Wl,--max-memory=268435456
WASM_SRCS_C = wasm/wasm_main.c wasm/sys_shim.c wasm/term_grid.c \
wasm/ide_stub.c user/expr.c kernel/fs.c kernel/fat.c kernel/part.c
WASM_RENAMED = build/wasm/user/shell.o build/wasm/user/calc.o \
	build/wasm/user/keo.o
WASM_OBJS = $(patsubst %.c,build/wasm/%.o,$(WASM_SRCS_C)) $(WASM_RENAMED)
WASM_DEPS = $(WASM_OBJS:.o=.d)
WASM_OUT = web/inaneos.wasm

wasm: $(WASM_OUT)

$(WASM_OUT): $(WASM_OBJS)
	mkdir -p $(dir $@)
	$(WASM_CC) --target=wasm32 -nostdlib $(WASM_LDFLAGS) -o $@ $(WASM_OBJS)

# same file as QEMU, entry renamed so shell+calc+keo link together
build/wasm/user/shell.o: user/shell.c
	mkdir -p $(dir $@)
	$(WASM_CC) $(WASM_CFLAGS) -Duser_main=shell_main -c $< -o $@

build/wasm/user/calc.o: user/calc.c
	mkdir -p $(dir $@)
	$(WASM_CC) $(WASM_CFLAGS) -Duser_main=calc_main -c $< -o $@

build/wasm/user/keo.o: user/keo.c
	mkdir -p $(dir $@)
	$(WASM_CC) $(WASM_CFLAGS) -Duser_main=keo_main -c $< -o $@

build/wasm/%.o: %.c
	mkdir -p $(dir $@)
	$(WASM_CC) $(WASM_CFLAGS) -c $< -o $@

runwasm: $(WASM_OUT)
	python3 web/serve.py

wasmtest: $(WASM_OUT)
	node tools/wasm_parity.mjs

build/obj-user/%.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(USER_CFLAGS) -c $< -o $@

build/obj-user/%.o: %.S
	mkdir -p $(dir $@)
	$(CC) $(USER_CFLAGS) -c $< -o $@

build/obj/%.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

build/obj/%.o: %.S
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

os.iso: $(KERNEL_ELF) $(USER_ELF) $(CALC_ELF) $(KEO_ELF) $(GRUB_CFG)
	mkdir -p $(ISODIR)/boot/grub
	cp $(KERNEL_ELF) $(ISODIR)/boot/
	cp $(USER_ELF) $(ISODIR)/boot/
	cp $(CALC_ELF) $(ISODIR)/boot/
	cp $(KEO_ELF) $(ISODIR)/boot/
	cp $(GRUB_CFG) $(ISODIR)/boot/grub/grub.cfg
	grub-mkrescue -o os.iso $(ISODIR)

run: os.iso disk.img
	qemu-system-x86_64 -cdrom os.iso -drive file=disk.img,format=raw,if=ide -boot order=d

disk.img: tools/mkdisk.sh
	sh tools/mkdisk.sh

clean:
	rm -rf build os.iso
	rm -f kernel.elf *.o *.d
	rm -rf isodir
	rm -f web/inaneos.wasm

.PHONY: all run clean wasm runwasm

-include $(DEPS)
-include $(USER_DEPS)
-include $(WASM_DEPS)
