#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <errno.h>
#include <locale.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "net.h"
#include "protocol.h"
#include "tui.h"
#include "util.h"

static void trim_whitespace(char *text)
{
    char *start = text;
    while (*start && isspace((unsigned char)*start))
        start++;

    if (start != text)
        memmove(text, start, strlen(start) + 1);

    size_t length = strlen(text);
    while (length > 0 && isspace((unsigned char)text[length - 1]))
        text[--length] = '\0';
}

static int valid_username(const char *name)
{
    size_t length = strlen(name);

    if (length == 0 || length >= NAME_LEN)
        return 0;

    for (const char *cursor = name; *cursor; cursor++) {
        if (isspace((unsigned char)*cursor))
            return 0;
    }

    return 1;
}

static int register_client(int fd, char **out_name, uint32_t *out_id,
                           int *out_color)
{
    char name[NAME_LEN];

    printf("Username is your password; reuse it to keep your id.\n");

    for (;;) {
        printf("Username: ");
        fflush(stdout);

        if (!fgets(name, sizeof(name), stdin))
            return -1;

        trim_whitespace(name);
        if (valid_username(name))
            break;

        printf("1-%d characters, no spaces.\n", NAME_LEN - 1);
    }

    char request[NAME_LEN + 16];
    int request_length = snprintf(request, sizeof(request),
                                  "request_id %s", name);
    if (request_length < 0 ||
        (size_t)request_length >= sizeof(request))
        return -1;

    if (send_packet(fd, PKT_SYS, 0, request,
                    (size_t)request_length) < 0)
        return -1;

    PacketHeader header;
    char *reply = NULL;
    if (recv_packet(fd, &header, &reply) < 0)
        return -1;

    char *copy = xmalloc((size_t)header.length + 1);
    memcpy(copy, reply, header.length);
    copy[header.length] = '\0';

    int ok = 0;
    uint32_t id = 0;
    int color = 0;

    if (header.type == PKT_SYS) {
        char *first = strchr(copy, ' ');
        if (first) {
            *first = '\0';
            char *second = strchr(first + 1, ' ');
            if (second && !strchr(second + 1, ' ')) {
                *second = '\0';
                if (strcmp(copy, "ok") == 0) {
                    id = (uint32_t)strtoul(first + 1, NULL, 10);
                    color = atoi(second + 1);
                    ok = 1;
                }
            }
        }
    }

    if (!ok) {
        if (header.length > 0)
            fwrite(reply, 1, header.length, stderr);
        else
            fputs("registration failed", stderr);
        fputc('\n', stderr);
        free(copy);
        free(reply);
        return -1;
    }

    free(copy);
    free(reply);

    *out_name = xstrdup(name);
    *out_id = id;
    *out_color = color;
    return 0;
}

int main(void)
{
    setlocale(LC_ALL, "");

    printf("=== Register ===\n");
    printf("Server IP: ");
    fflush(stdout);

    char host[256];
    if (!fgets(host, sizeof(host), stdin))
        host[0] = '\0';

    trim_whitespace(host);
    if (host[0] == '\0')
        strcpy(host, "127.0.0.1");

    int fd = net_connect(host, DEFAULT_PORT);
    if (fd < 0) {
        fprintf(stderr, "Could not connect to %s:%d: %s\n",
                host, DEFAULT_PORT, strerror(errno));
        return EXIT_FAILURE;
    }

    char *name = NULL;
    uint32_t client_id = 0;
    int color = 0;

    if (register_client(fd, &name, &client_id, &color) < 0) {
        close(fd);
        return EXIT_FAILURE;
    }

    if (net_set_nonblocking(fd) < 0) {
        close(fd);
        free(name);
        return EXIT_FAILURE;
    }

    const char *message = tui_run(fd, client_id, name, color);

    close(fd);
    free(name);

    if (message)
        printf("\033[1;31m%s\033[0m\n", message);

    return EXIT_SUCCESS;
}
