#include "terminal_buf.h"

#include <string.h>

int terminal_buf_append(char *buf, size_t cap, const char *add)
{
	size_t cur, add_len, need, drop;
	const char *nl;

	if (!buf || !add || cap == 0)
		return 0;

	add_len = strlen(add);
	if (add_len == 0)
		return 0;

	cur = strlen(buf);
	if (cur + add_len <= cap) {
		memcpy(buf + cur, add, add_len + 1);
		return 0;
	}

	/* The new chunk alone fills the buffer: keep only its tail. */
	if (add_len >= cap) {
		memcpy(buf, add + add_len - cap, cap);
		buf[cap] = '\0';
		return 1;
	}

	/* Drop whole lines from the front until the chunk fits. `need` bytes
	 * must go; round up to the end of the line that contains them. If no
	 * newline remains, the old text is one open line: drop all of it. */
	need = cur + add_len - cap; /* 1..cur, since add_len < cap */
	nl = memchr(buf + need - 1, '\n', cur - (need - 1));
	drop = nl ? (size_t)(nl - buf) + 1 : cur;

	memmove(buf, buf + drop, cur - drop + 1);
	cur -= drop;
	memcpy(buf + cur, add, add_len + 1);
	return 1;
}
