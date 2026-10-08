#pragma once

#include <stdbool.h>
#include "ShapeLib/shape_core.h"

bool ShapeDocument_SaveToJsonFile(const ShapeDocument* doc,
                                  const char* path);

bool ShapeDocument_LoadFromJsonFile(const char* path,
                                    ShapeDocument* outDoc);

/* Borrowed NUL-terminated JSON; no filename reopen. Caller supplies empty output.
 * This decoder retains legacy schema behavior; hosts own strict admission. */
bool ShapeDocument_LoadFromJsonText(const char* text, ShapeDocument* outDoc);
