/*
 * terminal_buf - bounded text buffer for the on-screen log terminals.
 *
 * Keeps the newest text: when an append would exceed the capacity, the oldest
 * whole lines are discarded (instead of wiping the entire terminal). No LVGL
 * dependency, so it is unit-tested on the host.
 */
#ifndef TERMINAL_BUF_H
#define TERMINAL_BUF_H

#include <stddef.h>

/* Append `add` to the NUL-terminated text in `buf` (storage of at least
 * cap + 1 bytes, at most `cap` characters of text).
 *
 * Returns 0 when the text was appended unchanged, so a widget can simply
 * append the same chunk. Returns 1 when older text was discarded; the caller
 * must then rewrite the whole widget from `buf`.
 *
 * If `add` alone is >= cap characters, only its last `cap` characters are
 * kept. NULL or empty inputs are no-ops that return 0. */
int terminal_buf_append(char *buf, size_t cap, const char *add);

#endif /* TERMINAL_BUF_H */
