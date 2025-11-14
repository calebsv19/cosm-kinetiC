#include "command/command_bus.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool command_bus_grow(CommandBus *bus, size_t new_capacity) {
    if (!bus) return false;
    if (new_capacity <= bus->capacity) return true;

    Command *new_buffer = (Command *)calloc(new_capacity, sizeof(Command));
    if (!new_buffer) {
        fprintf(stderr, "[command] Failed to grow command bus to %zu entries\n",
                new_capacity);
        return false;
    }

    for (size_t i = 0; i < bus->count; ++i) {
        size_t src = (bus->head + i) % bus->capacity;
        new_buffer[i] = bus->buffer[src];
    }

    free(bus->buffer);
    bus->buffer  = new_buffer;
    bus->capacity = new_capacity;
    bus->head    = 0;
    bus->tail    = bus->count;
    return true;
}

void command_bus_init(CommandBus *bus, size_t capacity) {
    if (!bus) return;
    if (capacity == 0) capacity = 32;
    bus->buffer = (Command *)calloc(capacity, sizeof(Command));
    bus->capacity = bus->buffer ? capacity : 0;
    bus->head = 0;
    bus->tail = 0;
    bus->count = 0;
    bus->next_id = 1;
}

void command_bus_shutdown(CommandBus *bus) {
    if (!bus) return;
    free(bus->buffer);
    bus->buffer = NULL;
    bus->capacity = 0;
    bus->head = bus->tail = bus->count = 0;
    bus->next_id = 1;
}

bool command_bus_push(CommandBus *bus, const Command *cmd) {
    if (!bus || !cmd || !bus->buffer || bus->count >= bus->capacity) {
        size_t target = bus->capacity ? bus->capacity * 2 : 32;
        if (!command_bus_grow(bus, target)) {
            return false;
        }
    }
    Command entry = *cmd;
    entry.sequence_id = bus->next_id++;
    bus->buffer[bus->tail] = entry;
    bus->tail = (bus->tail + 1) % bus->capacity;
    bus->count++;
    return true;
}

bool command_bus_pop(CommandBus *bus, Command *out) {
    if (!bus || !out || !bus->buffer || bus->count == 0) {
        return false;
    }
    *out = bus->buffer[bus->head];
    bus->head = (bus->head + 1) % bus->capacity;
    bus->count--;
    return true;
}

void command_bus_clear(CommandBus *bus) {
    if (!bus) return;
    bus->head = bus->tail = bus->count = 0;
    bus->next_id = 1;
    if (bus->buffer) {
        memset(bus->buffer, 0, bus->capacity * sizeof(Command));
    }
}

size_t command_bus_count(const CommandBus *bus) {
    return bus ? bus->count : 0;
}

size_t command_bus_dispatch(CommandBus *bus,
                            size_t max_commands,
                            CommandHandler handler,
                            void *user_data) {
    if (!bus || !handler) return 0;
    size_t processed = 0;
    size_t limit = (max_commands == 0) ? (size_t)-1 : max_commands;

    Command cmd;
    while (processed < limit && command_bus_pop(bus, &cmd)) {
        handler(&cmd, user_data);
        ++processed;
    }
    return processed;
}
