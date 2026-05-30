#include "app/scene_controller_internal.h"

#include <math.h>

void stroke_sampler_init(StrokeSampler *sampler,
                         size_t capacity,
                         const AppConfig *cfg) {
    if (!sampler) return;
    stroke_buffer_init(&sampler->buffer, capacity);
    sampler->brush_mode = BRUSH_MODE_DENSITY;
    sampler->pointer_down = false;
    sampler->last_emit_x = sampler->last_emit_y = 0.0f;
    sampler->current_x = sampler->current_y = 0.0f;
    sampler->accumulator = 0.0;
    {
        double rate = (cfg && cfg->stroke_sample_rate > 0.0)
                          ? cfg->stroke_sample_rate
                          : SCENE_CONTROLLER_DEFAULT_SAMPLE_RATE;
        sampler->sample_interval = 1.0 / rate;
    }
    sampler->sample_spacing = (cfg && cfg->stroke_spacing > 0.0f)
                                  ? cfg->stroke_spacing
                                  : SCENE_CONTROLLER_DEFAULT_SAMPLE_SPACING;
}

void stroke_sampler_shutdown(StrokeSampler *sampler) {
    if (!sampler) return;
    stroke_buffer_shutdown(&sampler->buffer);
}

void stroke_sampler_capture(StrokeSampler *sampler,
                            const InputCommands *cmds,
                            double dt) {
    if (!sampler || !cmds) return;
    sampler->accumulator += dt;

    if (cmds->brush_mode_changed) {
        sampler->brush_mode = cmds->brush_mode;
    }

    sampler->current_x = (float)cmds->mouse_x;
    sampler->current_y = (float)cmds->mouse_y;

    if (!cmds->mouse_down) {
        sampler->pointer_down = false;
        sampler->accumulator = fmin(sampler->accumulator, sampler->sample_interval);
        return;
    }

    if (!sampler->pointer_down) {
        StrokeSample sample = {0};
        sampler->pointer_down = true;
        sampler->last_emit_x = sampler->current_x;
        sampler->last_emit_y = sampler->current_y;
        sample.x = (int)lroundf(sampler->current_x);
        sample.y = (int)lroundf(sampler->current_y);
        sample.vx = 0.0f;
        sample.vy = 0.0f;
        sample.mode = sampler->brush_mode;
        stroke_buffer_push(&sampler->buffer, &sample);
    }

    while (sampler->accumulator >= sampler->sample_interval) {
        float dx;
        float dy;
        float dist;
        float spacing;
        int steps;
        float step_x;
        float step_y;
        sampler->accumulator -= sampler->sample_interval;

        dx = sampler->current_x - sampler->last_emit_x;
        dy = sampler->current_y - sampler->last_emit_y;
        dist = sqrtf(dx * dx + dy * dy);
        spacing = (sampler->sample_spacing > 0.0f)
                      ? sampler->sample_spacing
                      : SCENE_CONTROLLER_DEFAULT_SAMPLE_SPACING;
        steps = (spacing > 0.0f) ? (int)ceilf(dist / spacing) : 1;
        if (steps < 1) steps = 1;
        step_x = (steps > 0) ? dx / (float)steps : 0.0f;
        step_y = (steps > 0) ? dy / (float)steps : 0.0f;

        for (int i = 0; i < steps; ++i) {
            StrokeSample sample = {0};
            sampler->last_emit_x += step_x;
            sampler->last_emit_y += step_y;
            sample.x = (int)lroundf(sampler->last_emit_x);
            sample.y = (int)lroundf(sampler->last_emit_y);
            sample.vx = step_x;
            sample.vy = step_y;
            sample.mode = sampler->brush_mode;
            stroke_buffer_push(&sampler->buffer, &sample);
        }
    }
}

void stroke_sampler_apply(StrokeSampler *sampler,
                          SceneState *scene,
                          size_t max_samples) {
    if (!sampler || !scene) return;
    {
        size_t processed = 0;
        StrokeSample sample;
        while (processed < max_samples &&
               stroke_buffer_pop(&sampler->buffer, &sample)) {
            scene_apply_brush_sample(scene, &sample);
            ++processed;
        }
    }
}
