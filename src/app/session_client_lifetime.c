#define _POSIX_C_SOURCE 200809L
#include "app/session_client_lifetime.h"
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <sys/wait.h>
#include <unistd.h>

static void *reap_client(void *value) {
    pid_t pid = (pid_t)(intptr_t)value;
    while (waitpid(pid, NULL, 0) < 0 && errno == EINTR) {}
    return NULL;
}

bool physics_sim_session_client_release(pid_t pid) {
    if (pid <= 0) return false;
    /* Confirm direct-child custody before handing off or signalling. */
    int status;
    pid_t observed;
    do { observed = waitpid(pid, &status, WNOHANG); } while (observed < 0 && errno == EINTR);
    if (observed == pid) return true;
    if (observed < 0) return false;

    pthread_attr_t attributes;
    int error = pthread_attr_init(&attributes);
    if (!error) {
        error = pthread_attr_setdetachstate(&attributes, PTHREAD_CREATE_DETACHED);
        if (!error) {
            pthread_t reaper;
            error = pthread_create(&reaper, &attributes, reap_client, (void *)(intptr_t)pid);
        }
        pthread_attr_destroy(&attributes);
        if (!error) return true;
    }
    /* No reaper was created. This unreaped direct child still reserves its PID.
     * Stop only the command client, never its separately owned simulation or
     * a process group. A reply may be unknown; do not replay the command. */
    if (kill(pid, SIGKILL) < 0 && errno != ESRCH) return false;
    do { observed = waitpid(pid, &status, 0); } while (observed < 0 && errno == EINTR);
    return observed == pid;
}
