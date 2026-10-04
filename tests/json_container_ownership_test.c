/* Exercise the JSON dependency, including nested and shared container ownership.
 * Numerical live-byte counters cannot observe storage owned by json-c. */
#include <assert.h>
#include <json-c/json.h>
#include <stdio.h>
#ifdef __APPLE__
#include <malloc/malloc.h>
static size_t live_bytes(void) {
    malloc_statistics_t stats = {0};
    malloc_zone_statistics(NULL, &stats);
    return stats.size_in_use;
}
#endif
static void cycle(void) {
    struct json_object *root = json_object_new_object(), *samples = json_object_new_array();
    struct json_object *health = json_object_new_object();
    json_object_object_add(health, "pressure_pa", json_object_new_double(.01));
    json_object_object_add(root, "health", json_object_get(health));
    for (int cell = 0; cell < 16; cell++) {
        struct json_object *row = json_object_new_array();
        for (int field = 0; field < 11; field++)
            json_object_array_add(row, json_object_new_double(field));
        json_object_array_add(samples, row);
    }
    json_object_object_add(root, "samples", samples);
    assert(json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN));
    json_object_put(root);
    struct json_object *pressure = NULL;
    assert(json_object_object_get_ex(health, "pressure_pa", &pressure));
    assert(json_object_get_double(pressure) == .01);
    json_object_put(health);
}
int main(void) {
    for (int i = 0; i < 100; i++)
        cycle();
#ifdef __APPLE__
    size_t before = live_bytes();
#endif
    for (int i = 0; i < 2000; i++)
        cycle();
#ifdef __APPLE__
    size_t after = live_bytes();
    size_t growth = after > before ? after - before : 0;
    printf("{\"json_version\":\"%s\",\"cycles\":2000,\"live_growth_bytes\":%zu,"
           "\"limit_bytes\":1048576}\n", json_c_version(), growth);
    fflush(stdout);
    assert(growth < 1024 * 1024);
#else
    printf("{\"json_version\":\"%s\",\"cycles\":2000,"
           "\"memory_growth_screen\":\"macOS_only\"}\n", json_c_version());
#endif
    return 0;
}
