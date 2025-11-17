#pragma once

// Unified cross-platform font resolution system.
// Mac uses system Arial.
// Linux uses Roboto fonts bundled with distro.
// Windows can be added later if needed.

#if defined(__APPLE__)

// --- macOS paths ---
#define FONT_TITLE_PATH_1 "/System/Library/Fonts/Supplemental/Arial Bold.ttf"
#define FONT_TITLE_PATH_2 "/Library/Fonts/Arial Bold.ttf"

#define FONT_BODY_PATH_1  "/System/Library/Fonts/Supplemental/Arial.ttf"
#define FONT_BODY_PATH_2  "/Library/Fonts/Arial.ttf"

#else

// --- Linux paths (OpenSUSE Tumbleweed) ---
// Pick fonts you actually have:
#define FONT_TITLE_PATH_1 "/usr/share/fonts/truetype/RobotoCondensed-Medium.ttf"
#define FONT_TITLE_PATH_2 "/usr/share/fonts/truetype/RobotoCondensed-Medium.ttf"

#define FONT_BODY_PATH_1  "/usr/share/fonts/truetype/Roboto-Light.ttf"
#define FONT_BODY_PATH_2  "/usr/share/fonts/truetype/Roboto-Light.ttf"

#endif
