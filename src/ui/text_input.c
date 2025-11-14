#include "ui/text_input.h"

#include <SDL2/SDL.h>
#include <string.h>

static void clamp_length(TextInputField *field) {
    if (!field) return;
    if (field->length >= field->max_length) {
        field->length = field->max_length;
        field->buffer[field->max_length] = '\0';
    }
}

void text_input_begin(TextInputField *field,
                      const char *initial_text,
                      size_t max_length) {
    if (!field) return;
    memset(field, 0, sizeof(*field));
    field->max_length = (max_length > 0 && max_length < TEXT_INPUT_MAX_CHARS)
                            ? max_length
                            : (TEXT_INPUT_MAX_CHARS - 1);
    if (initial_text) {
        strncpy(field->buffer, initial_text, field->max_length);
        field->buffer[field->max_length] = '\0';
    } else {
        field->buffer[0] = '\0';
    }
    field->length = strlen(field->buffer);
    field->active = true;
    field->caret_visible = true;
    field->caret_timer = 0.0;
    SDL_StartTextInput();
}

void text_input_end(TextInputField *field) {
    if (!field) return;
    field->active = false;
    field->caret_visible = false;
    SDL_StopTextInput();
}

void text_input_handle_text(TextInputField *field, const char *text) {
    if (!field || !field->active || !text) return;
    size_t incoming = strlen(text);
    if (incoming == 0) return;
    if (field->length + incoming >= field->max_length) {
        incoming = field->max_length - field->length;
    }
    if (incoming == 0) return;
    strncat(field->buffer, text, incoming);
    field->length += incoming;
    field->caret_visible = true;
    field->caret_timer = 0.0;
    clamp_length(field);
}

void text_input_handle_key(TextInputField *field, SDL_Keycode key) {
    if (!field || !field->active) return;
    if (key == SDLK_BACKSPACE) {
        if (field->length > 0) {
            field->buffer[field->length - 1] = '\0';
            field->length--;
            field->caret_visible = true;
            field->caret_timer = 0.0;
        }
    }
}

void text_input_update(TextInputField *field, double dt) {
    if (!field || !field->active) return;
    field->caret_timer += dt;
    if (field->caret_timer >= 0.5) {
        field->caret_timer = 0.0;
        field->caret_visible = !field->caret_visible;
    }
}

const char *text_input_value(const TextInputField *field) {
    if (!field) return "";
    return field->buffer;
}
