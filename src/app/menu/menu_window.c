#include "app/menu/menu_window.h"

#include "app/menu/menu_render.h"
#include "app/menu/shared_theme_font_adapter.h"
#include "font_paths.h"
#include <SDL2/SDL_vulkan.h>
#include <stdio.h>

#include "render/vk_shared_device.h"

bool menu_create_window(SceneMenuInteraction *ctx) {
    if (!ctx) return false;

    physics_sim_shared_theme_load_persisted();

    SDL_Window *window = SDL_CreateWindow(
        "Physics Sim - Scene Editor",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        MENU_WIDTH, MENU_HEIGHT,
        SDL_WINDOW_SHOWN | SDL_WINDOW_VULKAN
    );
    if (!window) {
        fprintf(stderr, "Failed to create menu window: %s\n", SDL_GetError());
        return false;
    }

    ctx->window = window;
    ctx->renderer = (SDL_Renderer *)&ctx->renderer_storage;

    if (ctx->use_shared_device) {
        if (!vk_shared_device_init(window, &ctx->vk_cfg)) {
            fprintf(stderr, "Failed to init shared Vulkan device.\n");
            SDL_DestroyWindow(window);
            ctx->window = NULL;
            return false;
        }

        VkRendererDevice* shared_device = vk_shared_device_get();
        if (!shared_device) {
            fprintf(stderr, "Failed to access shared Vulkan device.\n");
            SDL_DestroyWindow(window);
            ctx->window = NULL;
            return false;
        }

        VkResult vk_init = vk_renderer_init_with_device((VkRenderer *)ctx->renderer, shared_device, window, &ctx->vk_cfg);
        if (vk_init != VK_SUCCESS) {
            fprintf(stderr, "Failed to init menu Vulkan renderer: %d\n", vk_init);
            SDL_DestroyWindow(window);
            ctx->window = NULL;
            return false;
        }
        vk_shared_device_acquire();
    } else {
        VkResult vk_init = vk_renderer_init((VkRenderer *)ctx->renderer, window, &ctx->vk_cfg);
        if (vk_init != VK_SUCCESS) {
            fprintf(stderr, "Failed to init menu Vulkan renderer: %d\n", vk_init);
            SDL_DestroyWindow(window);
            ctx->window = NULL;
            return false;
        }
    }

    vk_renderer_set_logical_size((VkRenderer *)ctx->renderer, (float)MENU_WIDTH, (float)MENU_HEIGHT);

    {
        PhysicsSimMenuThemePalette shared_palette = {0};
        if (physics_sim_shared_theme_resolve_menu_palette(&shared_palette)) {
            MenuThemePalette menu_palette = {
                .background = shared_palette.background_fill,
                .panel = shared_palette.panel_fill,
                .text = shared_palette.text_primary,
                .text_dim = shared_palette.text_muted,
                .accent = shared_palette.accent_primary,
                .button_bg = shared_palette.button_fill,
                .button_bg_active = shared_palette.button_active_fill
            };
            menu_set_theme_palette(&menu_palette);
        }
    }

    {
        char shared_path[256];
        int shared_size = 32;
        if (physics_sim_shared_font_resolve_menu_title(shared_path, sizeof(shared_path), &shared_size)) {
            ctx->font_title = TTF_OpenFont(shared_path, shared_size);
        }
    }
    if (!ctx->font_title) {
        ctx->font_title = TTF_OpenFont(FONT_TITLE_PATH_1, 32);
        if (!ctx->font_title) {
            ctx->font_title = TTF_OpenFont(FONT_TITLE_PATH_2, 32);
        }
    }
    if (!ctx->font_title) {
        fprintf(stderr, "Failed to open title font: %s\n", TTF_GetError());
    }

    {
        char shared_path[256];
        int shared_size = 22;
        if (physics_sim_shared_font_resolve_menu_body(shared_path, sizeof(shared_path), &shared_size)) {
            ctx->font = TTF_OpenFont(shared_path, shared_size);
        }
    }
    if (!ctx->font) {
        ctx->font = TTF_OpenFont(FONT_BODY_PATH_1, 22);
        if (!ctx->font) {
            ctx->font = TTF_OpenFont(FONT_BODY_PATH_2, 22);
        }
    }
    if (!ctx->font) {
        fprintf(stderr, "Failed to open body font: %s\n", TTF_GetError());
    }

    {
        char shared_path[256];
        int shared_size = 18;
        if (physics_sim_shared_font_resolve_menu_small(shared_path, sizeof(shared_path), &shared_size)) {
            ctx->font_small = TTF_OpenFont(shared_path, shared_size);
        }
    }
    if (!ctx->font_small) {
        ctx->font_small = TTF_OpenFont(FONT_BODY_PATH_1, 18);
        if (!ctx->font_small) {
            ctx->font_small = TTF_OpenFont(FONT_BODY_PATH_2, 18);
        }
    }

    return true;
}

void menu_destroy_window(SceneMenuInteraction *ctx) {
    if (!ctx) return;
    physics_sim_shared_theme_save_persisted();
    if (ctx->font_title) {
        TTF_CloseFont(ctx->font_title);
        ctx->font_title = NULL;
    }
    if (ctx->font) {
        TTF_CloseFont(ctx->font);
        ctx->font = NULL;
    }
    if (ctx->font_small) {
        TTF_CloseFont(ctx->font_small);
        ctx->font_small = NULL;
    }

    if (ctx->renderer) {
        vk_renderer_wait_idle((VkRenderer *)ctx->renderer);
        if (ctx->use_shared_device) {
            vk_renderer_shutdown_surface((VkRenderer *)ctx->renderer);
            vk_shared_device_release();
        } else {
            vk_renderer_shutdown((VkRenderer *)ctx->renderer);
        }
        ctx->renderer = NULL;
    }
    if (ctx->window) {
        SDL_DestroyWindow(ctx->window);
        ctx->window = NULL;
    }
}
