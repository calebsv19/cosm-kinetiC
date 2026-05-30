#ifndef PRESET_IO_INTERNAL_H
#define PRESET_IO_INTERNAL_H

#include "app/preset_io.h"

FluidSceneDomainType sanitize_domain(FluidSceneDomainType domain);
FluidSceneDimensionMode sanitize_dimension_mode(FluidSceneDimensionMode mode);
void sanitize_emitter(FluidEmitter *em, FluidSceneDimensionMode dimension_mode);
void sanitize_preset_object(PresetObject *obj);
void sanitize_import_shape(ImportedShape *imp);
void boundary_flows_reset(BoundaryFlow flows[BOUNDARY_EDGE_COUNT]);
void boundary_flows_assign(BoundaryFlow dst[BOUNDARY_EDGE_COUNT],
                           const BoundaryFlow src[BOUNDARY_EDGE_COUNT]);
void sanitize_atmosphere(AtmosphericPresetSettings *settings);
float sanitize_dimension_value(float value);
void preset_slot_reset(CustomPresetSlot *slot, int index);
bool preset_library_reserve(CustomPresetLibrary *lib, int desired);

#endif // PRESET_IO_INTERNAL_H
