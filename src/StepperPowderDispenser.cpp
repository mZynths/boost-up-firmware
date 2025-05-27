#include <StepperPowderDispenser.h>
#include <Arduino.h>

StepperPowderDispenser::StepperPowderDispenser(
    String powder_name,
    int step_pin,
    int sleep_pin,
    int dir_pin,
    bool dispense_is_CW,
    float steps_per_gram,
    int step_interval,
    int pulse_duration,
    int steps_per_revolution,
    int vibration_step_interval,
    int vibration_pulse_duration,
    int steps_per_vibration
):
    s_powder_name(powder_name),
    s_step_pin(step_pin),
    s_dir_pin(dir_pin),
    s_sleep_pin(sleep_pin),
    s_dispense_is_CW(dispense_is_CW),
    s_steps_per_gram(steps_per_gram),
    s_step_interval(step_interval),
    s_pulse_duration(pulse_duration),
    s_steps_per_revolution(steps_per_revolution),
    s_vibration_step_interval(vibration_step_interval),
    s_vibration_pulse_duration(vibration_pulse_duration),
    s_steps_per_vibration(steps_per_vibration),
    s_steps_till_vibration(steps_per_vibration),
    s_steps_remaining(0),
    s_stepStartTime(0),
    s_pulseStartTime(0),
    s_isPulsing(false),
    s_isEnabled(false)
{
    pinMode(s_step_pin, OUTPUT);
    digitalWrite(s_step_pin, LOW); // Make sure step pin is LOW initially
    pinMode(s_dir_pin, OUTPUT);
    digitalWrite(s_dir_pin, s_dispense_is_CW ? HIGH : LOW); // Make sure step pin is LOW initially
    pinMode(s_sleep_pin, OUTPUT);
    digitalWrite(s_sleep_pin, LOW); // Make sure step pin is LOW initially;
}

void StepperPowderDispenser::enable() {
    digitalWrite(s_dir_pin, s_dispense_is_CW ? HIGH : LOW); // Set direction
    digitalWrite(s_sleep_pin, HIGH); // Wake up the stepper driver
    delay(5); // Wait for the driver to wake up

    s_isEnabled = true;
}

void StepperPowderDispenser::disable() {
    s_isEnabled = false;
    s_steps_remaining = 0;
    digitalWrite(s_step_pin, LOW);
    digitalWrite(s_sleep_pin, LOW); // Put the stepper driver to sleep
    s_isPulsing = false;
}

void StepperPowderDispenser::calibrate(int steps, float grams_dispensed) {
    if (grams_dispensed > 0) {
        s_steps_per_gram = static_cast<float>(steps) / grams_dispensed;
    }
}

void StepperPowderDispenser::dispense(float grams) {
    if (!s_isEnabled || grams <= 0 || s_steps_per_gram <= 0) return;

    digitalWrite(s_dir_pin, s_dispense_is_CW ? HIGH : LOW); // Set direction
    
    // s_steps_till_vibration = s_steps_per_vibration;
    
    s_steps_remaining = static_cast<int>(grams * s_steps_per_gram);
    s_stepStartTime = micros();
}

void StepperPowderDispenser::spin(int steps) {
    if (!s_isEnabled || steps <= 0) return;

    Serial.printf("Spinning %d steps\n", steps);
    digitalWrite(s_dir_pin, s_dispense_is_CW ? HIGH : LOW); // Set direction

    s_steps_till_vibration = s_steps_per_vibration;

    s_steps_remaining = steps;
    s_stepStartTime = micros();
}

void StepperPowderDispenser::vibrate() {
    if (!s_isEnabled) return;

    for (int x = 0; x < 60; x++) {
        digitalWrite(s_dir_pin, LOW); // Set direction to LOW
        delay(1); // Wait for DIR pin to stabilize

        // Spin the stepper motor for 10 steps
        for (int i = 0; i < 3; i++) {
            digitalWrite(s_step_pin, HIGH);
            delayMicroseconds(1000);
            digitalWrite(s_step_pin, LOW);
            delayMicroseconds(1000);
        }

        // Wait for the motor to stop
        delay(4);

        digitalWrite(s_dir_pin, HIGH); // Set direction to HIGH
        delay(2); // Wait for DIR pin to stabilize
        
        // Spin the stepper motor for 10 steps
        for (int i = 0; i < 3; i++) {
            digitalWrite(s_step_pin, HIGH);
            delayMicroseconds(1000);
            digitalWrite(s_step_pin, LOW);
            delayMicroseconds(1000);
        }

        delay(5);
    }
}

void StepperPowderDispenser::update() {
    if (!s_isEnabled || s_steps_remaining <= 0) return;

    unsigned long currentTime = micros();

    if (!s_isPulsing) {
        // Check if enough time passed since last step start
        if ((currentTime - s_stepStartTime) >= s_step_interval) {
            digitalWrite(s_step_pin, HIGH);
            s_pulseStartTime = currentTime;
            s_isPulsing = true;
            s_stepStartTime = currentTime; // Reset timing for next step
        }
    } else {
        // Check if pulse duration completed
        if ((currentTime - s_pulseStartTime) >= s_pulse_duration) {
            digitalWrite(s_step_pin, LOW);
            s_isPulsing = false;
            s_steps_remaining--;
            // s_steps_till_vibration--;
            // if (s_steps_till_vibration <= 0) {
            //     vibrate();
            //     s_steps_till_vibration = s_steps_per_vibration;
            //     digitalWrite(s_dir_pin, s_dispense_is_CW ? HIGH : LOW); // Set direction
            //     delay(1000); // Wait for the powder to settle
            // }
        }
    }
}

bool StepperPowderDispenser::isDispensing() {
    return s_steps_remaining > 0;
}

String StepperPowderDispenser::getPowderName() {
    return s_powder_name;
}

void StepperPowderDispenser::setPowderName(String name) {
    s_powder_name = name;
}

void StepperPowderDispenser::setStepsPerGram(int steps_per_gram) {
    s_steps_per_gram = steps_per_gram;
}

void StepperPowderDispenser::printDebugInfo() {
    Serial.println(F("--- Stepper Powder Dispenser Debug Info ---"));
    Serial.print(F("Powder Name: "));
    Serial.println(s_powder_name);
    Serial.print(F("Steps Per Gram: "));
    Serial.println(s_steps_per_gram, 4); // Print with 4 decimal places for precision
    Serial.print(F("Pulse Duration (us): "));
    Serial.println(s_pulse_duration);
    Serial.print(F("Step Interval (us): "));
    Serial.println(s_step_interval);
    Serial.print(F("Steps Remaining: "));
    Serial.println(s_steps_remaining);
    Serial.print(F("Is Pulsing: "));
    Serial.println(s_isPulsing ? "True" : "False");
    Serial.print(F("Enabled: "));
    Serial.println(s_isEnabled ? "True" : "False");
    Serial.println(F("-----------------------------------------"));
}