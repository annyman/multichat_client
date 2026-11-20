#ifndef MULTICHAT_NET_H
#define MULTICHAT_NET_H

#include <stddef.h>
#include <stdint.h>

#include "protocol.h"

/* Connects to host:port. Returns a socket fd, or -1 on failure. */
int net_connect(const char *host, int port);
int net_set_nonblocking(int fd);

/* Reads exactly n bytes. Returns 0 on success, -1 on close/error. */
int recv_exact(int fd, void *buffer, size_t length);

/*
 * Reads one packet. On success stores the header and a NUL-terminated,
 * malloc'd body (caller frees). Returns 0 on success, -1 on close/error.
 */
int recv_packet(int fd, PacketHeader *header, char **body);

/* Sends one packet. Returns 0 on success, -1 on failure or oversized body. */
int send_packet(int fd, uint8_t type, uint32_t sender_id,
                const void *body, size_t length);

#endif
