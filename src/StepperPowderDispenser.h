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
    * @param step_interval Default delay between steps in microseconds (controls speed)
    * @param pulse_duration Duration of STEP pin high pulse in microseconds
    */
    StepperPowderDispenser(String powder_name, int step_pin, unsigned long step_interval = 1000, unsigned long pulse_duration = 10);

    /**
     * @brief Constructs a StepperPowderDispenser with a default step interval and pulse duration.
     * @param powder_name Name of the powder being dispensed.
     * @param step_pin STEP pin connected to the stepper driver.
     */
    StepperPowderDispenser(String powder_name, int step_pin);

    /**
     * @brief Constructs a StepperPowderDispenser with a default powder name, step interval, and pulse duration.
     * @param step_pin STEP pin connected to the stepper driver.
     */
    StepperPowderDispenser(int step_pin);

    /**
     * @brief Constructs a StepperPowderDispenser with a specific steps per gram, and default step interval and pulse duration.
     * @param powder_name Name of the powder being dispensed.
     * @param step_pin STEP pin connected to the stepper driver.
     * @param steps_per_gram Initial calibration value (steps per gram).
     */
    StepperPowderDispenser(String powder_name, int step_pin, float steps_per_gram);

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
    void setStepsPerGram(float steps_per_gram);

    /**
     * @brief Set the duration of the STEP pin high pulse.
     * @param pulse_duration The new pulse duration in microseconds.
     */
    void setPulseDuration(unsigned long pulse_duration);

    /**
     * @brief Set the delay between steps.
     * @param step_interval The new step interval in microseconds.
     */
    void setStepInterval(unsigned long step_interval);

    /**
     * @brief Prints the relevant tunable variables of the dispenser for debugging purposes.
     */
    void printDebugInfo();

private:
    String s_powderName;
    int s_stepPin;
    float s_stepsPerGram;
    unsigned long s_pulseDuration;
    unsigned long s_stepInterval;
    int s_stepsRemaining;
    bool s_isPulsing;
    unsigned long s_pulseStartTime;
    unsigned long s_stepStartTime;
    bool s_enabled;

};

#endif