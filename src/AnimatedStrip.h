#ifndef ANIMATED_STRIP_H
#define ANIMATED_STRIP_H

#include <FastLED.h>
#include <vector>

class AnimatedStrip {
public:
    struct Animation {
        unsigned long last_update_ms = 0;
        int current_frame = 0;
        int total_frames = 0;
        unsigned long frame_interval_ms = 16;
    
        unsigned long created_at_ms = 0;
        unsigned long start_delay_ms = 0;
        bool started = false;

        bool perpetual = false; // If true, animation will loop indefinitely
        bool finished = false;

        virtual ~Animation() = default;

        virtual bool update(CRGB* leds) = 0;

        virtual bool isFinished() const {
            return finished || (!perpetual && current_frame >= total_frames);
        }

        void finish() {
            finished = true;
        }

    protected:
        bool shouldUpdate() {
            if (isFinished()) return false;

            unsigned long now = millis();

            if (!started) {
                if (created_at_ms == 0) created_at_ms = now;
                
                if (now - created_at_ms >= start_delay_ms) {
                    started = true;
                    last_update_ms = now; // reset timing after delay
                } else {
                    return false;
                }
            }

            if (now - last_update_ms >= frame_interval_ms) {
                last_update_ms = now;
                return true;
            }
            return false;
        }

    };

    AnimatedStrip(CRGB* leds, int num_leds);
    void addAnimation(Animation* anim);
    void update();
    
    void startSymmetricFill(
        int start_index,
        int end_index,
        const CRGB& color,
        float duration_ms = 1000.0f, // Default 1 second
        int fps = 60
    );
    
private:
    CRGB* leds;
    int num_leds;
    std::vector<Animation*> activeAnims;
};


#endif