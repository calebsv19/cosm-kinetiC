#define _DARWIN_C_SOURCE
#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "app/physics_sim_job_startup.h"
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
static long long now_ms(void) {
    struct timespec time;
    if (clock_gettime(CLOCK_MONOTONIC, &time) != 0) return -1;
    return (long long)time.tv_sec * 1000 + time.tv_nsec / 1000000;
}
void physics_sim_job_startup_close(PhysicsSimJobStartup *startup) {
    if (startup->reader >= 0) close(startup->reader);
    if (startup->writer >= 0) close(startup->writer);
    startup->reader = startup->writer = -1;
}
bool physics_sim_job_startup_open(PhysicsSimJobStartup *startup) {
    int pipe_fd[2]; startup->reader = startup->writer = -1;
    if (pipe(pipe_fd) != 0) return false;
    startup->reader = fcntl(pipe_fd[0], F_DUPFD_CLOEXEC, 3);
    startup->writer = fcntl(pipe_fd[1], F_DUPFD_CLOEXEC, 3);
    close(pipe_fd[0]); close(pipe_fd[1]);
    if (startup->reader < 0 || startup->writer < 0 ||
        fcntl(startup->reader, F_SETFL, O_NONBLOCK) != 0) {
        physics_sim_job_startup_close(startup); return false;
    }
    return true;
}
void physics_sim_job_startup_child(PhysicsSimJobStartup *startup) {
    close(startup->reader); startup->reader = -1;
}
void physics_sim_job_startup_error(PhysicsSimJobStartup *startup, int phase, int error) {
    int message[2] = {phase, error};
    ssize_t written;
    do { written = write(startup->writer, message, sizeof(message)); } while (written < 0 && errno == EINTR);
    physics_sim_job_startup_close(startup);
}
static bool observe(pid_t pid, bool *reaped, int *status) {
    if (*reaped) return true;
    pid_t result;
    do { result = waitpid(pid, status, WNOHANG); } while (result < 0 && errno == EINTR);
    if (result == pid) *reaped = true;
    return result >= 0;
}
static bool reap_failed(pid_t pid, bool *reaped, int *status) {
    if (!observe(pid, reaped, status)) return false;
    if (*reaped) return true;
    /* An unreaped direct child reserves its PID; no saved-PID signal is used. */
    if (kill(pid, SIGKILL) != 0 && errno != ESRCH) return false;
    long long start = now_ms();
    if (start < 0) return false;
    for (;;) {
        if (!observe(pid, reaped, status)) return false;
        if (*reaped) return true;
        long long now = now_ms();
        if (now < 0 || now - start >= 1000) return false;
        struct timespec pause = {0, 10000000}; nanosleep(&pause, NULL);
    }
}
bool physics_sim_job_startup_wait(PhysicsSimJobStartup *startup, pid_t pid, int timeout_ms,
    char *diagnostics, size_t size) {
    if (startup->writer >= 0) close(startup->writer);
    startup->writer = -1;
    bool reaped = false, accepted = false;
    int status = 0, message[2] = {0}; size_t received = 0;
    const char *reason = "startup observation held";
    long long start = now_ms();
    if (pid <= 0 || startup->reader < 0 || timeout_ms <= 0 || timeout_ms > 10000 || start < 0 ||
        !observe(pid, &reaped, &status)) {
        snprintf(diagnostics, size, "startup observation held: invalid channel/budget or child ownership");
        physics_sim_job_startup_close(startup); return false;
    }
    for (;;) {
        ssize_t count = read(startup->reader, (char *)message + received, sizeof(message) - received);
        if (count > 0) {
            received += (size_t)count;
            if (received == sizeof(message)) { reason = "child setup/exec failed"; break; }
        } else if (count == 0) {
            if (received) { reason = "incomplete startup error record"; break; }
            if (!observe(pid, &reaped, &status)) { reason = "child ownership observation failed"; break; }
            if (reaped && (!WIFEXITED(status) || WEXITSTATUS(status) != 0)) { reason = "child exited during startup"; break; }
            accepted = true; break;
        } else if (errno != EAGAIN && errno != EINTR) { reason = "startup channel read failed"; break; }
        long long now = now_ms();
        if (now < 0 || now - start >= timeout_ms) { reason = "startup observation timed out"; break; }
        struct pollfd channel = {startup->reader, POLLIN | POLLHUP, 0};
        int remaining = timeout_ms - (int)(now - start);
        int result = poll(&channel, 1, remaining);
        if (result < 0 && errno != EINTR) { reason = "startup channel poll failed"; break; }
    }
    physics_sim_job_startup_close(startup);
    if (accepted) { snprintf(diagnostics, size, "exec channel closed without setup error"); return true; }
    bool cleaned = reap_failed(pid, &reaped, &status);
    snprintf(diagnostics, size, "%s; phase=%d errno=%d; direct_child_reaped=%s", reason,
        message[0], message[1], cleaned ? "true" : "false");
    return false;
}
