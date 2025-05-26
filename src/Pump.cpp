#include <Pump.h>

Pump::Pump(
    String fluid_name, 
    int drive_pin, 
    float calibration_K
)
    : m_fluid_name(fluid_name), 
    m_drive_pin(drive_pin), 
    m_calibration_K(calibration_K), 
    m_isEnabled(false)
{
    pinMode(m_drive_pin, OUTPUT);
    digitalWrite(m_drive_pin, LOW);  // Make sure pump is off initially
}

Pump::Pump(String fluid_name, int drive_pin)
    : Pump(fluid_name, drive_pin, 1.0f) // Default calibration of 1 mL/s if none provided
{}

Pump::Pump(int drive_pin)
    : Pump("Some fluid", drive_pin, 1.0f) // Default calibration of 1 mL/s if none provided
{}

void Pump::enable() {
    m_isEnabled = true;
}

void Pump::disable() {
    m_isEnabled = false;
    digitalWrite(m_drive_pin, LOW); // Immediately turn off pump when disabled
}

void Pump::calibrate(int milliseconds_run, float milliliters_dispensed) {
    if (milliseconds_run > 0) {
        m_calibration_K = (milliliters_dispensed * 1000.0f) / milliseconds_run;
        // calibration_K is mL/sec, so convert ms to sec by dividing by 1000
    }
}

void Pump::set_calibration(float mL_per_second) {
    if (mL_per_second > 0) {
        m_calibration_K = mL_per_second;
    }
}

void Pump::dispense(float milliliters) {
    if (!m_isEnabled || milliliters <= 0 || m_calibration_K <= 0) {
        return; // Do nothing if disabled or invalid input
    }

    // Calculate how long to run pump (in ms)
    m_dispenseDurationMs = static_cast<unsigned long>((milliliters / m_calibration_K) * 1000);
    m_dispenseStartTime = millis();

    // Start dispensing
    digitalWrite(m_drive_pin, HIGH);
    m_isDispensing = true;
}

void Pump::spin(int milliseconds) {
    if (!m_isEnabled || milliseconds <= 0 || m_calibration_K <= 0) {
        return; // Do nothing if disabled or invalid input
    }

    // Calculate how long to run pump (in ms)
    m_dispenseDurationMs = milliseconds;
    m_dispenseStartTime = millis();

    // Start dispensing
    digitalWrite(m_drive_pin, HIGH);
    m_isDispensing = true;
}

void Pump::update() {
    if (m_isDispensing) {
        unsigned long now = millis();
        // Handle millis() overflow safely with subtraction
        if ((now - m_dispenseStartTime) >= m_dispenseDurationMs) {
            digitalWrite(m_drive_pin, LOW);
            m_isDispensing = false;
            Serial.println("Dispense complete.");
        }
    }
}

bool Pump::isDispensing() {
    return m_isDispensing;
}

bool Pump::isEnabled() {
    return m_isEnabled;
}

String Pump::getFluidName() {
    return m_fluid_name;
}

void Pump::setFluidName(String fluid_name) {
    m_fluid_name = fluid_name;
}