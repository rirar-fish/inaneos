#pragma once

void shell_run(void);

#ifndef  __wasm__
void shell_handle_key(char c);
#endif
