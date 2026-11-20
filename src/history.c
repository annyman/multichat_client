#include "history.h"

#include <stdlib.h>
#include <string.h>

#include "util.h"

static void entry_free(Entry *entry)
{
    free(entry->name);
    free(entry->text);
    entry->name = NULL;
    entry->text = NULL;
}

void history_init(History *history)
{
    history->items = NULL;
    history->count = 0;
    history->capacity = 0;
}

void history_free(History *history)
{
    for (size_t i = 0; i < history->count; i++)
        entry_free(&history->items[i]);
    free(history->items);
    history_init(history);
}

void history_add(History *history, Entry entry)
{
    if (history->count == HISTORY_LIMIT) {
        entry_free(&history->items[0]);
        memmove(&history->items[0], &history->items[1],
                (history->count - 1) * sizeof(Entry));
        history->count--;
    }

    if (history->count == history->capacity) {
        history->capacity = history->capacity ? history->capacity * 2 : 64;
        if (history->capacity > HISTORY_LIMIT)
            history->capacity = HISTORY_LIMIT;
        history->items = xrealloc(history->items,
                                  history->capacity * sizeof(Entry));
    }

    history->items[history->count++] = entry;
}
