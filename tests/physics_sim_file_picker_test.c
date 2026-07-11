#include "app/platform/physics_sim_file_picker.h"

#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static bool write_fake_picker(const char *path, const char *body) {
    FILE *file = fopen(path, "w");
    if (!file) return false;
    if (fputs(body, file) == EOF || fclose(file) != 0) return false;
    return chmod(path, 0700) == 0;
}

static bool read_text(const char *path, char *out_text, size_t out_text_size) {
    FILE *file = fopen(path, "r");
    size_t read_count = 0u;
    if (!file || !out_text || out_text_size == 0u) return false;
    read_count = fread(out_text, 1u, out_text_size - 1u, file);
    out_text[read_count] = '\0';
    (void)fclose(file);
    return true;
}

static bool setup_fixture(char *root, char *zenity, char *kdialog, char *args_log, char *marker) {
    char root_template[] = "/tmp/physics_sim_file_picker_XXXXXX";
    const char *zenity_script =
        "#!/bin/sh\n"
        "printf '%s\\n' \"$@\" > \"$PHYSICS_SIM_FILE_PICKER_ARGS_LOG\"\n"
        "case \"$PHYSICS_SIM_FILE_PICKER_TEST_ZENITY\" in\n"
        "  selected) printf '%s\\n' \"$PHYSICS_SIM_FILE_PICKER_SELECTED_PATH\"; exit 0 ;;\n"
        "  cancelled) exit 1 ;;\n"
        "  unavailable) exit 127 ;;\n"
        "  *) exit 2 ;;\n"
        "esac\n";
    const char *kdialog_script =
        "#!/bin/sh\n"
        ": > \"$PHYSICS_SIM_FILE_PICKER_KDIALOG_MARKER\"\n"
        "printf '%s\\n' \"$@\" > \"$PHYSICS_SIM_FILE_PICKER_ARGS_LOG\"\n"
        "printf '%s\\n' \"$PHYSICS_SIM_FILE_PICKER_KDIALOG_PATH\"\n";
    char *created_root = mkdtemp(root_template);
    if (!created_root || snprintf(root, PATH_MAX, "%s", created_root) >= PATH_MAX ||
        snprintf(zenity, PATH_MAX, "%s/zenity", root) >= PATH_MAX ||
        snprintf(kdialog, PATH_MAX, "%s/kdialog", root) >= PATH_MAX ||
        snprintf(args_log, PATH_MAX, "%s/args.log", root) >= PATH_MAX ||
        snprintf(marker, PATH_MAX, "%s/kdialog-ran", root) >= PATH_MAX) return false;
    return write_fake_picker(zenity, zenity_script) && write_fake_picker(kdialog, kdialog_script);
}

static void remove_fixture(const char *root, const char *zenity, const char *kdialog, const char *args_log, const char *marker) {
    (void)unlink(zenity); (void)unlink(kdialog); (void)unlink(args_log); (void)unlink(marker); (void)rmdir(root);
}

static bool check(bool condition, const char *name) {
    if (!condition) fprintf(stderr, "physics_sim_file_picker_test: failed: %s\n", name);
    return condition;
}

static bool test_folder_zenity_arguments(void) {
    char root[PATH_MAX], zenity[PATH_MAX], kdialog[PATH_MAX], args_log[PATH_MAX], marker[PATH_MAX], output[PATH_MAX], args[2048];
    bool passed = setup_fixture(root, zenity, kdialog, args_log, marker);
    passed = passed && setenv("PATH", root, 1) == 0 && setenv("PHYSICS_SIM_FILE_PICKER_ARGS_LOG", args_log, 1) == 0 && setenv("PHYSICS_SIM_FILE_PICKER_KDIALOG_MARKER", marker, 1) == 0 && setenv("PHYSICS_SIM_FILE_PICKER_TEST_ZENITY", "selected", 1) == 0 && setenv("PHYSICS_SIM_FILE_PICKER_SELECTED_PATH", "/tmp/selected folder", 1) == 0;
    passed = passed && PhysicsSim_FilePicker_SelectFolder("Choose input", "/tmp/start folder", output, sizeof(output)) == PHYSICS_SIM_FILE_PICKER_SELECTED && strcmp(output, "/tmp/selected folder") == 0 && access(marker, F_OK) != 0 && read_text(args_log, args, sizeof(args)) && strstr(args, "--file-selection\n--directory\n--title\nChoose input\n--filename\n/tmp/start folder\n") != NULL;
    if (!passed) check(false, "folder zenity arguments");
    remove_fixture(root, zenity, kdialog, args_log, marker);
    return passed;
}

static bool test_file_kdialog_fallback(void) {
    char root[PATH_MAX], zenity[PATH_MAX], kdialog[PATH_MAX], args_log[PATH_MAX], marker[PATH_MAX], output[PATH_MAX], args[2048];
    bool passed = setup_fixture(root, zenity, kdialog, args_log, marker);
    passed = passed && setenv("PATH", root, 1) == 0 && setenv("PHYSICS_SIM_FILE_PICKER_ARGS_LOG", args_log, 1) == 0 && setenv("PHYSICS_SIM_FILE_PICKER_KDIALOG_MARKER", marker, 1) == 0 && setenv("PHYSICS_SIM_FILE_PICKER_TEST_ZENITY", "unavailable", 1) == 0 && setenv("PHYSICS_SIM_FILE_PICKER_KDIALOG_PATH", "/tmp/warm-start.vf3d", 1) == 0;
    passed = passed && PhysicsSim_FilePicker_SelectFile("Choose warm start", "/tmp/cache", output, sizeof(output)) == PHYSICS_SIM_FILE_PICKER_SELECTED && strcmp(output, "/tmp/warm-start.vf3d") == 0 && access(marker, F_OK) == 0 && read_text(args_log, args, sizeof(args)) && strstr(args, "--getopenfilename\n/tmp/cache\n--title\nChoose warm start\n") != NULL;
    if (!passed) check(false, "file kdialog fallback");
    remove_fixture(root, zenity, kdialog, args_log, marker);
    return passed;
}

static bool test_cancelled_zenity_stops_fallback(void) {
    char root[PATH_MAX], zenity[PATH_MAX], kdialog[PATH_MAX], args_log[PATH_MAX], marker[PATH_MAX], output[PATH_MAX];
    bool passed = setup_fixture(root, zenity, kdialog, args_log, marker);
    passed = passed && setenv("PATH", root, 1) == 0 && setenv("PHYSICS_SIM_FILE_PICKER_ARGS_LOG", args_log, 1) == 0 && setenv("PHYSICS_SIM_FILE_PICKER_KDIALOG_MARKER", marker, 1) == 0 && setenv("PHYSICS_SIM_FILE_PICKER_TEST_ZENITY", "cancelled", 1) == 0;
    passed = passed && PhysicsSim_FilePicker_SelectFile("Cancel", NULL, output, sizeof(output)) == PHYSICS_SIM_FILE_PICKER_CANCELLED && output[0] == '\0' && access(marker, F_OK) != 0;
    if (!passed) check(false, "cancelled zenity");
    remove_fixture(root, zenity, kdialog, args_log, marker);
    return passed;
}

static bool test_missing_linux_picker_reports_unavailable(void) {
    char root_template[] = "/tmp/physics_sim_file_picker_empty_XXXXXX";
    char output[PATH_MAX];
    char *root = mkdtemp(root_template);
    bool passed = root != NULL && setenv("PATH", root, 1) == 0 &&
                  PhysicsSim_FilePicker_SelectFolder("Unavailable", NULL, output, sizeof(output)) ==
                      PHYSICS_SIM_FILE_PICKER_UNAVAILABLE &&
                  output[0] == '\0';
    if (!passed) check(false, "missing Linux picker");
    if (root) (void)rmdir(root);
    return passed;
}

int main(void) {
    return test_folder_zenity_arguments() && test_file_kdialog_fallback() &&
                   test_cancelled_zenity_stops_fallback() && test_missing_linux_picker_reports_unavailable()
               ? 0
               : 1;
}
