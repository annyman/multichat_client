#ifndef MULTICHAT_HISTORY_H
#define MULTICHAT_HISTORY_H

#include <stddef.h>
#include <wchar.h>

#include "protocol.h"

typedef enum {
    ENTRY_INFO,
    ENTRY_CHAT,
    ENTRY_JOIN,
    ENTRY_LEAVE,
    ENTRY_ERROR,
} EntryKind;

typedef struct {
    EntryKind kind;
    wchar_t *name; /* chat author, join name, or leave name */
    wchar_t *text; /* chat text or info/error text */
    int color;
} Entry;

typedef struct {
    Entry *items;
    size_t count;
    size_t capacity;
} History;

void history_init(History *history);
void history_free(History *history);

/* Appends an entry, taking ownership of entry.name and entry.text. */
void history_add(History *history, Entry entry);

#endif
