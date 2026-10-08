#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
// Headroom is admission guidance, not an allocation guarantee. A low-memory
// attempt gets a turn after 60s; failed callers retain their retry request.
static inline bool ml_tls_admit(uint32_t now, uint32_t previous, size_t free_heap, size_t largest) {
    uint32_t elapsed = now-previous;
    return elapsed >= 60000 || (elapsed >= 5000 && free_heap >= 60000 && largest >= 24000);
}
static inline uint32_t ml_retry_delay(unsigned attempt) {
    return attempt >= 4 ? 30000 : 2000U << attempt;
}
static inline bool ml_tls_service_window(uint32_t now, uint32_t started) {
    return (uint32_t)(now-started) < 6000;
}
static inline unsigned ml_disco_budget(unsigned processed) { return processed < 2 ? 2-processed : 0; }
static inline bool ml_reply_via_derp(bool via_derp) { return via_derp; }
static inline bool ml_remote_available(bool wifi, bool control, uint32_t authenticated_ms, uint32_t now) {
    return wifi && control && authenticated_ms != 0 && (uint32_t)(now-authenticated_ms) <= 180000;
}
