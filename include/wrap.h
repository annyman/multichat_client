#ifndef MULTICHAT_WRAP_H
#define MULTICHAT_WRAP_H

#include <stddef.h>
#include <wchar.h>

typedef struct {
    wchar_t *text;
    int attr;
} Segment;

typedef struct {
    Segment *segments;
    size_t count;
} Line;

typedef struct {
    Line *lines;
    size_t count;
} Lines;

/*
 * Word-wraps styled segments to the given width, merging adjacent runs with
 * the same attribute. Always returns at least one (possibly empty) line.
 */
Lines wrap_segments(const Segment *segments, size_t count, int width);
void lines_free(Lines *lines);

#endif
