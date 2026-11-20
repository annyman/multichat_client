#ifndef MULTICHAT_TUI_H
#define MULTICHAT_TUI_H

#include <stdint.h>

/*
 * Runs the curses interface until the user quits or the connection closes.
 * Returns a message to print after the terminal is restored, or NULL.
 */
const char *tui_run(int fd, uint32_t client_id, const char *name, int color);

#endif
