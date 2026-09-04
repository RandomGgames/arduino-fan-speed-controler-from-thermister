#include <math.h>

const byte PWM_PIN = 9;
const byte TEMP_PIN = A0;

// --- Temperature Control Settings ---
const float TEMP_MIN = 75.0; // 0% fan target threshold, in degrees Fahrenheit
const float TEMP_MAX = 85.0; // 100% fan target threshold, in degrees Fahrenheit

// --- Fan Response Boundaries ---
const byte MIN_FAN_PWM = 50;  // 0% target maps to 50% PWM duty cycle. Based on the fan's minimum speed.
const byte MAX_FAN_PWM = 100; // 100% target maps to 100% PWM duty cycle

// Thermistor Parameters (Standard 100k NTC 3950)
const float THERMISTOR_NOMINAL = 100000.0;
const float TEMPERATURE_NOMINAL = 25.0;
const float B_COEFFICIENT = 3950.0;
const float SERIES_RESISTOR = 100000.0;

// Safety Thresholds (ADC values 0-1023)
const int ADC_FAULT_LOW = 15;    // Near 0 (Short to GND / Over-temp)
const int ADC_FAULT_HIGH = 1008; // Near 1023 (Open circuit / Disconnected)

// 25 kHz PWM timer top limit (16 MHz / (2 * 1 * 25000) = 320)
const uint16_t TIMER1_TOP = 320;

unsigned long lastTempCheck = 0;

void setup25kHzPWM() {
    pinMode(PWM_PIN, OUTPUT);

    // Timer1 setup for Phase-Correct PWM, TOP = ICR1, Prescaler = 1
    // Formula: 16 MHz / (2 * 1 * 320) = 25,000 Hz (25 kHz)
    TCCR1A = _BV(COM1A1) | _BV(WGM11);
    TCCR1B = _BV(WGM13) | _BV(CS10);
    ICR1 = TIMER1_TOP;
}

void setPWM(byte userPercent) {
    if (userPercent > 100)
        userPercent = 100;

    byte actualPWM = map(userPercent, 0, 100, MIN_FAN_PWM, MAX_FAN_PWM);

    // Direct 16-bit register calculation for 25 kHz
    uint16_t compareValue = (uint32_t)TIMER1_TOP * actualPWM / 100;
    OCR1A = compareValue;
}

float calculateTempF(int rawADC) {
    float resistance = SERIES_RESISTOR / ((1023.0 / rawADC) - 1.0);

    float steinhart = resistance / THERMISTOR_NOMINAL;
    steinhart = log(steinhart);
    steinhart /= B_COEFFICIENT;
    steinhart += 1.0 / (TEMPERATURE_NOMINAL + 273.15);
    steinhart = 1.0 / steinhart;

    float tempC = steinhart - 273.15;
    return (tempC * 9.0 / 5.0) + 32.0;
}

void setup() {
    Serial.begin(115200);

    setup25kHzPWM();

    // Start at 100% PWM safety override until valid temperature data is read
    setPWM(100);

    Serial.println("==========================================");
    Serial.println("  Thermistor Fan Controller (25 kHz PWM)");
    Serial.println("==========================================");
}

void loop() {
    if (millis() - lastTempCheck >= 1000) {
        lastTempCheck = millis();

        int rawADC = analogRead(TEMP_PIN);

        // --- Safety Check: Open Circuit or Short Circuit ---
        if (rawADC <= ADC_FAULT_LOW || rawADC >= ADC_FAULT_HIGH) {
            setPWM(100);

            Serial.print("SAFETY FAULT DETECTED! ADC: ");
            Serial.print(rawADC);
            if (rawADC <= ADC_FAULT_LOW)
                Serial.println(" (Short to GND / Over-Temp)");
            else
                Serial.println(" (Sensor Disconnected / Open)");

            return;
        }

        float tempF = calculateTempF(rawADC);

        // --- Safety Check: Out of Bounds Temperature ---
        if (tempF < 30.0 || tempF > 150.0) {
            setPWM(100);

            Serial.print("SAFETY FAULT: Temp out of bounds (");
            Serial.print(tempF, 1);
            Serial.println(" F). Overriding to 100% PWM.");

            return;
        }

        // --- Normal Temperature Operation ---
        float clampedTemp = constrain(tempF, TEMP_MIN, TEMP_MAX);
        byte targetPercent = map(clampedTemp * 10, TEMP_MIN * 10, TEMP_MAX * 10, 0, 100);

        setPWM(targetPercent);

        byte actualPWM = map(targetPercent, 0, 100, MIN_FAN_PWM, MAX_FAN_PWM);

        Serial.print("ADC: ");
        Serial.print(rawADC);
        Serial.print(" | Temp: ");
        Serial.print(tempF, 1);
        Serial.print(" F | Target: ");
        Serial.print(targetPercent);
        Serial.print("% | PWM Output: ");
        Serial.print(actualPWM);
        Serial.println("%");
    }
}