#ifndef PHYSICS_SIM_JOB_JSON_H
#define PHYSICS_SIM_JOB_JSON_H
#include <json-c/json.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* Trusted-local metadata object, 16 MiB / 64 levels / 100k values; caller owns result. */
/* Same bounded strict object contract over already-admitted bytes; caller owns result. */
json_object *physics_sim_job_json_parse(const char *text, size_t size);
json_object *physics_sim_job_json_read(const char *path);
/* Same ancestor/alias admission for a directory; caller closes returned descriptor. */
int physics_sim_job_json_open_directory(const char *path, char *selected, size_t size);
/* At most eight reads / a cooperative 100 ms deadline, only after identity drift. */
json_object *physics_sim_job_json_observe(const char *path);
typedef enum PhysicsSimJobJsonType { PHYSICS_JOB_INT, PHYSICS_JOB_STRING, PHYSICS_JOB_BOOL } PhysicsSimJobJsonType;
typedef struct PhysicsSimJobJsonField { const char *name; PhysicsSimJobJsonType type; int64_t minimum, maximum; } PhysicsSimJobJsonField;
/* Optional fields may be absent; every present known field must satisfy its contract. */
bool physics_sim_job_json_fields(json_object *root, const PhysicsSimJobJsonField *fields, size_t count);
bool physics_sim_job_json_integer(json_object *root, const char *key, int *value);
#endif
