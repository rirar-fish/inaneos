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

.PHONY: all run clean

-include $(DEPS)
-include $(USER_DEPS)
