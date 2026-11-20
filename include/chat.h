#ifndef MULTICHAT_CHAT_H
#define MULTICHAT_CHAT_H

#include <stddef.h>
#include <stdint.h>

#include "history.h"
#include "roster.h"

/*
 * Applies one incoming packet to the roster and history, mirroring the
 * Python client's process_packet().
 */
void chat_process_packet(uint8_t type, uint32_t sender_id,
                         const char *body, size_t length,
                         Roster *roster, uint32_t client_id,
                         History *history);

#endif
