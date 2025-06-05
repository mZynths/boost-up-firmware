#ifndef STEPPER_POWDER_DISPENSER_H
#define STEPPER_POWDER_DISPENSER_H

#include <Arduino.h>

/**
 * @brief Controls an stepper motor for powder dispensing.
 */
class StepperPowderDispenser {

public:
    /**
     * @param powder_name Name of the powder being dispensed
     * @param step_pin STEP pin connected to the stepper driver
     * @param dir_pin STEP pin connected to the stepper driver
     * @param sleep_pin SLEEP pin connected to the stepper driver
     * @param steps_per_gram Steps needed to dispense 1 gram of powder
     * @param step_interval Microseconds between steps
     * @param pulse_duration Duration of pulse in microseconds
     * @param steps_per_revolution Steps per revolution of the motor
     * @param vibration_step_interval Microseconds between steps in vibration motion
     * @param vibration_pulse_duration Duration of pulse in microseconds in vibration motion
     * @param steps_per_vibration How many steps to take before vibrating in dispense motion
     * @param dispense_is_CW Direction of the dispense motion (true for clockwise, false for counter-clockwise)
     */
    StepperPowderDispenser(
        String powder_name,
        int step_pin,
        int sleep_pin,
        float steps_per_gram,
        int step_interval,
        int pulse_duration,
        int steps_per_revolution
    );

    StepperPowderDispenser(
        int step_pin,
        int sleep_pin,
        float steps_per_gram,
        int steps_per_revolution
    );

    StepperPowderDispenser(
        int step_pin,
        int sleep_pin
    );
   
    /// @brief Allow motor to receive commands
    void enable();

    /// @brief Stop motor and prevent further commands
    void disable();

    /**
     * @brief Set calibration ratio (steps per gram)
     * @param steps             Steps taken during calibration
     * @param grams_dispensed   Actual grams dispensed
     */
    void calibrate(int steps, float grams_dispensed);

    /**
     * @brief Dispense precise amount of powder using calibration
     * @param grams  Grams to dispense
     */
    void dispense(float grams);

    /**
     * @brief Rotate motor for uncalibrated motion
     * @param steps  Number of steps to spin
     */
    void spin(int steps);
    
    /// @brief Non-blocking update to handle motor movement
    void update();

    /// @brief Check if currently dispensing
    bool isDispensing();

    /// @brief Get powder name
    String getPowderName();

    /// @brief Set powder name
    void setPowderName(String name);

    /**
     * @brief Set the steps per gram for calibration.
     * @param steps_per_gram The new steps per gram value.
     */
    void setStepsPerGram(int steps_per_gram);

    /**
     * @brief Prints the relevant tunable variables of the dispenser for debugging purposes.
     */
    void printDebugInfo();

private:
    // Hardware variables
    int s_step_pin;
    int s_sleep_pin;

    // Stepper motor timing
    int s_step_interval;                // microseconds between steps
    int s_pulse_duration;               // duration of pulse in microseconds
    int s_steps_per_revolution;         // steps per revolution of the motor

    // Dispenser variables
    String s_powder_name;
    bool s_dispense_is_CW;               // steps per gram of powder dispensed
    float s_steps_per_gram;               // steps per gram of powder dispensed
    
    // State variables
    int s_steps_remaining;              // steps remaining in dispense motion
    unsigned long s_pulseStartTime;
    unsigned long s_stepStartTime;
    bool s_isPulsing = false;         
    bool s_isEnabled = false;
};

#endif