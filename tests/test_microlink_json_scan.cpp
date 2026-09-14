#include <cassert>
#include <cstddef>
#include <cstring>

extern "C" {
#include "ml_json_scan.h"
}

static bool equals(ml_json_slice_t slice, const char *expected) {
    return slice.len == std::strlen(expected) &&
           std::memcmp(slice.ptr, expected, slice.len) == 0;
}

int main() {
    const char json[] =
        "{\"Noise\":{\"text\":\"fake \\\"Node\\\": { brace }\"},"
        "\"Node\":{\"HomeDERP\":9,\"Addresses\":[\"100.64.0.7/32\"]},"
        "\"Peers\":[{\"Name\":\"one\",\"Nested\":{\"x\":1}},"
        "{\"Name\":\"two\",\"Enabled\":true}],"
        "\"DERPMap\":{\"Regions\":{\"1\":{\"RegionID\":1},"
        "\"9\":{\"RegionID\":9,\"Nodes\":[{\"HostName\":\"derp9\"}]}}}}";

    ml_json_slice_t root{json, sizeof(json) - 1};
    ml_json_slice_t node{};
    assert(ml_json_object_get(root, "Node", &node));
    assert(equals(node, "{\"HomeDERP\":9,\"Addresses\":[\"100.64.0.7/32\"]}"));

    ml_json_slice_t peers{};
    assert(ml_json_object_get(root, "Peers", &peers));
    size_t cursor = 0;
    ml_json_slice_t peer{};
    assert(ml_json_array_next(peers, &cursor, &peer));
    assert(equals(peer, "{\"Name\":\"one\",\"Nested\":{\"x\":1}}"));
    assert(ml_json_array_next(peers, &cursor, &peer));
    assert(equals(peer, "{\"Name\":\"two\",\"Enabled\":true}"));
    assert(!ml_json_array_next(peers, &cursor, &peer));

    ml_json_slice_t derp_map{};
    ml_json_slice_t regions{};
    ml_json_slice_t home_region{};
    assert(ml_json_object_get(root, "DERPMap", &derp_map));
    assert(ml_json_object_get(derp_map, "Regions", &regions));
    assert(ml_json_object_get(regions, "9", &home_region));
    assert(equals(home_region,
                  "{\"RegionID\":9,\"Nodes\":[{\"HostName\":\"derp9\"}]}"));

    ml_json_slice_t absent{};
    assert(!ml_json_object_get(root, "Missing", &absent));

    const char malformed[] = "{\"Node\":{\"unterminated\":true}";
    ml_json_slice_t malformed_root{malformed, sizeof(malformed) - 1};
    assert(!ml_json_object_get(malformed_root, "Node", &node));

    const char missing_comma[] = "[{\"Name\":\"one\"}{\"Name\":\"two\"}]";
    ml_json_slice_t missing_comma_array{missing_comma, sizeof(missing_comma) - 1};
    cursor = 0;
    assert(!ml_json_array_next(missing_comma_array, &cursor, &peer));

    const char trailing_comma[] = "[{\"Name\":\"one\"},]";
    ml_json_slice_t trailing_comma_array{trailing_comma, sizeof(trailing_comma) - 1};
    cursor = 0;
    assert(!ml_json_array_next(trailing_comma_array, &cursor, &peer));

    const char invalid_primitive[] = "{\"Node\":not-json}";
    ml_json_slice_t invalid_primitive_root{invalid_primitive, sizeof(invalid_primitive) - 1};
    assert(!ml_json_object_get(invalid_primitive_root, "Node", &node));
}
