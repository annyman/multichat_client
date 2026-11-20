#include "roster.h"

#include <stdlib.h>
#include <string.h>

#include "util.h"

static RosterEntry *find_entry(Roster *roster, uint32_t id)
{
    for (size_t i = 0; i < roster->count; i++) {
        if (roster->items[i].id == id)
            return &roster->items[i];
    }
    return NULL;
}

void roster_init(Roster *roster)
{
    roster->items = NULL;
    roster->count = 0;
    roster->capacity = 0;
}

void roster_free(Roster *roster)
{
    for (size_t i = 0; i < roster->count; i++)
        free(roster->items[i].name);
    free(roster->items);
    roster_init(roster);
}

void roster_set(Roster *roster, uint32_t id, const wchar_t *name, int color)
{
    RosterEntry *entry = find_entry(roster, id);

    if (entry) {
        wchar_t *copy = xwcsdup(name);
        free(entry->name);
        entry->name = copy;
        entry->color = color;
        return;
    }

    if (roster->count == roster->capacity) {
        roster->capacity = roster->capacity ? roster->capacity * 2 : 8;
        roster->items = xrealloc(roster->items,
                                 roster->capacity * sizeof(RosterEntry));
    }

    roster->items[roster->count].id = id;
    roster->items[roster->count].name = xwcsdup(name);
    roster->items[roster->count].color = color;
    roster->count++;
}

const RosterEntry *roster_get(const Roster *roster, uint32_t id)
{
    for (size_t i = 0; i < roster->count; i++) {
        if (roster->items[i].id == id)
            return &roster->items[i];
    }
    return NULL;
}

wchar_t *roster_remove(Roster *roster, uint32_t id)
{
    for (size_t i = 0; i < roster->count; i++) {
        if (roster->items[i].id == id) {
            wchar_t *name = roster->items[i].name;
            memmove(&roster->items[i], &roster->items[i + 1],
                    (roster->count - i - 1) * sizeof(RosterEntry));
            roster->count--;
            return name;
        }
    }
    return NULL;
}
