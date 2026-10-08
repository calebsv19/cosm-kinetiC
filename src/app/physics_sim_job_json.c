#define _DARWIN_C_SOURCE
#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#include "app/physics_sim_job_json.h"
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

#define JSON_BYTES (16 * 1024 * 1024)

typedef struct Scan { const char *at, *end; size_t values; } Scan;
static void whitespace(Scan *scan) {
    while (scan->at < scan->end && strchr(" \t\r\n", *scan->at)) ++scan->at;
}
static json_object *token(const char *text, size_t size) {
    if (size > JSON_BYTES) return NULL;
    struct json_tokener *parser = json_tokener_new_ex(64);
    if (!parser) return NULL;
    json_tokener_set_flags(parser, JSON_TOKENER_STRICT | JSON_TOKENER_VALIDATE_UTF8);
    /* A terminating whitespace byte completes numeric/literal tokens. */
    char *terminated = malloc(size + 2);
    if (!terminated) { json_tokener_free(parser); return NULL; }
    memcpy(terminated, text, size); terminated[size] = '\n'; terminated[size+1] = '\0';
    json_object *value = json_tokener_parse_ex(parser, terminated, (int)size+1);
    bool valid = json_tokener_get_error(parser) == json_tokener_success &&
        json_tokener_get_parse_end(parser) == size+1;
    if (value && json_object_is_type(value, json_type_double) && !isfinite(json_object_get_double(value))) valid = false;
    free(terminated); json_tokener_free(parser);
    if (!valid) { if (value) json_object_put(value); return NULL; }
    return value;
}
static json_object *string_token(Scan *scan, size_t limit) {
    if (scan->at == scan->end || *scan->at != '"') return NULL;
    const char *start = scan->at++;
    bool escaped = false, closed = false;
    while (scan->at < scan->end) {
        char c = *scan->at++;
        if (escaped) escaped = false;
        else if (c == '\\') escaped = true;
        else if (c == '"') { closed = true; break; }
        if ((size_t)(scan->at-start) > limit) return NULL;
    }
    if (!closed || (size_t)(scan->at-start) > limit) return NULL;
    json_object *value = token(start, (size_t)(scan->at-start));
    if (!value || !json_object_is_type(value, json_type_string) ||
        (size_t)json_object_get_string_len(value) != strlen(json_object_get_string(value))) {
        if (value) json_object_put(value); return NULL;
    }
    return value;
}
static bool value(Scan *scan, unsigned depth) {
    whitespace(scan);
    if (scan->at == scan->end || depth > 64 || ++scan->values > 100000) return false;
    char kind = *scan->at;
    if (kind == '"') { json_object *text = string_token(scan, 1048576); if (!text) return false; json_object_put(text); return true; }
    if (kind == '{' || kind == '[') {
        ++scan->at; whitespace(scan);
        char closing = kind == '{' ? '}' : ']';
        json_object *keys = kind == '{' ? json_object_new_object() : NULL;
        if (kind == '{' && !keys) return false;
        bool valid = true;
        if (scan->at < scan->end && *scan->at == closing) { ++scan->at; if (keys) json_object_put(keys); return true; }
        for (;;) {
            if (kind == '{') {
                json_object *key = string_token(scan, 4096), *previous = NULL;
                if (!key) { valid = false; break; }
                const char *name = json_object_get_string(key);
                if (json_object_object_get_ex(keys, name, &previous)) { json_object_put(key); valid = false; break; }
                int added = json_object_object_add(keys, name, json_object_new_boolean(true)); json_object_put(key);
                if (added != 0) { valid = false; break; }
                whitespace(scan);
                if (scan->at == scan->end || *scan->at++ != ':') { valid = false; break; }
            }
            if (!value(scan, depth+1)) { valid = false; break; }
            whitespace(scan);
            if (scan->at < scan->end && *scan->at == closing) { ++scan->at; break; }
            if (scan->at == scan->end || *scan->at++ != ',') { valid = false; break; }
            whitespace(scan);
        }
        if (keys) json_object_put(keys); return valid;
    }
    const char *start = scan->at;
    while (scan->at < scan->end && !strchr(" \t\r\n,]}", *scan->at)) ++scan->at;
    size_t length = (size_t)(scan->at-start);
    if (!length || length > 128) return false;
    if (length == 4 && memcmp(start, "null", 4) == 0) return true;
    json_object *leaf = token(start, length);
    if (!leaf) return false;
    bool valid = json_object_is_type(leaf, json_type_boolean) || json_object_is_type(leaf, json_type_int) ||
        (json_object_is_type(leaf, json_type_double) && isfinite(json_object_get_double(leaf)));
    json_object_put(leaf); return valid;
}
static bool admitted_file(const char *path, char *out, size_t size) {
    char absolute[4096], copy[4096], cursor[4096] = "", *part, *save = NULL;
    if (!path || !path[0]) return false;
    if (path[0] == '/') { if (snprintf(absolute, sizeof(absolute), "%s", path) >= (int)sizeof(absolute)) return false; }
    else { char cwd[4096]; if (!getcwd(cwd, sizeof(cwd)) || snprintf(absolute, sizeof(absolute), "%s/%s", cwd, path) >= (int)sizeof(absolute)) return false; }
    snprintf(copy, sizeof(copy), "%s", absolute);
    for (part = strtok_r(copy, "/", &save); part; part = strtok_r(NULL, "/", &save)) {
        if (strcmp(part, ".") == 0) continue;
        if (strcmp(part, "..") == 0 || strcmp(part, ".git") == 0 || strcmp(part, ".ssh") == 0 || strcmp(part, ".aws") == 0) return false;
        size_t n = strlen(cursor), length = strlen(part);
        if (n + length + 2 > sizeof(cursor)) return false;
        cursor[n] = '/'; memcpy(cursor+n+1, part, length+1);
        struct stat st; if (lstat(cursor, &st) != 0) return false;
        if (S_ISLNK(st.st_mode)) {
            char resolved[4096];
            if (!realpath(cursor, resolved) || !((strcmp(cursor, "/tmp") == 0 && strcmp(resolved, "/private/tmp") == 0) ||
                (strcmp(cursor, "/var") == 0 && strcmp(resolved, "/private/var") == 0))) return false;
            snprintf(cursor, sizeof(cursor), "%s", resolved);
        }
    }
    if (!cursor[0] || strlen(cursor) >= size) return false;
    strcpy(out, cursor); return true;
}
static bool unchanged(const struct stat *a, const struct stat *b) {
    if (a->st_dev != b->st_dev || a->st_ino != b->st_ino || a->st_mode != b->st_mode ||
        a->st_size != b->st_size || a->st_nlink != b->st_nlink) return false;
#ifdef __APPLE__
    return a->st_mtimespec.tv_sec == b->st_mtimespec.tv_sec && a->st_mtimespec.tv_nsec == b->st_mtimespec.tv_nsec &&
        a->st_ctimespec.tv_sec == b->st_ctimespec.tv_sec && a->st_ctimespec.tv_nsec == b->st_ctimespec.tv_nsec;
#else
    return a->st_mtim.tv_sec == b->st_mtim.tv_sec && a->st_mtim.tv_nsec == b->st_mtim.tv_nsec &&
        a->st_ctim.tv_sec == b->st_ctim.tv_sec && a->st_ctim.tv_nsec == b->st_ctim.tv_nsec;
#endif
}
int physics_sim_job_json_open_directory(const char *path, char *selected, size_t size) {
    char final_path[4096]; struct stat before, after, final;
    if (!selected || !admitted_file(path, selected, size) || lstat(selected, &before) != 0 || !S_ISDIR(before.st_mode)) return -1;
    int fd = open(selected, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) return -1;
    if (fstat(fd, &after) != 0 || !unchanged(&before, &after) ||
        !admitted_file(path, final_path, sizeof(final_path)) || strcmp(selected, final_path) != 0 ||
        lstat(final_path, &final) != 0 || !unchanged(&after, &final)) { close(fd); return -1; }
    return fd;
}

json_object *physics_sim_job_json_read(const char *path) {
    errno = EINVAL;
    char selected[4096], final_path[4096]; struct stat before, after, final;
    if (!admitted_file(path, selected, sizeof(selected))) return NULL;
    int fd = open(selected, O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) return NULL;
    if (fstat(fd, &before) != 0 || !S_ISREG(before.st_mode) || before.st_nlink != 1 || before.st_size <= 0 || before.st_size > JSON_BYTES) { close(fd); return NULL; }
    size_t size = (size_t)before.st_size, offset = 0; char *text = malloc(size+1);
    if (!text) { close(fd); return NULL; }
    while (offset < size) {
        ssize_t count = read(fd, text+offset, size-offset);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) break;
        offset += (size_t)count;
    }
    bool observed = fstat(fd, &after) == 0 && admitted_file(path, final_path, sizeof(final_path)) &&
        strcmp(selected, final_path) == 0 && lstat(final_path, &final) == 0;
    bool drift = observed && (!unchanged(&before, &after) || !unchanged(&after, &final));
    bool valid = observed && !drift && offset == size;
    close(fd); text[size] = '\0';
    if (!valid) { free(text); errno = drift ? EAGAIN : EINVAL; return NULL; }
    errno = EINVAL;
    json_object *object = physics_sim_job_json_parse(text, size);
    free(text);
    return object;
}

json_object *physics_sim_job_json_parse(const char *text, size_t size) {
    if (!text || !size || size > JSON_BYTES || memchr(text, 0, size)) return NULL;
    Scan scan = {text, text+size, 0};
    bool valid = value(&scan, 1); whitespace(&scan); valid = valid && scan.at == scan.end;
    json_object *object = valid ? token(text, size) : NULL;
    if (object && !json_object_is_type(object, json_type_object)) { json_object_put(object); object = NULL; }
    return object;
}

bool physics_sim_job_json_fields(json_object *root, const PhysicsSimJobJsonField *fields, size_t count) {
    if (!root || !json_object_is_type(root, json_type_object)) return false;
    for (size_t i = 0; i < count; ++i) {
        json_object *value = NULL;
        if (!json_object_object_get_ex(root, fields[i].name, &value)) continue;
        if (!value) return false;
        if (fields[i].type == PHYSICS_JOB_INT) {
            if (!json_object_is_type(value, json_type_int)) return false;
            int64_t number = json_object_get_int64(value);
            if (number < fields[i].minimum || number > fields[i].maximum) return false;
        } else if (fields[i].type == PHYSICS_JOB_BOOL) {
            if (!json_object_is_type(value, json_type_boolean)) return false;
        } else if (fields[i].type == PHYSICS_JOB_STRING) {
            if (!json_object_is_type(value, json_type_string)) return false;
            size_t length = (size_t)json_object_get_string_len(value);
            if ((int64_t)length < fields[i].minimum || (int64_t)length > fields[i].maximum ||
                length != strlen(json_object_get_string(value))) return false;
        } else return false;
    }
    return true;
}
bool physics_sim_job_json_integer(json_object *root, const char *key, int *value) {
    PhysicsSimJobJsonField field = {key, PHYSICS_JOB_INT, INT_MIN, INT_MAX};
    json_object *object = NULL;
    if (value) *value = 0;
    if (!physics_sim_job_json_fields(root, &field, 1) || !json_object_object_get_ex(root, key, &object)) return false;
    if (value) *value = (int)json_object_get_int64(object);
    return true;
}

json_object *physics_sim_job_json_observe(const char *path) {
    struct timespec start, now;
    if (clock_gettime(CLOCK_MONOTONIC, &start) != 0) return NULL;
    for (int attempt = 0; attempt < 8; ++attempt) {
        json_object *object = physics_sim_job_json_read(path);
        if (object || errno != EAGAIN) return object;
        if (clock_gettime(CLOCK_MONOTONIC, &now) != 0 ||
            (now.tv_sec-start.tv_sec)*1000000000LL + now.tv_nsec-start.tv_nsec >= 100000000LL) return NULL;
        struct timespec pause = {0, 1000000}; nanosleep(&pause, NULL);
    }
    return NULL;
}
