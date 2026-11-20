#define _XOPEN_SOURCE_EXTENDED 1

#include "colors.h"

#include <stdlib.h>

#include "curses_compat.h"

#include "util.h"

void colors_init(Colors *colors)
{
    colors->pairs = NULL;
    colors->count = 0;
    colors->capacity = 0;
    colors->background = 0;
    colors->enabled = has_colors();

    if (colors->enabled) {
        start_color();
        if (use_default_colors() != ERR)
            colors->background = -1;
    }
}

void colors_free(Colors *colors)
{
    free(colors->pairs);
    colors->pairs = NULL;
    colors->count = 0;
    colors->capacity = 0;
}

int colors_attr(Colors *colors, int color)
{
    if (!colors->enabled)
        return 0;

    for (int i = 0; i < colors->count; i++) {
        if (colors->pairs[i].color == color)
            return COLOR_PAIR(colors->pairs[i].pair);
    }

    int pair = colors->count + 1;
    if (pair >= COLOR_PAIRS)
        return 0;

    int index = 0;
    if (COLORS) {
        index = color % COLORS;
        if (index < 0)
            index += COLORS;
    }
    init_pair((short)pair, (short)index, (short)colors->background);

    if (colors->count == colors->capacity) {
        colors->capacity = colors->capacity ? colors->capacity * 2 : 8;
        colors->pairs = xrealloc(colors->pairs,
                                 colors->capacity * sizeof(ColorPair));
    }

    colors->pairs[colors->count].color = color;
    colors->pairs[colors->count].pair = pair;
    colors->count++;

    return COLOR_PAIR(pair);
}
