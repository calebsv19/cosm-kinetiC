#include "hud_renderer.h"
#include "../core/timer_manager.h"
#include "../config/settings_loader.h"
#include "../hud/TextRender/text_render.h"
#include "../core/time_utils.h"

#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>

#define HUD_PADDING 6
#define HUD_SPACING 4
#define HUD_BG_ALPHA 180
#define HUD_TEXT_COLOR (SDL_Color){255, 255, 255, 255}
#define HUD_BG_COLOR (SDL_Color){0, 0, 0, HUD_BG_ALPHA}
#define HUD_UPDATE_INTERVAL_MS 500
#define MAX_HUD_LINES MAX_TIMERS

static uint64_t last_update_time_ns = 0;
static char cached_lines[MAX_HUD_LINES][256];
static int cached_line_count = 0;

void hud_init(void) {
    last_update_time_ns = 0;
    cached_line_count = 0;
    if (!Text_Init()) {
        fprintf(stderr, "[TimerHUD] Failed to initialise text renderer.\n");
    }
}

void hud_shutdown(void) {
    Text_Quit();
}

void ts_render(SDL_Renderer* renderer) {
    if (!ts_settings.hud_enabled) {
        return;
    }
    TimerManager* tm = &g_timer_manager;

    // --- 1. Throttled update ---
    if (has_interval_elapsed(&last_update_time_ns, HUD_UPDATE_INTERVAL_MS)) {
        cached_line_count = 0;
        for (int i = 0; i < tm->count && i < MAX_HUD_LINES; i++) {
            Timer* t = &tm->timers[i];
            snprintf(cached_lines[cached_line_count], sizeof(cached_lines[cached_line_count]),
                     "%s: %.2f ms (min %.2f / max %.2f / σ %.2f)",
                     t->name, t->avg, t->min, t->max, t->stddev);
            cached_line_count++;
        }
    }

    // --- 2. Determine output size from renderer ---
    int screenW = 0;
    int screenH = 0;
    if (SDL_GetRendererOutputSize(renderer, &screenW, &screenH) != 0) {
        screenW = 800;
        screenH = 600;
    }

    int fontHeight = Text_LineHeight();
    if (fontHeight <= 0) fontHeight = 14;

    // --- 3. Compute block layout ---
    int totalHeight = 0;
    int maxWidth = 0;
    for (int i = 0; i < cached_line_count; i++) {
        SDL_Rect bounds = Text_Measure(cached_lines[i]);
        int textW = bounds.w;
        int lineW = textW + HUD_PADDING * 2;
        if (lineW > maxWidth) maxWidth = lineW;
        totalHeight += fontHeight + HUD_PADDING * 2 + HUD_SPACING;
    }
    if (cached_line_count > 0) totalHeight -= HUD_SPACING;

    // --- 4. Determine corner anchor
    const char* pos = ts_settings.hud_position;
    int offsetX = ts_settings.hud_offset_x;
    int offsetY = ts_settings.hud_offset_y;

    int baseX = 0;
    int baseY = 0;
    bool rightAlign = false;

    if (strcmp(pos, "top-left") == 0) {
        baseX = offsetX;
        baseY = offsetY;
    } else if (strcmp(pos, "top-right") == 0) {
        baseX = screenW - offsetX - maxWidth;
        baseY = offsetY;
        rightAlign = true;
    } else if (strcmp(pos, "bottom-left") == 0) {
        baseX = offsetX;
        baseY = screenH - offsetY - totalHeight;
    } else if (strcmp(pos, "bottom-right") == 0) {
        baseX = screenW - offsetX - maxWidth;
        baseY = screenH - offsetY - totalHeight;
        rightAlign = true;
    } else {
        baseX = offsetX;
        baseY = offsetY;
    }

    // --- 5. Draw each line ---
    int y = baseY;
    for (int i = 0; i < cached_line_count; i++) {
        const char* line = cached_lines[i];
        SDL_Rect bounds = Text_Measure(line);
        int textW = bounds.w;
        int bgW = textW + HUD_PADDING * 2;
        int bgH = fontHeight + HUD_PADDING * 2;

        int bgX = baseX;
        int textX = rightAlign ? (bgX + bgW - HUD_PADDING) : (bgX + HUD_PADDING);
        int align = ALIGN_TOP | (rightAlign ? ALIGN_RIGHT : ALIGN_LEFT);

        // Background box
        SDL_Rect bg = { bgX, y, bgW, bgH };
#if !USE_VULKAN
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
#endif
        SDL_SetRenderDrawColor(renderer, HUD_BG_COLOR.r, HUD_BG_COLOR.g, HUD_BG_COLOR.b, HUD_BG_COLOR.a);
        SDL_RenderFillRect(renderer, &bg);

        // Text
        Text_Draw(renderer, line, textX, y + HUD_PADDING, align, HUD_TEXT_COLOR);

        y += bgH + HUD_SPACING;
    }
}
