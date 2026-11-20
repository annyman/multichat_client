#ifndef MULTICHAT_ROSTER_H
#define MULTICHAT_ROSTER_H

#include <stddef.h>
#include <stdint.h>
#include <wchar.h>

typedef struct {
    uint32_t id;
    wchar_t *name;
    int color;
} RosterEntry;

typedef struct {
    RosterEntry *items;
    size_t count;
    size_t capacity;
} Roster;

void roster_init(Roster *roster);
void roster_free(Roster *roster);

/* Adds or replaces an entry. The name is copied. */
void roster_set(Roster *roster, uint32_t id, const wchar_t *name, int color);

/* Returns the entry for id, or NULL when absent. */
const RosterEntry *roster_get(const Roster *roster, uint32_t id);

/* Removes id and returns its name (caller frees), or NULL when absent. */
wchar_t *roster_remove(Roster *roster, uint32_t id);

#endif
