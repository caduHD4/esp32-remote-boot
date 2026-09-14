#pragma once

#include <stdbool.h>
#include <stddef.h>

typedef struct {
    const char *ptr;
    size_t len;
} ml_json_slice_t;

bool ml_json_object_get(ml_json_slice_t object, const char *key,
                        ml_json_slice_t *value);
bool ml_json_array_next(ml_json_slice_t array, size_t *cursor,
                        ml_json_slice_t *value);
