#define _XOPEN_SOURCE_EXTENDED 1

#include "tui.h"

#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <wchar.h>
#include <wctype.h>

#include "chat.h"
#include "colors.h"
#include "curses_compat.h"
#include "history.h"
#include "net.h"
#include "protocol.h"
#include "roster.h"
#include "util.h"
#include "wrap.h"

typedef struct {
    unsigned char *data;
    size_t length;
    size_t capacity;
} ByteBuffer;

static volatile sig_atomic_t interrupted = 0;

static void handle_sigint(int signal_number)
{
    (void)signal_number;
    interrupted = 1;
}

static void buffer_append(ByteBuffer *buffer, const void *data, size_t length)
{
    if (buffer->length + length > buffer->capacity) {
        size_t capacity = buffer->capacity ? buffer->capacity : 4096;
        while (capacity < buffer->length + length)
            capacity *= 2;
        buffer->data = xrealloc(buffer->data, capacity);
        buffer->capacity = capacity;
    }

    memcpy(buffer->data + buffer->length, data, length);
    buffer->length += length;
}

static void buffer_consume(ByteBuffer *buffer, size_t length)
{
    if (length >= buffer->length) {
        buffer->length = 0;
        return;
    }

    memmove(buffer->data, buffer->data + length, buffer->length - length);
    buffer->length -= length;
}

static Segment *entry_segments(const Entry *entry, Colors *colors,
                               size_t *out_count)
{
    switch (entry->kind) {
    case ENTRY_CHAT: {
        Segment *segments = xmalloc(3 * sizeof(Segment));
        segments[0].text = xwcsdup(entry->name);
        segments[0].attr = colors_attr(colors, entry->color);
        segments[1].text = xwcsdup(L": ");
        segments[1].attr = 0;
        segments[2].text = xwcsdup(entry->text);
        segments[2].attr = 0;
        *out_count = 3;
        return segments;
    }
    case ENTRY_JOIN: {
        size_t size = wcslen(entry->name) + 16;
        wchar_t *text = xmalloc(size * sizeof(wchar_t));
        swprintf(text, size, L"++ %ls joined", entry->name);

        Segment *segments = xmalloc(sizeof(Segment));
        segments[0].text = text;
        segments[0].attr = colors_attr(colors, entry->color);
        *out_count = 1;
        return segments;
    }
    case ENTRY_LEAVE: {
        size_t size = wcslen(entry->name) + 24;
        wchar_t *text = xmalloc(size * sizeof(wchar_t));
        swprintf(text, size, L"-- %ls disconnected --", entry->name);

        Segment *segments = xmalloc(sizeof(Segment));
        segments[0].text = text;
        segments[0].attr = colors_attr(colors, COLOR_RED) | A_BOLD;
        *out_count = 1;
        return segments;
    }
    case ENTRY_ERROR: {
        Segment *segments = xmalloc(sizeof(Segment));
        segments[0].text = xwcsdup(entry->text);
        segments[0].attr = colors_attr(colors, COLOR_RED) | A_BOLD;
        *out_count = 1;
        return segments;
    }
    case ENTRY_INFO:
    default: {
        Segment *segments = xmalloc(sizeof(Segment));
        segments[0].text = xwcsdup(entry->text);
        segments[0].attr = 0;
        *out_count = 1;
        return segments;
    }
    }
}

static void segments_free(Segment *segments, size_t count)
{
    for (size_t i = 0; i < count; i++)
        free(segments[i].text);
    free(segments);
}

static Lines build_lines(const History *history, int width, Colors *colors)
{
    Lines result = { .lines = NULL, .count = 0 };

    for (size_t i = 0; i < history->count; i++) {
        size_t count;
        Segment *segments = entry_segments(&history->items[i], colors,
                                           &count);
        Lines wrapped = wrap_segments(segments, count, width);
        segments_free(segments, count);

        result.lines = xrealloc(result.lines,
                                (result.count + wrapped.count) *
                                    sizeof(Line));
        memcpy(&result.lines[result.count], wrapped.lines,
               wrapped.count * sizeof(Line));
        result.count += wrapped.count;
        free(wrapped.lines);
    }

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

static void add_message(History *history, EntryKind kind, const char *text)
{
    Entry entry = {
        .kind = kind,
        .name = NULL,
        .text = utf8_to_wcs(text, strlen(text)),
        .color = 0,
    };
    history_add(history, entry);
}

static void draw(History *history, Colors *colors, int top, bool follow,
                 int *out_top, int *out_max_top, int *out_view_height,
                 const wchar_t *input, size_t input_length)
{
    int height, width;
    getmaxyx(stdscr, height, width);
    if (height < 1)
        height = 1;
    if (width < 1)
        width = 1;

    int view_height = height - 1;
    if (view_height < 1)
        view_height = 1;

    Lines lines = build_lines(history, width, colors);

    int max_top = (int)lines.count - view_height;
    if (max_top < 0)
        max_top = 0;

    if (follow)
        top = max_top;
    else if (top < 0)
        top = 0;
    else if (top > max_top)
        top = max_top;

    erase();

    for (int row = 0; row < view_height; row++) {
        int index = top + row;
        if (index >= (int)lines.count)
            break;

        int column = 0;
        Line *line = &lines.lines[index];

        for (size_t s = 0; s < line->count; s++) {
            if (column >= width)
                break;
            mvwaddnwstr(stdscr, row, column, line->segments[s].text,
                        width - column);
            column += (int)wcslen(line->segments[s].text);
        }
    }

    int available = width - 2;
    if (available < 1)
        available = 1;

    size_t start = input_length > (size_t)available
                       ? input_length - (size_t)available
                       : 0;

    wchar_t *bar = xmalloc(((size_t)width + 4) * sizeof(wchar_t));
    int length = 0;
    bar[length++] = L'>';
    bar[length++] = L' ';

    for (size_t i = start; i < input_length; i++)
        bar[length++] = input[i];

    int bar_length = length;
    while (length < width)
        bar[length++] = L' ';
    bar[length] = L'\0';

    attron(A_REVERSE);
    mvwaddnwstr(stdscr, height - 1, 0, bar, width);
    attroff(A_REVERSE);

    int cursor = bar_length < width - 1 ? bar_length : width - 1;
    wmove(stdscr, height - 1, cursor);
    refresh();

    free(bar);
    lines_free(&lines);

    *out_top = top;
    *out_max_top = max_top;
    *out_view_height = view_height;
}

const char *tui_run(int fd, uint32_t client_id, const char *name, int color)
{
    if (initscr() == NULL)
        return "Could not initialize the terminal.";

    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    timeout(50);

    if (curs_set(1) == ERR) {
        /* Cursor visibility is best-effort. */
    }

    interrupted = 0;
    signal(SIGINT, handle_sigint);

    Colors colors;
    colors_init(&colors);

    Roster roster;
    roster_init(&roster);

    wchar_t *wide_name = utf8_to_wcs(name, strlen(name));
    roster_set(&roster, client_id, wide_name, color);

    History history;
    history_init(&history);

    wchar_t greeting[256];
    swprintf(greeting, 256, L"Registered as %ls (id %u)",
             wide_name, client_id);

    Entry greeting_entry = {
        .kind = ENTRY_INFO,
        .name = NULL,
        .text = xwcsdup(greeting),
        .color = 0,
    };
    history_add(&history, greeting_entry);

    add_message(&history, ENTRY_INFO,
                "Type a message. PgUp/PgDn or arrows to scroll. "
                "'close' quits.");

    ByteBuffer receive_buffer = { .data = NULL, .length = 0, .capacity = 0 };

    wchar_t input[MAX_BODY + 1];
    input[0] = L'\0';
    size_t input_length = 0;

    int top = 0;
    bool follow = true;
    const char *result = NULL;

    for (;;) {
        if (interrupted)
            break;

        int max_top = 0;
        int view_height = 1;
        draw(&history, &colors, top, follow, &top, &max_top,
             &view_height, input, input_length);

        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(fd, &read_fds);

        struct timeval timeout = { .tv_sec = 0, .tv_usec = 0 };
        if (select(fd + 1, &read_fds, NULL, NULL, &timeout) > 0 &&
            FD_ISSET(fd, &read_fds)) {
            bool closed = false;

            for (;;) {
                char chunk[65536];
                ssize_t received = recv(fd, chunk, sizeof(chunk), 0);

                if (received > 0) {
                    buffer_append(&receive_buffer, chunk,
                                  (size_t)received);
                    continue;
                }
                if (received == 0) {
                    closed = true;
                    break;
                }
                if (errno == EINTR)
                    continue;
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    break;
                closed = true;
                break;
            }

            while (receive_buffer.length >= HDR_LEN) {
                PacketHeader header;
                header_decode(receive_buffer.data, &header);

                if (header.length > MAX_BODY) {
                    closed = true;
                    break;
                }
                if (receive_buffer.length <
                    (size_t)HDR_LEN + header.length)
                    break;

                chat_process_packet(header.type, header.sender_id,
                                    (const char *)receive_buffer.data +
                                        HDR_LEN,
                                    header.length, &roster, client_id,
                                    &history);
                buffer_consume(&receive_buffer, HDR_LEN + header.length);
            }

            if (closed) {
                result = "Connection closed by server.";
                break;
            }
        }

        wint_t key;
        int code = wget_wch(stdscr, &key);

        if (interrupted)
            break;

        if (code == ERR)
            continue;

        bool enter = false;
        bool backspace = false;
        bool quit = false;

        if (code == KEY_CODE_YES) {
            if (key == KEY_ENTER) {
                enter = true;
            } else if (key == KEY_BACKSPACE) {
                backspace = true;
            } else if (key == KEY_UP) {
                follow = false;
                top -= 1;
            } else if (key == KEY_DOWN) {
                if (!follow) {
                    top += 1;
                    if (top >= max_top)
                        follow = true;
                }
            } else if (key == KEY_PPAGE) {
                follow = false;
                top -= view_height;
            } else if (key == KEY_NPAGE) {
                if (!follow) {
                    top += view_height;
                    if (top >= max_top)
                        follow = true;
                }
            }
        } else if (code == OK) {
            if (key == L'\n' || key == L'\r') {
                enter = true;
            } else if (key == 127 || key == 8) {
                backspace = true;
            } else if (key == 3) {
                quit = true;
            } else if (iswprint((wint_t)key) &&
                       input_length < MAX_BODY) {
                input[input_length++] = (wchar_t)key;
                input[input_length] = L'\0';
            }
        }

        if (quit)
            break;

        if (backspace) {
            if (input_length > 0) {
                input_length--;
                input[input_length] = L'\0';
            }
            continue;
        }

        if (!enter)
            continue;

        if (input_length == 0)
            continue;

        size_t message_length;
        char *message = wcs_to_utf8(input, &message_length);

        if (message_length > MAX_BODY) {
            add_message(&history, ENTRY_ERROR, "Message too long.");
            free(message);
            input_length = 0;
            input[0] = L'\0';
            continue;
        }

        bool close_requested = wcscmp(input, L"close") == 0;

        if (send_packet(fd, PKT_MSG, client_id, message,
                        message_length) < 0) {
            free(message);
            result = "Send failed.";
            break;
        }

        free(message);
        input_length = 0;
        input[0] = L'\0';
        follow = true;

        if (close_requested)
            break;
    }

    endwin();

    colors_free(&colors);
    roster_free(&roster);
    history_free(&history);
    free(wide_name);
    free(receive_buffer.data);

    return result;
}
