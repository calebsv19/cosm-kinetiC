#ifndef UI_TEXT_INPUT_H
#define UI_TEXT_INPUT_H

#include <SDL2/SDL.h>
#include <stdbool.h>
#include <stddef.h>

#define TEXT_INPUT_MAX_CHARS 64

typedef struct TextInputField {
    bool active;
    char buffer[TEXT_INPUT_MAX_CHARS];
    size_t length;
    size_t max_length;
    double caret_timer;
    bool caret_visible;
} TextInputField;

void text_input_begin(TextInputField *field,
                      const char *initial_text,
                      size_t max_length);
void text_input_end(TextInputField *field);
void text_input_handle_text(TextInputField *field, const char *text);
void text_input_handle_key(TextInputField *field, SDL_Keycode key);
void text_input_update(TextInputField *field, double dt);
const char *text_input_value(const TextInputField *field);

#endif // UI_TEXT_INPUT_H
