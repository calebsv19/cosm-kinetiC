#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "app/physics_sim_cli_helpers.h"
#include "app/physics_sim_job_runner.h"

static void usage(const char *argv0) {
    fprintf(stderr,
            "usage: %s <submit|status|cancel> [--request <request.json|job.json>|--job-id <job_id>] [--jobs-root <path>] [--overwrite]\n",
            argv0 ? argv0 : "physics_sim_job_runner");
}

static void print_runner_error(const char *mode,
                               const char *stage,
                               const char *reason,
                               const char *request_path,
                               const char *job_id,
                               const char *jobs_root,
                               const char *action) {
    fprintf(stderr,
            "[physics_sim_job_runner] ERROR mode=%s stage=%s reason=%s\n",
            mode ? mode : "unknown",
            stage ? stage : "unknown",
            reason ? reason : "unknown");
    if (request_path && request_path[0]) {
        fprintf(stderr, "[physics_sim_job_runner]       request_path=%s\n", request_path);
    }
    if (job_id && job_id[0]) {
        fprintf(stderr, "[physics_sim_job_runner]       job_id=%s\n", job_id);
    }
    if (jobs_root && jobs_root[0]) {
        fprintf(stderr, "[physics_sim_job_runner]       jobs_root=%s\n", jobs_root);
    }
    if (action && action[0]) {
        fprintf(stderr, "[physics_sim_job_runner]       action=%s\n", action);
    }
}

int main(int argc, char **argv) {
    const char *mode = NULL;
    const char *request_path = NULL;
    const char *job_id = NULL;
    const char *jobs_root = NULL;
    bool overwrite = false;
    char diagnostics[256] = {0};
    char generated_job_id[CORE_HEADLESS_JOB_MAX_ID_LENGTH + 1] = {0};
    const char *value = NULL;

    if (argc < 2) {
        usage(argv[0]);
        return 2;
    }
    mode = argv[1];
    for (int i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "--request") == 0 &&
            physics_sim_cli_take_value(argc, argv, &i, false, &value)) {
            request_path = value;
        } else if (strcmp(argv[i], "--job-id") == 0 &&
                   physics_sim_cli_take_value(argc, argv, &i, false, &value)) {
            job_id = value;
        } else if (strcmp(argv[i], "--jobs-root") == 0 &&
                   physics_sim_cli_take_value(argc, argv, &i, false, &value)) {
            jobs_root = value;
        } else if (strcmp(argv[i], "--overwrite") == 0) {
            overwrite = true;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            usage(argv[0]);
            return 0;
        } else {
            usage(argv[0]);
            return 2;
        }
    }

    if (strcmp(mode, "submit") == 0) {
        if (!request_path) {
            usage(argv[0]);
            return 2;
        }
        if (!physics_sim_job_runner_submit(argv[0],
                                           request_path,
                                           jobs_root,
                                           overwrite,
                                           generated_job_id,
                                           sizeof(generated_job_id),
                                           diagnostics,
                                           sizeof(diagnostics))) {
            print_runner_error("submit",
                               "submit_request",
                               diagnostics,
                               request_path,
                               NULL,
                               jobs_root,
                               "check the request JSON, output root, and overwrite policy");
            return 1;
        }
        printf("{\"job_id\":\"%s\",\"status\":\"submitted\"}\n", generated_job_id);
        return 0;
    }
    if (strcmp(mode, "status") == 0) {
        if (!job_id) {
            usage(argv[0]);
            return 2;
        }
        if (!physics_sim_job_runner_print_status(stdout,
                                                 argv[0],
                                                 job_id,
                                                 jobs_root,
                                                 diagnostics,
                                                 sizeof(diagnostics))) {
            print_runner_error("status",
                               "status_lookup",
                               diagnostics,
                               NULL,
                               job_id,
                               jobs_root,
                               "check the job id and job_status.json under the jobs root");
            return 1;
        }
        return 0;
    }
    if (strcmp(mode, "cancel") == 0) {
        if (!job_id) {
            usage(argv[0]);
            return 2;
        }
        if (!physics_sim_job_runner_cancel(argv[0],
                                           job_id,
                                           jobs_root,
                                           diagnostics,
                                           sizeof(diagnostics))) {
            print_runner_error("cancel",
                               "cancel_request",
                               diagnostics,
                               NULL,
                               job_id,
                               jobs_root,
                               "check job status and cancel_requested.flag write access");
            return 1;
        }
        printf("{\"job_id\":\"%s\",\"status\":\"cancelled\"}\n", job_id);
        return 0;
    }

    usage(argv[0]);
    return 2;
}
