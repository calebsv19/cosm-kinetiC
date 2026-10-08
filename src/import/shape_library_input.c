#define _DARWIN_C_SOURCE
#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "import/shape_library_input.h"
#include "import/shape_asset_input.h"
#include "app/physics_sim_job_json.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#define MAX_ASSETS 1024u
#define MAX_ENTRIES 4096u
#define MAX_LIBRARY_BYTES (64u * 1024u * 1024u)
#define MAX_LIBRARY_POINTS 100000u
#define MAX_LIBRARY_PATHS 10000u
typedef struct AssetEntry { char *name; const char *identity; struct stat witness; } AssetEntry;
static bool same(const struct stat *a, const struct stat *b) {
    if (a->st_dev != b->st_dev || a->st_ino != b->st_ino || a->st_mode != b->st_mode ||
        a->st_nlink != b->st_nlink || a->st_size != b->st_size) return false;
#ifdef __APPLE__
    return a->st_mtimespec.tv_sec == b->st_mtimespec.tv_sec && a->st_mtimespec.tv_nsec == b->st_mtimespec.tv_nsec &&
        a->st_ctimespec.tv_sec == b->st_ctimespec.tv_sec && a->st_ctimespec.tv_nsec == b->st_ctimespec.tv_nsec;
#else
    return a->st_mtim.tv_sec == b->st_mtim.tv_sec && a->st_mtim.tv_nsec == b->st_mtim.tv_nsec &&
        a->st_ctim.tv_sec == b->st_ctim.tv_sec && a->st_ctim.tv_nsec == b->st_ctim.tv_nsec;
#endif
}
static int compare(const void *left, const void *right) {
    return strcmp(((const AssetEntry *)left)->name, ((const AssetEntry *)right)->name);
}
static int compare_identity(const void *left, const void *right) {
    return strcmp(((const AssetEntry *)left)->identity, ((const AssetEntry *)right)->identity);
}
bool physics_sim_shape_library_load(const char *path, ShapeAssetLibrary *out_lib) {
    if (!out_lib || out_lib->assets || out_lib->count) return false;
    char selected[4096], final_path[4096], asset_path[4096];
    int fd = physics_sim_job_json_open_directory(path, selected, sizeof(selected));
    if (fd < 0) { fprintf(stderr,"[shape] Directory admission failed\n"); return false; }
    DIR *dir = fdopendir(fd);
    if (!dir) { close(fd); return false; }
    struct stat directory, observed;
    AssetEntry *entries = calloc(MAX_ASSETS, sizeof(*entries));
    ShapeAsset *assets = NULL;
    size_t count = 0, scanned = 0, total_bytes = 0, total_points = 0, total_paths = 0;
    bool ok = entries && fstat(fd, &directory) == 0;
    while (ok) {
        errno = 0; struct dirent *entry = readdir(dir);
        if (!entry) { if (errno) ok = false; break; }
        if (!strcmp(entry->d_name,".") || !strcmp(entry->d_name,"..")) continue;
        if (++scanned > MAX_ENTRIES) { fprintf(stderr,"[shape] Entry budget exceeded\n"); ok = false; break; }
        size_t length = strlen(entry->d_name);
        if (entry->d_name[0] == '.' || length < 5 || strcmp(entry->d_name + length - 5,".json")) continue;
        if (count == MAX_ASSETS) { fprintf(stderr,"[shape] Asset count budget exceeded\n"); ok = false; break; }
        entries[count].name = strdup(entry->d_name);
        if (!entries[count].name) { ok = false; break; }
        count++;
    }
    if (ok && (!count || fstat(fd,&observed) != 0 || !same(&directory,&observed))) ok = false;
    if (ok) qsort(entries,count,sizeof(*entries),compare);
    /* Admit the complete byte inventory before parsing any file. */
    for (size_t i = 0; ok && i < count; ++i) {
        struct stat *st = &entries[i].witness;
        if (fstatat(fd,entries[i].name,st,AT_SYMLINK_NOFOLLOW) != 0 || !S_ISREG(st->st_mode) ||
            st->st_nlink != 1 || st->st_size <= 0 || st->st_size > PHYSICS_SIM_ASSET_MAX_BYTES) { ok = false; break; }
        if ((size_t)st->st_size > MAX_LIBRARY_BYTES - total_bytes) {
            fprintf(stderr,"[shape] File byte budget exceeded\n"); ok = false; break;
        }
        total_bytes += (size_t)st->st_size;
    }
    if (ok) { assets = calloc(count,sizeof(*assets)); ok = assets != NULL; }
    for (size_t i = 0; ok && i < count; ++i) {
        int length = snprintf(asset_path,sizeof(asset_path),"%s/%s",selected,entries[i].name);
        if (length < 0 || (size_t)length >= sizeof(asset_path) ||
            !physics_sim_shape_asset_load(asset_path,&assets[i]) || !assets[i].name || !assets[i].name[0]) { ok = false; break; }
        entries[i].identity = assets[i].name;
        if (assets[i].path_count > MAX_LIBRARY_PATHS - total_paths) {
            fprintf(stderr,"[shape] Path budget exceeded\n"); ok = false; break;
        }
        total_paths += assets[i].path_count;
        for (size_t j = 0; j < assets[i].path_count; ++j) {
            if (assets[i].paths[j].point_count > MAX_LIBRARY_POINTS - total_points) {
                fprintf(stderr,"[shape] Point budget exceeded\n"); ok = false; break;
            }
            total_points += assets[i].paths[j].point_count;
        }
    }
    if (ok) {
        /* Sort only inventory metadata; assets retain their filename-defined IDs. */
        qsort(entries,count,sizeof(*entries),compare_identity);
        for (size_t i = 1; i < count; ++i)
            if (!strcmp(entries[i-1].identity,entries[i].identity)) {
                fprintf(stderr,"[shape] Duplicate asset identity\n"); ok = false; break;
            }
    }
    for (size_t i = 0; ok && i < count; ++i)
        if (fstatat(fd,entries[i].name,&observed,AT_SYMLINK_NOFOLLOW) != 0 || !same(&entries[i].witness,&observed)) ok = false;
    if (ok && (fstat(fd,&observed) != 0 || !same(&directory,&observed))) ok = false;
    int final_fd = ok ? physics_sim_job_json_open_directory(path,final_path,sizeof(final_path)) : -1;
    if (ok && (final_fd < 0 || strcmp(selected,final_path) || fstat(final_fd,&observed) != 0 || !same(&directory,&observed))) ok = false;
    if (final_fd >= 0) close(final_fd);
    if (closedir(dir) != 0) ok = false;
    for (size_t i = 0; i < count; ++i) free(entries[i].name);
    free(entries);
    if (!ok) {
        if (assets) for (size_t i = 0; i < count; ++i) shape_asset_free(&assets[i]);
        free(assets); fprintf(stderr,"[shape] Asset directory refused; no partial library published\n"); return false;
    }
    out_lib->assets = assets; out_lib->count = count;
    return true;
}
