#ifndef MULTICHAT_COLORS_H
#define MULTICHAT_COLORS_H

typedef struct {
    int color;
    int pair;
} ColorPair;

typedef struct {
    ColorPair *pairs;
    int count;
    int capacity;
    int enabled;
    int background;
} Colors;

void colors_init(Colors *colors);
void colors_free(Colors *colors);

/* Returns a curses attribute for a logical color number. */
int colors_attr(Colors *colors, int color);

#endif
