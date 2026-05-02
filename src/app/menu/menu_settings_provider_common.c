#include "app/menu/menu_settings_schema.h"

size_t menu_settings_provider_common_fields(const MenuSettingsFieldId **out_fields) {
    static const MenuSettingsFieldId fields[] = {
        MENU_SETTINGS_FIELD_GRID_X,
        MENU_SETTINGS_FIELD_PHYSICS_SUBSTEPS,
        MENU_SETTINGS_FIELD_SOLVER_ITERATIONS,
        MENU_SETTINGS_FIELD_QUALITY_PRESET,
        MENU_SETTINGS_FIELD_SAVE_VOLUME_FRAMES,
        MENU_SETTINGS_FIELD_SAVE_RENDER_FRAMES,
        MENU_SETTINGS_FIELD_HEADLESS_FRAME_COUNT,
        MENU_SETTINGS_FIELD_ENABLE_RENDER_BLUR
    };
    if (out_fields) *out_fields = fields;
    return sizeof(fields) / sizeof(fields[0]);
}
