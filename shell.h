#pragma once

void shell_run(void);

#ifdef  __wasm__
void shell_handle_key(char c);
#endif
