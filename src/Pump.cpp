#include <Pump.h>

Pump::Pump(String fluid_name, int drive_pin, float calibration_K, bool negated_logic)
    : m_fluid_name(fluid_name),
      m_drive_pin(drive_pin),
      m_calibration_K(calibration_K),
      m_negated_logic(negated_logic),
      m_isEnabled(false)
{
    pinMode(m_drive_pin, OUTPUT);
    pumpOff(); // Make sure pump is off initially
}

Pump::Pump(String fluid_name, int drive_pin, bool negated_logic)
    : Pump(fluid_name, drive_pin, 1.0f, negated_logic) // Default calibration
{}

Pump::Pump(int drive_pin, bool negated_logic)
    : Pump("Some fluid", drive_pin, 1.0f, negated_logic) // Default fluid name and calibration
{}

void Pump::enable() {
    m_isEnabled = true;
}

void Pump::disable() {
    m_isEnabled = false;
    pumpOff(); // Immediately turn off pump when disabled
}

void Pump::calibrate(int milliseconds_run, float milliliters_dispensed) {
    if (milliseconds_run > 0) {
        m_calibration_K = (milliliters_dispensed * 1000.0f) / milliseconds_run;
    }
}

void Pump::set_calibration(float mL_per_second) {
    if (mL_per_second > 0) {
        m_calibration_K = mL_per_second;
    }
}

void Pump::dispense(float milliliters) {
    if (!m_isEnabled || milliliters <= 0 || m_calibration_K <= 0) {
        return;
    }

    m_dispenseDurationMs = static_cast<unsigned long>((milliliters / m_calibration_K) * 1000);
    m_dispenseStartTime = millis();

    pumpOn();
    m_isDispensing = true;
}

void Pump::spin(int milliseconds) {
    if (!m_isEnabled || milliseconds <= 0 || m_calibration_K <= 0) {
        return;
    }

    m_dispenseDurationMs = milliseconds;
    m_dispenseStartTime = millis();

    pumpOn();
    m_isDispensing = true;
}

void Pump::update() {
    if (m_isDispensing) {
        unsigned long now = millis();
        if ((now - m_dispenseStartTime) >= m_dispenseDurationMs) {
            pumpOff();
            m_isDispensing = false;
            Serial.println("Dispense complete.");
            disable();
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

// -------------------- Private Helper Methods --------------------

void Pump::pumpOn() {
    digitalWrite(m_drive_pin, m_negated_logic ? LOW : HIGH);
}

void Pump::pumpOff() {
    digitalWrite(m_drive_pin, m_negated_logic ? HIGH : LOW);
}