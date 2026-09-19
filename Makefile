CC = gcc
LD = ld

# one dir = one module
MODULES = arch/x86_64 drivers/vga drivers/keyboard kernel
INCLUDES = $(addprefix -I,$(MODULES))

CFLAGS = -m64 -ffreestanding -fno-stack-protector -fno-pic -fno-pie -O2 -Wall -g -MMD -MP -mgeneral-regs-only -mno-red-zone $(INCLUDES)

# collect sources
SRCS_C = $(shell find $(MODULES) -name '*.c')
SRCS_S = $(shell find arch -name '*.S')
OBJS = $(patsubst %.c,build/obj/%.o,$(SRCS_C)) $(patsubst %.S,build/obj/%.o,$(SRCS_S))
DEPS = $(OBJS:.o=.d)

LINKER = arch/x86_64/linker.ld
KERNEL_ELF = build/kernel.elf
GRUB_CFG = boot/grub/grub.cfg
ISODIR = build/isodir

all: os.iso

$(KERNEL_ELF): $(OBJS) $(LINKER)
	ld -m elf_x86_64 -T $(LINKER) -o $@ $(OBJS)

build/obj/%.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

build/obj/%.o: %.S
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

os.iso: $(KERNEL_ELF) $(GRUB_CFG)
	mkdir -p $(ISODIR)/boot/grub
	cp $(KERNEL_ELF) $(ISODIR)/boot/
	cp $(GRUB_CFG) $(ISODIR)/boot/grub/grub.cfg
	grub-mkrescue -o os.iso $(ISODIR)

run: os.iso
	qemu-system-x86_64 -cdrom os.iso

clean:
	rm -rf build os.iso
	rm -f kernel.elf *.o *.d
	rm -rf isodir

.PHONY: all run clean

-include $(DEPS)
