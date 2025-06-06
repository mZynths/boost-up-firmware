#include <AnimatedStrip.h>
#include <FastLED.h>
#include <Arduino.h>
#include "AnimatedStrip.h"
#include "SymmetricFillAnim.h"
#include "BlinkingSymetricFillAnim.h"

AnimatedStrip::AnimatedStrip(CRGB* leds, int num_leds)
    : leds(leds), num_leds(num_leds) {}

// struct SymmetricFillAnim : public AnimatedStrip::Animation {
//     int start_index, end_index;
//     CRGB color;
//     float center_index;
//     float max_radius;
//     const float gamma = 1.5;  // Perceptual gamma correction exponent

//     SymmetricFillAnim(int s, int e, const CRGB& c, float duration_ms, int fps)
//         : start_index(s), end_index(e), color(c) 
//     {
//         int range = abs(end_index - start_index);
//         center_index = (start_index + end_index) / 2.0f;
//         max_radius = range / 2.0f;

//         total_frames = fps > 0 ? (duration_ms / 1000.0f) * fps : 1;
//         current_frame = 0;
//         frame_interval_ms = duration_ms / total_frames;
//         last_update_ms = millis();
//     }

//     bool update(CRGB* leds) override {
//         if (!shouldUpdate()) return true;

//         // Apply gamma correction to progress for perceptual linearity
//         float linear_progress = (float)current_frame / total_frames;
//         // float perceptual_progress = pow(linear_progress, gamma);

//         // Calculate expanded radius to ensure full coverage at end
//         float blur = 3.0f;
//         float radius = linear_progress * (max_radius + blur);

//         for (int i = start_index; i <= end_index; ++i) {
//             float dist = fabs(i - center_index);
//             float edge_dist = dist - (radius - blur);
            
//             if (edge_dist <= 0) {
//                 // Core region: full target color
//                 leds[i] = color;
//             } 
//             else if (edge_dist < blur) {
//                 // Transition region: gamma-corrected blend
//                 float t = 1.0f - (edge_dist / blur);
//                 t = pow(t, gamma);  // Apply gamma to blend ratio
//                 leds[i] = blend(leds[i], color, (uint8_t)(t * 255));
//             }
//             // Else: no change (handled implicitly)
//         }

//         // Force full coverage on final frame
//         if (isFinished()) {
//             for (int i = start_index; i <= end_index; ++i) {
//                 leds[i] = color;
//             }
//         }

//         current_frame++;
//         return !isFinished();
//     }
// };

    
void AnimatedStrip::update() {
    bool anyChanges = false;

    for (auto it = activeAnims.begin(); it != activeAnims.end(); ) {
        bool animRunning = (*it)->update(leds);
        if (!animRunning) {
            delete *it;
            it = activeAnims.erase(it);
            anyChanges = true;  // animation ended => LEDs changed on last frame
        } else {
            anyChanges = true;  // animation updated LEDs this frame
            ++it;
        }
    }

    if (anyChanges) {
        FastLED.show();
    }
}

void AnimatedStrip::addAnimation(Animation* anim) {
    activeAnims.push_back(anim);
}

void AnimatedStrip::startSymmetricFill(
    int start_index,
    int end_index,
    const CRGB& color,
    float duration_ms,
    int fps
) {
    // Log what's happening for debugging
    Serial.printf(
        "Starting symmetric fill from %d to %d with color %06X, duration %.2f ms, fps %d\n",
        start_index, end_index, color.as_uint32_t(), duration_ms, fps
    );

    // Create animation and add it to the active list
    SymmetricFillAnim* anim = new SymmetricFillAnim(start_index, end_index, color, duration_ms, fps);
    activeAnims.push_back(anim);
}
