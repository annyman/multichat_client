#include "wrap.h"

#include <stdlib.h>

#include "util.h"

typedef struct {
    wchar_t ch;
    int attr;
} CharAttr;

static void line_append(Line *line, wchar_t ch, int attr)
{
    if (line->count > 0 && line->segments[line->count - 1].attr == attr) {
        Segment *segment = &line->segments[line->count - 1];
        size_t length = wcslen(segment->text);
        segment->text = xrealloc(segment->text,
                                 (length + 2) * sizeof(wchar_t));
        segment->text[length] = ch;
        segment->text[length + 1] = L'\0';
        return;
    }

    line->segments = xrealloc(line->segments,
                              (line->count + 1) * sizeof(Segment));
    Segment *segment = &line->segments[line->count++];
    segment->text = xmalloc(2 * sizeof(wchar_t));
    segment->text[0] = ch;
    segment->text[1] = L'\0';
    segment->attr = attr;
}

Lines wrap_segments(const Segment *segments, size_t count, int width)
{
    if (width < 1)
        width = 1;

    size_t total = 0;
    for (size_t i = 0; i < count; i++)
        total += wcslen(segments[i].text);

    CharAttr *chars = xmalloc((total ? total : 1) * sizeof(CharAttr));
    size_t used = 0;

    for (size_t i = 0; i < count; i++) {
        for (const wchar_t *cursor = segments[i].text; *cursor; cursor++) {
            chars[used].ch = *cursor;
            chars[used].attr = segments[i].attr;
            used++;
        }
    }

    Lines result = { .lines = NULL, .count = 0 };
    size_t position = 0;

    while (position < used) {
        size_t cut = (size_t)width;
        if (cut > used - position)
            cut = used - position;

        if (cut < used - position) {
            for (size_t i = cut - 1; i > 0; i--) {
                if (chars[position + i].ch == L' ') {
                    cut = i;
                    break;
                }
            }
        }

        Line line = { .segments = NULL, .count = 0 };
        for (size_t i = 0; i < cut; i++)
            line_append(&line, chars[position + i].ch,
                        chars[position + i].attr);

        result.lines = xrealloc(result.lines,
                                (result.count + 1) * sizeof(Line));
        result.lines[result.count++] = line;

        position += cut;
        while (position < used && chars[position].ch == L' ')
            position++;
    }

    free(chars);

    if (result.count == 0) {
        result.lines = xmalloc(sizeof(Line));
        result.lines[0].segments = xmalloc(sizeof(Segment));
        result.lines[0].segments[0].text = xmalloc(sizeof(wchar_t));
        result.lines[0].segments[0].text[0] = L'\0';
        result.lines[0].segments[0].attr = 0;
        result.lines[0].count = 1;
        result.count = 1;
    }

    return result;
}

void lines_free(Lines *lines)
{
    for (size_t i = 0; i < lines->count; i++) {
        for (size_t j = 0; j < lines->lines[i].count; j++)
            free(lines->lines[i].segments[j].text);
        free(lines->lines[i].segments);
    }
    free(lines->lines);
    lines->lines = NULL;
    lines->count = 0;
}
