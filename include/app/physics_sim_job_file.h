#ifndef PHYSICS_SIM_JOB_FILE_H
#define PHYSICS_SIM_JOB_FILE_H
#include "app/physics_sim_headless_output.h"
typedef struct PhysicsSimJobFile {
    PhysicsSimHeadlessSidecar sidecar;
    int lock_descriptor;
    bool json;
    bool prepared;
    struct stat prepared_stage;
} PhysicsSimJobFile;
/* Caller owns the returned stream until finish; failure stages remain retained. */
FILE *physics_sim_job_file_begin(const char *path, bool json, PhysicsSimJobFile *file);
/* Prepare flushes/admit stages without changing the destination. Success keeps
 * the stream and metadata lock owned; failure consumes/closes the stream.
 * Publish/discard consume a successful prepared stream and retain failed stages. */
bool physics_sim_job_file_prepare(PhysicsSimJobFile *file, FILE *stream);
/* Recheck a prepared stage without consuming it or changing the destination. */
bool physics_sim_job_file_ready(PhysicsSimJobFile *file, FILE *stream);
bool physics_sim_job_file_publish(PhysicsSimJobFile *file, FILE *stream);
void physics_sim_job_file_discard(PhysicsSimJobFile *file, FILE *stream);
/* Fixed job_status.json + output/report.json pair; consumes both prepared streams.
 * Requires an active job operation guard. Interrupted evidence remains held. */
bool physics_sim_job_file_publish_pair(PhysicsSimJobFile *status, FILE *status_stream,
    PhysicsSimJobFile *report, FILE *report_stream);
bool physics_sim_job_file_finish(PhysicsSimJobFile *file, FILE *stream);
bool physics_sim_job_file_text(const char *path, const char *text);
#endif
