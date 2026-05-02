#include "app/menu/menu_settings_schema.h"

size_t menu_settings_provider_structural_fields(const MenuSettingsFieldId **out_fields) {
    static const MenuSettingsFieldId fields[] = {
        MENU_SETTINGS_FIELD_HEADLESS_FRAME_COUNT
    };
    if (out_fields) *out_fields = fields;
    return sizeof(fields) / sizeof(fields[0]);
}
