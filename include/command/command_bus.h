#ifndef COMMAND_BUS_H
#define COMMAND_BUS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum CommandType {
    COMMAND_NONE = 0,
    COMMAND_TOGGLE_PAUSE,
    COMMAND_CLEAR_SMOKE,
    COMMAND_EXPORT_SNAPSHOT,
    COMMAND_TOGGLE_VORTICITY,
    COMMAND_TOGGLE_PRESSURE,
    COMMAND_TOGGLE_VELOCITY_VECTORS,
    COMMAND_TOGGLE_VELOCITY_MODE,
    COMMAND_TOGGLE_PARTICLE_FLOW,
    COMMAND_TOGGLE_KIT_VIZ_DENSITY,
    COMMAND_TOGGLE_KIT_VIZ_VELOCITY,
    COMMAND_TOGGLE_KIT_VIZ_PRESSURE,
    COMMAND_TOGGLE_KIT_VIZ_VORTICITY,
    COMMAND_TOGGLE_KIT_VIZ_PARTICLES,
    COMMAND_TOGGLE_OBJECT_GRAVITY,
    COMMAND_TOGGLE_ELASTIC_COLLISIONS
} CommandType;

typedef struct CommandPayload {
    int   ints[4];
    float floats[4];
} CommandPayload;

typedef struct Command {
    CommandType type;
    uint64_t    sequence_id;
    CommandPayload payload;
} Command;

typedef struct CommandBus {
    Command *buffer;
    size_t   capacity;
    size_t   head;
    size_t   tail;
    size_t   count;
    uint64_t next_id;
} CommandBus;

typedef bool (*CommandHandler)(const Command *cmd, void *user_data);

void command_bus_init(CommandBus *bus, size_t capacity);
void command_bus_shutdown(CommandBus *bus);
bool command_bus_push(CommandBus *bus, const Command *cmd);
bool command_bus_pop(CommandBus *bus, Command *out);
void command_bus_clear(CommandBus *bus);
size_t command_bus_count(const CommandBus *bus);
size_t command_bus_dispatch(CommandBus *bus,
                            size_t max_commands,
                            CommandHandler handler,
                            void *user_data);

#endif // COMMAND_BUS_H
