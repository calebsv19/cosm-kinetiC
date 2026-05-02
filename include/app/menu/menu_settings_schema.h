#ifndef MENU_SETTINGS_SCHEMA_H
#define MENU_SETTINGS_SCHEMA_H

#include "app/menu/menu_settings_types.h"

const MenuSettingsFieldDef *menu_settings_schema_field(MenuSettingsFieldId id);
size_t menu_settings_schema_provider_fields(MenuSettingsProviderId provider,
                                            const MenuSettingsFieldId **out_fields);
MenuSettingsProviderId menu_settings_schema_provider_for_modes(SimulationMode sim_mode,
                                                               SpaceMode space_mode);

#endif
