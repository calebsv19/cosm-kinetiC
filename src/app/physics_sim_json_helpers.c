#include "app/physics_sim_json_helpers.h"

void physics_sim_json_write_string(FILE *file, const char *value) {
    const unsigned char *cursor = (const unsigned char *)(value ? value : "");
    if (!file) return;
    fputc('"', file);
    while (*cursor) {
        switch (*cursor) {
            case '\\':
                fputs("\\\\", file);
                break;
            case '"':
                fputs("\\\"", file);
                break;
            case '\n':
                fputs("\\n", file);
                break;
            case '\r':
                fputs("\\r", file);
                break;
            case '\t':
                fputs("\\t", file);
                break;
            default:
                if (*cursor < 0x20u) {
                    fprintf(file, "\\u%04x", (unsigned int)*cursor);
                } else {
                    fputc((int)*cursor, file);
                }
                break;
        }
        cursor++;
    }
    fputc('"', file);
}
