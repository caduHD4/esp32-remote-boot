#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>

// Connection-owned framing. No I/O, retries, or hidden unbounded allocation.
typedef enum { ML_FRAME_DERP, ML_FRAME_NOISE, ML_FRAME_H2, ML_FRAME_MAP } ml_frame_kind;
typedef struct {
    uint8_t header[9];
    size_t header_used, length, used;
    uint8_t *payload;
} ml_frame_reader;

static inline void ml_frame_reset(ml_frame_reader *r) {
    free(r->payload);
    memset(r, 0, sizeof(*r));
}

// Returns one complete frame, incomplete, or invalid/allocation failure.
// Caller must reset after consuming a complete frame, and on disconnect.
static inline int ml_frame_feed(ml_frame_reader *r, ml_frame_kind kind,
        const uint8_t *data, size_t size, size_t *consumed, size_t maximum) {
    size_t header_size = kind == ML_FRAME_DERP ? 5 : kind == ML_FRAME_H2 ? 9 :
                         kind == ML_FRAME_MAP ? 4 : 3;
    *consumed = 0;
    while (r->header_used < header_size && *consumed < size)
        r->header[r->header_used++] = data[(*consumed)++];
    if (r->header_used < header_size) return 0;
    if (!r->payload) {
        const uint8_t *h = r->header;
        r->length = kind == ML_FRAME_DERP ? ((uint32_t)h[1]<<24)|((uint32_t)h[2]<<16)|((uint32_t)h[3]<<8)|h[4] :
                    kind == ML_FRAME_MAP ? h[0]|((uint32_t)h[1]<<8)|((uint32_t)h[2]<<16)|((uint32_t)h[3]<<24) :
                    kind == ML_FRAME_H2 ? ((uint32_t)h[0]<<16)|((uint32_t)h[1]<<8)|h[2] :
                    ((uint32_t)h[1]<<8)|h[2];
        if (r->length > maximum || (kind == ML_FRAME_NOISE && (h[0] != 4 || r->length < 16)) ||
            (kind == ML_FRAME_MAP && r->length == 0)) return -1;
        r->payload = (uint8_t *)malloc(r->length + 1);
        if (!r->payload) return -1;
        r->payload[r->length] = 0; // Safe bounded JSON parsing without an OOB terminator.
    }
    size_t n = r->length-r->used;
    if (n > size-*consumed) n = size-*consumed;
    if (n) memcpy(r->payload+r->used, data+*consumed, n);
    r->used += n;
    *consumed += n;
    return r->used == r->length ? 1 : 0;
}

static inline int ml_receive_result(int n, int error_number) {
    if (n > 0) return n;
    if (n < 0 && (error_number == EAGAIN || error_number == EWOULDBLOCK || error_number == EINTR)) return 0;
    return -1; // EOF is never a timeout, regardless of stale errno.
}

static inline bool ml_h2_stream_closed(uint8_t type, uint8_t flags, uint32_t stream) {
    return type == 7 || (stream == 5 && (type == 3 || ((type == 0 || type == 1) && (flags & 1))));
}
