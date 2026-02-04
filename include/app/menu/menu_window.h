#ifndef MENU_WINDOW_H
#define MENU_WINDOW_H

#include <stdbool.h>

#include "app/menu/menu_types.h"

bool menu_create_window(SceneMenuInteraction *ctx);
void menu_destroy_window(SceneMenuInteraction *ctx);

#endif
