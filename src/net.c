#define _POSIX_C_SOURCE 200809L

#include "net.h"

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "util.h"

int net_connect(const char *host, int port)
{
    struct addrinfo hints;
    struct addrinfo *result = NULL;
    char service[16];
    int fd = -1;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    snprintf(service, sizeof(service), "%d", port);

    if (getaddrinfo(host, service, &hints, &result) != 0) {
        errno = EADDRNOTAVAIL;
        return -1;
    }

    for (struct addrinfo *entry = result; entry; entry = entry->ai_next) {
        fd = socket(entry->ai_family, entry->ai_socktype, entry->ai_protocol);
        if (fd < 0)
            continue;
        if (connect(fd, entry->ai_addr, entry->ai_addrlen) == 0)
            break;
        close(fd);
        fd = -1;
    }

    freeaddrinfo(result);
    return fd;
}

int net_set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0)
        return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int recv_exact(int fd, void *buffer, size_t length)
{
    unsigned char *bytes = buffer;
    size_t received = 0;

    while (received < length) {
        ssize_t count = recv(fd, bytes + received, length - received, 0);
        if (count == 0)
            return -1;
        if (count < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        received += (size_t)count;
    }

    return 0;
}

int recv_packet(int fd, PacketHeader *header, char **body)
{
    unsigned char raw[HDR_LEN];

    if (recv_exact(fd, raw, HDR_LEN) < 0)
        return -1;

    header_decode(raw, header);

    char *buffer = xmalloc((size_t)header->length + 1);
    if (header->length > 0 &&
        recv_exact(fd, buffer, header->length) < 0) {
        free(buffer);
        return -1;
    }

    buffer[header->length] = '\0';
    *body = buffer;
    return 0;
}

int send_packet(int fd, uint8_t type, uint32_t sender_id,
                const void *body, size_t length)
{
    if (length > MAX_BODY)
        return -1;

    PacketHeader header = {
        .type = type,
        .sender_id = sender_id,
        .length = (uint16_t)length,
    };

    unsigned char raw[HDR_LEN];
    header_encode(&header, raw);

    unsigned char *buffer = xmalloc(HDR_LEN + length);
    memcpy(buffer, raw, HDR_LEN);
    if (length > 0)
        memcpy(buffer + HDR_LEN, body, length);

    size_t total = HDR_LEN + length;
    size_t sent = 0;
    int result = 0;

    while (sent < total) {
        ssize_t count = send(fd, buffer + sent, total - sent, 0);
        if (count < 0) {
            if (errno == EINTR)
                continue;
            result = -1;
            break;
        }
        if (count == 0) {
            result = -1;
            break;
        }
        sent += (size_t)count;
    }

    free(buffer);
    return result;
}
