#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    ML_REGISTER_H2_SIZE = 16384,
    ML_REGISTER_RESPONSE_SIZE = 8192,
    ML_REGISTER_FRAME_SIZE = 4096,
    ML_REGISTER_RESPONSE_OFFSET = ML_REGISTER_H2_SIZE,
    ML_REGISTER_FRAME_OFFSET = ML_REGISTER_RESPONSE_OFFSET + ML_REGISTER_RESPONSE_SIZE,
    ML_REGISTER_WORKSPACE_SIZE = ML_REGISTER_FRAME_OFFSET + ML_REGISTER_FRAME_SIZE,
    ML_REGISTER_SHARED_MIN_CAPACITY = 32768
};

typedef struct {
    uint8_t *h2;
    uint8_t *response;
    uint8_t *frame;
} ml_register_workspace;

/* Registration and the initial map run sequentially on the coord task. */
static inline bool ml_register_workspace_init(ml_register_workspace *workspace,
        uint8_t *storage, size_t capacity) {
    if (!workspace || !storage || capacity < ML_REGISTER_SHARED_MIN_CAPACITY) return false;
    workspace->h2 = storage;
    workspace->response = storage + ML_REGISTER_RESPONSE_OFFSET;
    workspace->frame = storage + ML_REGISTER_FRAME_OFFSET;
    return true;
}
