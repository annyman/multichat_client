#include "chat.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "protocol.h"
#include "util.h"

/*
 * Splits a mutable string on single spaces, performing at most maxsplit
 * splits. Mirrors Python's str.split(" ", maxsplit). Returns the number of
 * parts written.
 */
static size_t split_max(char *text, int maxsplit, char **parts,
                        size_t max_parts)
{
    size_t count = 0;
    char *cursor = text;

    while (count < max_parts) {
        if (maxsplit >= 0 && (int)count == maxsplit) {
            parts[count++] = cursor;
            break;
        }

        char *space = strchr(cursor, ' ');
        if (!space) {
            parts[count++] = cursor;
            break;
        }

        *space = '\0';
        parts[count++] = cursor;
        cursor = space + 1;
    }

    return count;
}

void chat_process_packet(uint8_t type, uint32_t sender_id,
                         const char *body, size_t length,
                         Roster *roster, uint32_t client_id,
                         History *history)
{
    if (type == PKT_SYS) {
        char *buffer = xmalloc(length + 1);
        memcpy(buffer, body, length);
        buffer[length] = '\0';

        char *parts[4];
        size_t count = split_max(buffer, 3, parts, 4);

        if (count == 4 && strcmp(parts[0], "join") == 0) {
            uint32_t id = (uint32_t)strtoul(parts[1], NULL, 10);
            int color = atoi(parts[2]);
            wchar_t *name = utf8_to_wcs(parts[3], strlen(parts[3]));

            roster_set(roster, id, name, color);

            if (id != client_id) {
                Entry entry = {
                    .kind = ENTRY_JOIN,
                    .name = name,
                    .text = NULL,
                    .color = color,
                };
                history_add(history, entry);
            } else {
                free(name);
            }
        } else if (count >= 2 && strcmp(parts[0], "leave") == 0) {
            uint32_t id = (uint32_t)strtoul(parts[1], NULL, 10);
            wchar_t *name = roster_remove(roster, id);

            if (!name) {
                wchar_t fallback[32];
                swprintf(fallback, 32, L"id %u", id);
                name = xwcsdup(fallback);
            }

            Entry entry = {
                .kind = ENTRY_LEAVE,
                .name = name,
                .text = NULL,
                .color = 0,
            };
            history_add(history, entry);
        }

        free(buffer);
        return;
    }

    const RosterEntry *author = roster_get(roster, sender_id);
    wchar_t *name;
    int color;

    if (author) {
        name = xwcsdup(author->name);
        color = author->color;
    } else {
        wchar_t fallback[32];
        swprintf(fallback, 32, L"id %u", sender_id);
        name = xwcsdup(fallback);
        color = 15;
    }

    wchar_t *text = utf8_to_wcs(body, length);
    Entry entry = {
        .kind = ENTRY_CHAT,
        .name = name,
        .text = text,
        .color = color,
    };
    history_add(history, entry);
}
