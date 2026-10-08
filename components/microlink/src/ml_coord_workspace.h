#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
typedef struct { uint8_t *storage;size_t capacity; } ml_coord_workspace;
static inline bool ml_coord_workspace_acquire(ml_coord_workspace *lease,size_t size,
        void *(*allocate)(size_t)) {
    if(!lease||!size||!allocate) return false;
    if(lease->storage) return lease->capacity>=size;
    lease->storage=(uint8_t*)allocate(size);
    if(!lease->storage) return false;
    lease->capacity=size;
    return true;
}
static inline void ml_coord_workspace_release(ml_coord_workspace *lease) {
    if(!lease) return;
    free(lease->storage);lease->storage=NULL;lease->capacity=0;
}
