#include "app/platform/physics_sim_file_picker.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#if defined(__APPLE__) && !defined(PHYSICS_SIM_FILE_PICKER_FORCE_LINUX)
#define PHYSICS_SIM_FILE_PICKER_MACOS 1
#else
#define PHYSICS_SIM_FILE_PICKER_MACOS 0
#endif

typedef enum {
    PHYSICS_SIM_FILE_PICKER_FOLDER = 0,
    PHYSICS_SIM_FILE_PICKER_FILE
} PhysicsSimFilePickerKind;

static void trim_dialog_newline(char *text) {
    size_t length = 0u;
    if (!text) return;
    length = strlen(text);
    while (length > 0u && (text[length - 1u] == '\n' || text[length - 1u] == '\r')) {
        text[--length] = '\0';
    }
}

static PhysicsSimFilePickerResult run_picker(const char *const argv[],
                                             char *out_path,
                                             size_t out_path_size) {
    int pipe_fds[2] = {-1, -1};
    pid_t child = 0;
    ssize_t bytes_read = 0;
    size_t used = 0u;
    int wait_status = 0;

    if (pipe(pipe_fds) != 0) return PHYSICS_SIM_FILE_PICKER_FAILED;
    child = fork();
    if (child < 0) {
        (void)close(pipe_fds[0]);
        (void)close(pipe_fds[1]);
        return PHYSICS_SIM_FILE_PICKER_FAILED;
    }
    if (child == 0) {
        (void)close(pipe_fds[0]);
        if (dup2(pipe_fds[1], STDOUT_FILENO) < 0) _exit(126);
        (void)close(pipe_fds[1]);
        execvp(argv[0], (char *const *)argv);
        _exit(errno == ENOENT ? 127 : 126);
    }

    (void)close(pipe_fds[1]);
    while (used + 1u < out_path_size &&
           (bytes_read = read(pipe_fds[0], out_path + used, out_path_size - used - 1u)) > 0) {
        used += (size_t)bytes_read;
    }
    (void)close(pipe_fds[0]);
    out_path[used] = '\0';
    if (waitpid(child, &wait_status, 0) < 0) return PHYSICS_SIM_FILE_PICKER_FAILED;
    if (!WIFEXITED(wait_status)) return PHYSICS_SIM_FILE_PICKER_FAILED;
    if (WEXITSTATUS(wait_status) == 127) return PHYSICS_SIM_FILE_PICKER_UNAVAILABLE;
    if (WEXITSTATUS(wait_status) == 1) return PHYSICS_SIM_FILE_PICKER_CANCELLED;
    if (WEXITSTATUS(wait_status) != 0) return PHYSICS_SIM_FILE_PICKER_FAILED;
    trim_dialog_newline(out_path);
    return out_path[0] ? PHYSICS_SIM_FILE_PICKER_SELECTED : PHYSICS_SIM_FILE_PICKER_CANCELLED;
}

#if PHYSICS_SIM_FILE_PICKER_MACOS
static bool escape_applescript_literal(const char *input, char *output, size_t output_size) {
    size_t out_index = 0u;
    if (!input || !output || output_size == 0u) return false;
    output[0] = '\0';
    for (size_t index = 0u; input[index] != '\0'; ++index) {
        const char character = input[index];
        if ((character == '\\' || character == '"') && out_index + 2u >= output_size) return false;
        if (character != '\\' && character != '"' && out_index + 1u >= output_size) return false;
        if (character == '\\' || character == '"') output[out_index++] = '\\';
        output[out_index++] = character;
    }
    output[out_index] = '\0';
    return true;
}

static PhysicsSimFilePickerResult select_macos(PhysicsSimFilePickerKind kind,
                                                const char *prompt,
                                                const char *initial_directory,
                                                char *out_path,
                                                size_t out_path_size) {
    char escaped_prompt[512];
    char escaped_directory[2048];
    char script[3072];
    const char *argv[4];
    const char *chooser = kind == PHYSICS_SIM_FILE_PICKER_FOLDER ? "choose folder" : "choose file";

    if (!escape_applescript_literal(prompt, escaped_prompt, sizeof(escaped_prompt))) {
        return PHYSICS_SIM_FILE_PICKER_FAILED;
    }
    if (initial_directory && initial_directory[0]) {
        if (!escape_applescript_literal(initial_directory, escaped_directory, sizeof(escaped_directory))) {
            return PHYSICS_SIM_FILE_PICKER_FAILED;
        }
        snprintf(script, sizeof(script), "POSIX path of (%s with prompt \"%s\" default location POSIX file \"%s\")", chooser, escaped_prompt, escaped_directory);
    } else {
        snprintf(script, sizeof(script), "POSIX path of (%s with prompt \"%s\")", chooser, escaped_prompt);
    }
    argv[0] = "/usr/bin/osascript";
    argv[1] = "-e";
    argv[2] = script;
    argv[3] = NULL;
    return run_picker(argv, out_path, out_path_size);
}
#else
static PhysicsSimFilePickerResult select_linux(PhysicsSimFilePickerKind kind,
                                                const char *prompt,
                                                const char *initial_directory,
                                                char *out_path,
                                                size_t out_path_size) {
    const char *zenity_argv[8] = {"zenity", "--file-selection", NULL, "--title", prompt, NULL, NULL, NULL};
    const char *kdialog_argv[7] = {"kdialog", NULL, NULL, "--title", prompt, NULL, NULL};
    PhysicsSimFilePickerResult result;

    if (kind == PHYSICS_SIM_FILE_PICKER_FOLDER) {
        zenity_argv[2] = "--directory";
        kdialog_argv[1] = "--getexistingdirectory";
    } else {
        kdialog_argv[1] = "--getopenfilename";
    }
    if (initial_directory && initial_directory[0]) {
        zenity_argv[5] = "--filename";
        zenity_argv[6] = initial_directory;
        kdialog_argv[2] = initial_directory;
    } else {
        kdialog_argv[2] = ".";
    }
    result = run_picker(zenity_argv, out_path, out_path_size);
    if (result != PHYSICS_SIM_FILE_PICKER_UNAVAILABLE) return result;
    return run_picker(kdialog_argv, out_path, out_path_size);
}
#endif

static PhysicsSimFilePickerResult select_path(PhysicsSimFilePickerKind kind,
                                              const char *prompt,
                                              const char *initial_directory,
                                              char *out_path,
                                              size_t out_path_size) {
    if (!prompt || !prompt[0] || !out_path || out_path_size < 2u) return PHYSICS_SIM_FILE_PICKER_FAILED;
    out_path[0] = '\0';
#if PHYSICS_SIM_FILE_PICKER_MACOS
    return select_macos(kind, prompt, initial_directory, out_path, out_path_size);
#elif defined(__linux__) || defined(PHYSICS_SIM_FILE_PICKER_FORCE_LINUX)
    return select_linux(kind, prompt, initial_directory, out_path, out_path_size);
#else
    (void)kind;
    (void)initial_directory;
    return PHYSICS_SIM_FILE_PICKER_UNAVAILABLE;
#endif
}

PhysicsSimFilePickerResult PhysicsSim_FilePicker_SelectFolder(const char *prompt,
                                                              const char *initial_directory,
                                                              char *out_path,
                                                              size_t out_path_size) {
    return select_path(PHYSICS_SIM_FILE_PICKER_FOLDER, prompt, initial_directory, out_path, out_path_size);
}

PhysicsSimFilePickerResult PhysicsSim_FilePicker_SelectFile(const char *prompt,
                                                            const char *initial_directory,
                                                            char *out_path,
                                                            size_t out_path_size) {
    return select_path(PHYSICS_SIM_FILE_PICKER_FILE, prompt, initial_directory, out_path, out_path_size);
}
