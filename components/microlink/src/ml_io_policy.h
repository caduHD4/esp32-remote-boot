#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <errno.h>

#define ML_IO_SEND_RETRY_DELAY_MS 10U
#define ML_IO_SEND_MAX_RETRIES 25U
#define ML_IO_SEND_RETRY_WINDOW_MS 250U

static inline bool ml_io_errno_is_transient(int error_number) {
    return error_number == EAGAIN || error_number == EWOULDBLOCK ||
           error_number == EINTR;
}

static inline bool ml_io_send_should_retry(int error_number,
                                           unsigned int retries,
                                           uint64_t elapsed_ms) {
    return ml_io_errno_is_transient(error_number) &&
           retries < ML_IO_SEND_MAX_RETRIES &&
           elapsed_ms < ML_IO_SEND_RETRY_WINDOW_MS;
}
