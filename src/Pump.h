#ifndef PERISTALTIC_PUMP_H
#define PERISTALTIC_PUMP_H

#include <Arduino.h>

/**
 * @brief Controls a peristaltic pump for fluid dispensing.
 */
class Pump {

public:
    /**
     * @param fluid_name     The name of the dispensed fluid.
     * @param drive_pin      GPIO pin to control the pump motor.
     * @param calibration_K  Calibration factor in mL per second.
     * @param negated_logic  Whether the pump uses negated logic (LOW = on, HIGH = off).
     */
    Pump(String fluid_name, int drive_pin, float calibration_K, bool negated_logic);
    Pump(String fluid_name, int drive_pin, bool negated_logic);
    Pump(int drive_pin, bool negated_logic);

    /// @brief Lets the pump receive commands.
    void enable();

    /// @brief Prevents the pump from receiving commands.
    void disable();

    /**
     * @brief Sets the calibration factor for you.
     * @param milliseconds_run       Time the pump was run in milliseconds.
     * @param milliliters_dispensed  Volume that was dispensed in mL.
     */
    void calibrate(int milliseconds_run, float milliliters_dispensed);

    /**
     * @brief Sets the calibration factor directly.
     * @param mL_per_second  Calibration factor in mL per second.
     */
    void set_calibration(float mL_per_second);

    /**
     * @brief Dispenses a precise amount of fluid.
     * @param milliliters  Volume in mL to dispense.
     */
    void dispense(float milliliters);

    /**
     * @brief Spins the pump for an amount of time.
     * @param milliseconds  Time to run the pump in milliseconds.
     */
    void spin(int milliseconds);

    /**
     * @brief Call this frequently in your main loop to update pump state.
     */
    void update();

    /**
     * @brief Checks if the pump is currently dispensing.
     * @return true if dispensing, false otherwise.
     */
    bool isDispensing();

    /**
     * @brief Checks if the pump is enabled.
     * @return true if enabled, false otherwise.
     */
    bool isEnabled();

    /**
     * @brief Returns the name of the fluid this pump handles.
     * @return Name of the fluid.
     */
    String getFluidName();
    void setFluidName(String fluid_name);

private:
    void pumpOn();
    void pumpOff();

    String m_fluid_name = "Default Fluid Name";
    int m_drive_pin;
    float m_calibration_K; // in mL per second
    bool m_negated_logic;

    bool m_isEnabled = false;
    bool m_isDispensing = false;
    unsigned long m_dispenseStartTime = 0;
    unsigned long m_dispenseDurationMs = 0;
};

#endif
