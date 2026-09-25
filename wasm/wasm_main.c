// wasm boot: same init order as kernel_main in kernel/kernel.c
// (fs, disk scan, auto-mount part 0, then the shell module).
#include "fs.h"
#include "part.h"
#include "term_grid.h"

void shell_main(int argc, char **argv);

// enter_program(0, 0): fresh shell entry (kernel fs/grid state persists,
// like QEMU, where only the program image is reloaded).
void enter_shell(void) {
  static char *argv[] = {"shell", 0};
  shell_main(1, argv);
  __builtin_trap(); // shell never returns; trap if it ever does
}

void kernel_main(void) {
  term_init();
  fs_init();
  part_scan();
  if (part_count() > 0)
    fs_mount_part(0); // disk first if present
  enter_shell();
}
