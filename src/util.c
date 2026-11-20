#include "util.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void out_of_memory(void)
{
    fputs("multichat: out of memory\n", stderr);
    exit(EXIT_FAILURE);
}

void *xmalloc(size_t size)
{
    void *pointer = malloc(size ? size : 1);
    if (!pointer)
        out_of_memory();
    return pointer;
}

void *xrealloc(void *pointer, size_t size)
{
    void *result = realloc(pointer, size ? size : 1);
    if (!result)
        out_of_memory();
    return result;
}

char *xstrdup(const char *text)
{
    size_t size = strlen(text) + 1;
    char *copy = xmalloc(size);
    memcpy(copy, text, size);
    return copy;
}

wchar_t *xwcsdup(const wchar_t *text)
{
    size_t size = wcslen(text) + 1;
    wchar_t *copy = xmalloc(size * sizeof(wchar_t));
    memcpy(copy, text, size * sizeof(wchar_t));
    return copy;
}

wchar_t *utf8_to_wcs(const char *text, size_t length)
{
    wchar_t *out = xmalloc((length + 1) * sizeof(wchar_t));
    size_t in_index = 0;
    size_t out_index = 0;

    while (in_index < length) {
        unsigned char byte = (unsigned char)text[in_index];
        uint32_t codepoint;
        size_t extra;

        if (byte < 0x80) {
            codepoint = byte;
            extra = 0;
        } else if ((byte & 0xE0) == 0xC0) {
            codepoint = byte & 0x1F;
            extra = 1;
        } else if ((byte & 0xF0) == 0xE0) {
            codepoint = byte & 0x0F;
            extra = 2;
        } else if ((byte & 0xF8) == 0xF0) {
            codepoint = byte & 0x07;
            extra = 3;
        } else {
            out[out_index++] = (wchar_t)0xFFFD;
            in_index++;
            continue;
        }

        int valid = 1;
        for (size_t i = 1; i <= extra; i++) {
            if (in_index + i >= length ||
                ((unsigned char)text[in_index + i] & 0xC0) != 0x80) {
                valid = 0;
                break;
            }
            codepoint = (codepoint << 6) |
                        ((unsigned char)text[in_index + i] & 0x3F);
        }

        if (!valid) {
            out[out_index++] = (wchar_t)0xFFFD;
            in_index++;
            continue;
        }

        in_index += extra + 1;

        uint32_t minimum = extra == 1 ? 0x80
                         : extra == 2 ? 0x800
                         : extra == 3 ? 0x10000
                                      : 0;

        if (codepoint < minimum ||
            codepoint > 0x10FFFF ||
            (codepoint >= 0xD800 && codepoint <= 0xDFFF))
            codepoint = 0xFFFD;

        out[out_index++] = (wchar_t)codepoint;
    }

    out[out_index] = L'\0';
    return out;
}

char *wcs_to_utf8(const wchar_t *text, size_t *out_length)
{
    size_t length = wcslen(text);
    char *out = xmalloc(length * 4 + 1);
    size_t index = 0;

    for (size_t i = 0; i < length; i++) {
        uint32_t codepoint = (uint32_t)text[i];

        if (codepoint < 0x80) {
            out[index++] = (char)codepoint;
        } else if (codepoint < 0x800) {
            out[index++] = (char)(0xC0 | (codepoint >> 6));
            out[index++] = (char)(0x80 | (codepoint & 0x3F));
        } else if (codepoint < 0x10000) {
            out[index++] = (char)(0xE0 | (codepoint >> 12));
            out[index++] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
            out[index++] = (char)(0x80 | (codepoint & 0x3F));
        } else {
            out[index++] = (char)(0xF0 | (codepoint >> 18));
            out[index++] = (char)(0x80 | ((codepoint >> 12) & 0x3F));
            out[index++] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
            out[index++] = (char)(0x80 | (codepoint & 0x3F));
        }
    }

    out[index] = '\0';
    if (out_length)
        *out_length = index;
    return out;
}
