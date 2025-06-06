#pragma once
#include "AnimatedStrip.h"
#include <FastLED.h>

struct SymmetricFillAnim : public AnimatedStrip::Animation {
    int start_index, end_index;
    CRGB color;
    float center_index;
    float max_radius;
    const float gamma = 1.5;  // Perceptual gamma correction exponent

    SymmetricFillAnim(int s, int e, const CRGB& c, float duration_ms, int fps)
        : start_index(s), end_index(e), color(c) 
    {
        int range = abs(end_index - start_index);
        center_index = (start_index + end_index) / 2.0f;
        max_radius = range / 2.0f;

        total_frames = fps > 0 ? (duration_ms / 1000.0f) * fps : 1;
        current_frame = 0;
        frame_interval_ms = duration_ms / total_frames;
        last_update_ms = millis();
    }

    bool update(CRGB* leds) override {
        if (!shouldUpdate()) return true;

        // Apply gamma correction to progress for perceptual linearity
        float linear_progress = (float)current_frame / total_frames;
        // float perceptual_progress = pow(linear_progress, gamma);

        // Calculate expanded radius to ensure full coverage at end
        float blur = 3.0f;
        float radius = linear_progress * (max_radius + blur);

        for (int i = start_index; i <= end_index; ++i) {
            float dist = fabs(i - center_index);
            float edge_dist = dist - (radius - blur);
            
            if (edge_dist <= 0) {
                // Core region: full target color
                leds[i] = color;
            } 
            else if (edge_dist < blur) {
                // Transition region: gamma-corrected blend
                float t = 1.0f - (edge_dist / blur);
                t = pow(t, gamma);  // Apply gamma to blend ratio
                leds[i] = blend(leds[i], color, (uint8_t)(t * 255));
            }
            // Else: no change (handled implicitly)
        }

        // Force full coverage on final frame
        if (isFinished()) {
            for (int i = start_index; i <= end_index; ++i) {
                leds[i] = color;
            }
        }

        current_frame++;
        return !isFinished();
    }
};