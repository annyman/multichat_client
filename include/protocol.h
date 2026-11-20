#ifndef MULTICHAT_PROTOCOL_H
#define MULTICHAT_PROTOCOL_H

#include <stdint.h>

#define DEFAULT_PORT 8080

/* Big-endian header: uint8 type, uint32 sender id, uint16 body length. */
#define HDR_LEN 7
#define MAX_BODY 1024
#define NAME_LEN 32
#define HISTORY_LIMIT 2000

#define PKT_MSG 1
#define PKT_SYS 2

_Static_assert(HDR_LEN == 7, "wire header must be 7 bytes");

typedef struct {
    uint8_t type;
    uint32_t sender_id;
    uint16_t length;
} PacketHeader;

void header_encode(const PacketHeader *header, unsigned char out[HDR_LEN]);
void header_decode(const unsigned char in[HDR_LEN], PacketHeader *header);

#endif
