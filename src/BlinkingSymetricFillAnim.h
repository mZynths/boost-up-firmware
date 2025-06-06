#pragma once
#include "AnimatedStrip.h"
#include <FastLED.h>

struct RadiatingSymmetricPulseAnim : public AnimatedStrip::Animation {
    int start_index, end_index;
    CRGB color;
    float center_index;
    float max_radius;
    const float gamma = 1.5;
    bool toInside = false;
    int max_loops = 0;      // 0 means infinite (perpetual)
    int loop_count = 0;

    RadiatingSymmetricPulseAnim(int s, int e, bool toInside, int maxLoops, const CRGB& c, float duration_ms, int fps)
        : start_index(s), end_index(e), color(c), toInside(toInside), max_loops(maxLoops)
    {
        int range = abs(end_index - start_index);
        center_index = (start_index + end_index) / 2.0f;
        max_radius = range / 2.0f;

        total_frames = fps > 0 ? (duration_ms / 1000.0f) * fps : 1;
        current_frame = 0;
        frame_interval_ms = duration_ms / total_frames;
        last_update_ms = millis();

        perpetual = true; // This animation loops indefinitely
    }

    bool update(CRGB* leds) override {
        if (!shouldUpdate()) return true;

        int frames_per_pulse = 30;
        float cycle_progress = (current_frame % frames_per_pulse) / (float)frames_per_pulse;

        bool to_color = ((current_frame / frames_per_pulse) % 2) == 0;
        CRGB target = to_color ? color : CRGB::Black;

        float blur = 3.0f;
        float radius = cycle_progress * (max_radius + blur);

        for (int i = start_index; i <= end_index; ++i) {
            float dist = fabs(i - center_index);
            if (toInside) dist = max_radius - dist;
            float edge_dist = dist - (radius - blur);

            if (edge_dist <= 0) {
                leds[i] = target;
            } else if (edge_dist < blur) {
                float t = 1.0f - (edge_dist / blur);
                t = pow(t, gamma);
                leds[i] = blend(leds[i], target, (uint8_t)(t * 255));
            }
        }

        current_frame++;

        int full_cycle_frames = 2 * frames_per_pulse;

        if (current_frame % full_cycle_frames == 0) {
            loop_count++;

            if (max_loops > 0 && loop_count >= max_loops) {
                perpetual = false; // Stop looping after max_loops
                finished = true;
                return false;
            }
        }

        return true;
    }
};
