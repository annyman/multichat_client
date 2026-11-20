#include "protocol.h"

void header_encode(const PacketHeader *header, unsigned char out[HDR_LEN])
{
    out[0] = header->type;
    out[1] = (unsigned char)(header->sender_id >> 24);
    out[2] = (unsigned char)(header->sender_id >> 16);
    out[3] = (unsigned char)(header->sender_id >> 8);
    out[4] = (unsigned char)header->sender_id;
    out[5] = (unsigned char)(header->length >> 8);
    out[6] = (unsigned char)header->length;
}

void header_decode(const unsigned char in[HDR_LEN], PacketHeader *header)
{
    header->type = in[0];
    header->sender_id = ((uint32_t)in[1] << 24) |
                        ((uint32_t)in[2] << 16) |
                        ((uint32_t)in[3] << 8) |
                        (uint32_t)in[4];
    header->length = (uint16_t)(((uint16_t)in[5] << 8) |
                                (uint16_t)in[6]);
}
