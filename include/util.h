#ifndef MULTICHAT_UTIL_H
#define MULTICHAT_UTIL_H

#include <stddef.h>
#include <wchar.h>

void *xmalloc(size_t size);
void *xrealloc(void *ptr, size_t size);
char *xstrdup(const char *text);
wchar_t *xwcsdup(const wchar_t *text);

/*
 * UTF-8 <-> wide string conversion.
 *
 * Invalid UTF-8 sequences are replaced with U+FFFD, matching the Python
 * client's decode(errors="replace") behaviour.
 */
wchar_t *utf8_to_wcs(const char *text, size_t length);
char *wcs_to_utf8(const wchar_t *text, size_t *out_length);

#endif
