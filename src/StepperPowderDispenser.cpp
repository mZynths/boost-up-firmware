#include <StepperPowderDispenser.h>

StepperPowderDispenser::StepperPowderDispenser(
    String powder_name, 
    int step_pin, 
    unsigned long step_interval, 
    unsigned long pulse_duration
)
    : s_powderName(powder_name), 
    s_stepPin(step_pin), 
    s_stepsPerGram(1.0f),
    s_pulseDuration(pulse_duration), 
    s_stepInterval(step_interval),
    s_stepsRemaining(0), 
    s_isPulsing(false), 
    s_pulseStartTime(0),
    s_stepStartTime(0), 
    s_enabled(false) 
{
    pinMode(s_stepPin, OUTPUT);
    digitalWrite(s_stepPin, LOW);
}

// Existing constructors:
StepperPowderDispenser::StepperPowderDispenser(
    String powder_name, 
    int step_pin
)
    : StepperPowderDispenser(powder_name, step_pin, 1000, 10) // Default step_interval = 1000us, pulse_duration = 10us
{}

StepperPowderDispenser::StepperPowderDispenser(int step_pin)
    : StepperPowderDispenser("Some powder", step_pin, 1000, 10) // Default powder name "Some Powder"
{}

// New constructor that also sets steps per gram:
StepperPowderDispenser::StepperPowderDispenser(String powder_name, int step_pin, float steps_per_gram)
    : StepperPowderDispenser(powder_name, step_pin, 1000, 10) // Default step_interval = 1000us, pulse_duration = 10us
{
    s_stepsPerGram = steps_per_gram; // Set s_stepsPerGram after delegation
}


void StepperPowderDispenser::enable() {
    s_enabled = true;
}

void StepperPowderDispenser::disable() {
    s_enabled = false;
    s_stepsRemaining = 0;
    digitalWrite(s_stepPin, LOW);
    s_isPulsing = false;
}

void StepperPowderDispenser::calibrate(int steps, float grams_dispensed) {
    if (grams_dispensed > 0) {
        s_stepsPerGram = static_cast<float>(steps) / grams_dispensed;
    }
}

void StepperPowderDispenser::dispense(float grams) {
    if (!s_enabled || grams <= 0 || s_stepsPerGram <= 0) return;
    
    s_stepsRemaining = static_cast<int>(grams * s_stepsPerGram);
    s_stepStartTime = micros();
}

void StepperPowderDispenser::spin(int steps) {
    if (!s_enabled || steps <= 0) return;

    s_stepsRemaining = steps;
    s_stepStartTime = micros();
}

void StepperPowderDispenser::update() {
    if (!s_enabled || s_stepsRemaining <= 0) return;

    unsigned long currentTime = micros();

    if (!s_isPulsing) {
        // Check if enough time passed since last step start
        if ((currentTime - s_stepStartTime) >= s_stepInterval) {
            digitalWrite(s_stepPin, HIGH);
            s_pulseStartTime = currentTime;
            s_isPulsing = true;
            s_stepStartTime = currentTime; // Reset timing for next step
        }
    } else {
        // Check if pulse duration completed
        if ((currentTime - s_pulseStartTime) >= s_pulseDuration) {
            digitalWrite(s_stepPin, LOW);
            s_isPulsing = false;
            s_stepsRemaining--;
        }
    }
}

bool StepperPowderDispenser::isDispensing() {
    return s_stepsRemaining > 0;
}

String StepperPowderDispenser::getPowderName() {
    return s_powderName;
}

void StepperPowderDispenser::setPowderName(String name) {
    s_powderName = name;
}

void StepperPowderDispenser::setStepsPerGram(float steps_per_gram) {
    s_stepsPerGram = steps_per_gram;
}

void StepperPowderDispenser::setPulseDuration(unsigned long pulse_duration) {
    s_pulseDuration = pulse_duration;
}

void StepperPowderDispenser::setStepInterval(unsigned long step_interval) {
    s_stepInterval = step_interval;
}

void StepperPowderDispenser::printDebugInfo() {
    Serial.println(F("--- Stepper Powder Dispenser Debug Info ---"));
    Serial.print(F("Powder Name: "));
    Serial.println(s_powderName);
    Serial.print(F("Step Pin: "));
    Serial.println(s_stepPin);
    Serial.print(F("Steps Per Gram: "));
    Serial.println(s_stepsPerGram, 4); // Print with 4 decimal places for precision
    Serial.print(F("Pulse Duration (us): "));
    Serial.println(s_pulseDuration);
    Serial.print(F("Step Interval (us): "));
    Serial.println(s_stepInterval);
    Serial.print(F("Steps Remaining: "));
    Serial.println(s_stepsRemaining);
    Serial.print(F("Is Pulsing: "));
    Serial.println(s_isPulsing ? "True" : "False");
    Serial.print(F("Enabled: "));
    Serial.println(s_enabled ? "True" : "False");
    Serial.println(F("-----------------------------------------"));
}