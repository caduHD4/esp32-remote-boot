#include "ml_json_scan.h"

#include <string.h>

static size_t skip_space(ml_json_slice_t input, size_t pos) {
    while (pos < input.len &&
           (input.ptr[pos] == ' ' || input.ptr[pos] == '\t' ||
            input.ptr[pos] == '\r' || input.ptr[pos] == '\n')) {
        pos++;
    }
    return pos;
}

static bool skip_string(ml_json_slice_t input, size_t start, size_t *end) {
    if (start >= input.len || input.ptr[start] != '"') return false;
    for (size_t pos = start + 1; pos < input.len; pos++) {
        unsigned char ch = (unsigned char)input.ptr[pos];
        if (ch < 0x20) return false;
        if (ch == '\\') {
            if (++pos >= input.len) return false;
            char escape = input.ptr[pos];
            if (escape == 'u') {
                for (int i = 0; i < 4; i++) {
                    if (++pos >= input.len) return false;
                    char hex = input.ptr[pos];
                    if (!((hex >= '0' && hex <= '9') ||
                          (hex >= 'a' && hex <= 'f') ||
                          (hex >= 'A' && hex <= 'F'))) return false;
                }
            } else if (!strchr("\"\\/bfnrt", escape)) {
                return false;
            }
        } else if (ch == '"') {
            *end = pos + 1;
            return true;
        }
    }
    return false;
}

static bool skip_value_depth(ml_json_slice_t input, size_t start, size_t *end,
                             unsigned depth);

static bool skip_number(ml_json_slice_t input, size_t pos, size_t *end) {
    if (pos < input.len && input.ptr[pos] == '-') pos++;
    if (pos >= input.len) return false;
    if (input.ptr[pos] == '0') {
        pos++;
    } else {
        if (input.ptr[pos] < '1' || input.ptr[pos] > '9') return false;
        while (pos < input.len && input.ptr[pos] >= '0' && input.ptr[pos] <= '9') pos++;
    }
    if (pos < input.len && input.ptr[pos] == '.') {
        pos++;
        if (pos >= input.len || input.ptr[pos] < '0' || input.ptr[pos] > '9') return false;
        while (pos < input.len && input.ptr[pos] >= '0' && input.ptr[pos] <= '9') pos++;
    }
    if (pos < input.len && (input.ptr[pos] == 'e' || input.ptr[pos] == 'E')) {
        pos++;
        if (pos < input.len && (input.ptr[pos] == '+' || input.ptr[pos] == '-')) pos++;
        if (pos >= input.len || input.ptr[pos] < '0' || input.ptr[pos] > '9') return false;
        while (pos < input.len && input.ptr[pos] >= '0' && input.ptr[pos] <= '9') pos++;
    }
    *end = pos;
    return true;
}

static bool skip_array(ml_json_slice_t input, size_t pos, size_t *end,
                       unsigned depth) {
    pos = skip_space(input, pos + 1);
    if (pos < input.len && input.ptr[pos] == ']') { *end = pos + 1; return true; }
    for (;;) {
        if (!skip_value_depth(input, pos, &pos, depth + 1)) return false;
        pos = skip_space(input, pos);
        if (pos >= input.len) return false;
        if (input.ptr[pos] == ']') { *end = pos + 1; return true; }
        if (input.ptr[pos++] != ',') return false;
        pos = skip_space(input, pos);
        if (pos >= input.len || input.ptr[pos] == ']') return false;
    }
}

static bool skip_object(ml_json_slice_t input, size_t pos, size_t *end,
                        unsigned depth) {
    pos = skip_space(input, pos + 1);
    if (pos < input.len && input.ptr[pos] == '}') { *end = pos + 1; return true; }
    for (;;) {
        if (!skip_string(input, pos, &pos)) return false;
        pos = skip_space(input, pos);
        if (pos >= input.len || input.ptr[pos++] != ':') return false;
        if (!skip_value_depth(input, pos, &pos, depth + 1)) return false;
        pos = skip_space(input, pos);
        if (pos >= input.len) return false;
        if (input.ptr[pos] == '}') { *end = pos + 1; return true; }
        if (input.ptr[pos++] != ',') return false;
        pos = skip_space(input, pos);
        if (pos >= input.len || input.ptr[pos] == '}') return false;
    }
}

static bool skip_value_depth(ml_json_slice_t input, size_t start, size_t *end,
                             unsigned depth) {
    if (depth > 32) return false;
    size_t pos = skip_space(input, start);
    if (pos >= input.len) return false;

    if (input.ptr[pos] == '"') return skip_string(input, pos, end);
    if (input.ptr[pos] == '{') return skip_object(input, pos, end, depth);
    if (input.ptr[pos] == '[') return skip_array(input, pos, end, depth);
    if (input.len - pos >= 4 && memcmp(input.ptr + pos, "true", 4) == 0) {
        *end = pos + 4; return true;
    }
    if (input.len - pos >= 5 && memcmp(input.ptr + pos, "false", 5) == 0) {
        *end = pos + 5; return true;
    }
    if (input.len - pos >= 4 && memcmp(input.ptr + pos, "null", 4) == 0) {
        *end = pos + 4; return true;
    }
    return skip_number(input, pos, end);
}

static bool skip_value(ml_json_slice_t input, size_t start, size_t *end) {
    return skip_value_depth(input, start, end, 0);
}

static bool string_key_equals(ml_json_slice_t input, size_t start,
                              size_t end, const char *key) {
    size_t key_len = strlen(key);
    return end >= start + 2 && end - start - 2 == key_len &&
           memcmp(input.ptr + start + 1, key, key_len) == 0;
}

bool ml_json_object_get(ml_json_slice_t object, const char *key,
                        ml_json_slice_t *value) {
    if (!key || !value) return false;
    size_t pos = skip_space(object, 0);
    if (pos >= object.len || object.ptr[pos] != '{') return false;
    size_t object_end = 0;
    if (!skip_value(object, pos, &object_end) ||
        skip_space(object, object_end) != object.len) {
        return false;
    }
    pos++;

    for (;;) {
        pos = skip_space(object, pos);
        if (pos >= object.len || object.ptr[pos] == '}') return false;
        size_t key_start = pos;
        size_t key_end = 0;
        if (!skip_string(object, key_start, &key_end)) return false;
        pos = skip_space(object, key_end);
        if (pos >= object.len || object.ptr[pos++] != ':') return false;
        pos = skip_space(object, pos);
        size_t value_start = pos;
        size_t value_end = 0;
        if (!skip_value(object, value_start, &value_end)) return false;
        if (string_key_equals(object, key_start, key_end, key)) {
            value->ptr = object.ptr + value_start;
            value->len = value_end - value_start;
            return true;
        }
        pos = skip_space(object, value_end);
        if (pos >= object.len || object.ptr[pos] != ',') return false;
        pos++;
    }
}

bool ml_json_array_next(ml_json_slice_t array, size_t *cursor,
                        ml_json_slice_t *value) {
    if (!cursor || !value) return false;
    size_t pos = *cursor;
    if (pos == 0) {
        pos = skip_space(array, 0);
        if (pos >= array.len || array.ptr[pos++] != '[') return false;
        size_t array_end = 0;
        if (!skip_value(array, pos - 1, &array_end) ||
            skip_space(array, array_end) != array.len) return false;
    } else {
        pos = skip_space(array, pos);
        if (pos < array.len && array.ptr[pos] == ',') pos++;
    }
    pos = skip_space(array, pos);
    if (pos >= array.len || array.ptr[pos] == ']') return false;

    size_t end = 0;
    if (!skip_value(array, pos, &end)) return false;
    value->ptr = array.ptr + pos;
    value->len = end - pos;
    *cursor = end;
    return true;
}
