#include "render/vk_shared_device.h"

#include <stdio.h>
#include <string.h>
#include <stdbool.h>

static VkRendererDevice g_device;
static bool g_device_ready = false;
static bool g_device_lost = false;
static int g_device_users = 0;

bool vk_shared_device_init(SDL_Window* window, const VkRendererConfig* config) {
    if (g_device_ready && g_device_lost) {
        if (g_device_users > 0) {
            fprintf(stderr,
                    "[vulkan] shared device lost but still in use (%d users); defer reinit.\n",
                    g_device_users);
            return false;
        }
        vk_renderer_device_shutdown(&g_device);
        memset(&g_device, 0, sizeof(g_device));
        g_device_ready = false;
        g_device_lost = false;
    }
    if (g_device_ready) return true;
    if (!window || !config) return false;

    VkResult result = vk_renderer_device_init(&g_device, window, config);
    if (result != VK_SUCCESS) {
        fprintf(stderr, "[vulkan] shared device init failed: %d\n", result);
        memset(&g_device, 0, sizeof(g_device));
        g_device_ready = false;
        return false;
    }

    g_device_ready = true;
    return true;
}

void vk_shared_device_shutdown(void) {
    if (!g_device_ready) return;
    if (g_device_users > 0) {
        fprintf(stderr,
                "[vulkan] shared device shutdown requested with %d users; skipping.\n",
                g_device_users);
        return;
    }
    vk_renderer_device_shutdown(&g_device);
    memset(&g_device, 0, sizeof(g_device));
    g_device_ready = false;
    g_device_lost = false;
}

void vk_shared_device_wait_idle(void) {
    if (!g_device_ready) return;
    vk_renderer_device_wait_idle(&g_device);
}

VkRendererDevice* vk_shared_device_get(void) {
    if (!g_device_ready) return NULL;
    return &g_device;
}

void vk_shared_device_mark_lost(void) {
    g_device_lost = true;
}

void vk_shared_device_acquire(void) {
    if (g_device_ready) {
        g_device_users++;
    }
}

void vk_shared_device_release(void) {
    if (g_device_users > 0) {
        g_device_users--;
    }
}
