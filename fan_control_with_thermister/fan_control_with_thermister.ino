#include <math.h>

const byte PWM_PIN = 9;
const byte TEMP_PIN = A0;

// --- Configurable Parameters ---
const uint32_t TARGET_FREQ_HZ = 25000; 
const byte MIN_FAN_PWM = 50;          // 0% target maps to 50% PWM
const byte MAX_FAN_PWM = 100;         // 100% target maps to 100% PWM

// --- Temperature Control Settings ---
const float TEMP_MIN_F = 75.0; // 0% fan speed threshold
const float TEMP_MAX_F = 85.0; // 100% fan speed threshold

// Thermistor Parameters (Standard 100k NTC 3950)
const float THERMISTOR_NOMINAL = 100000.0; 
const float TEMPERATURE_NOMINAL = 25.0;     
const float B_COEFFICIENT = 3950.0;         
const float SERIES_RESISTOR = 100000.0;     

// Safety Thresholds (ADC values 0-1023)
const int ADC_FAULT_LOW = 15;     // Near 0 (short to GND / extremely hot)
const int ADC_FAULT_HIGH = 1008;  // Near 1023 (open circuit / disconnected)

uint16_t timer1Top = 320;
unsigned long lastTempCheck = 0;


void configurePWM(uint32_t frequency) {
    pinMode(PWM_PIN, OUTPUT);

    if (frequency == 490) {
        TCCR1A = _BV(COM1A1) | _BV(WGM10);
        TCCR1B = _BV(CS11) | _BV(CS10);
        timer1Top = 255;
        return;
    }

    if (frequency == 25000) {
        TCCR1A = _BV(COM1A1) | _BV(WGM11);
        TCCR1B = _BV(WGM13) | _BV(CS10);
        timer1Top = 320;
        ICR1 = timer1Top;
        return;
    }

    const uint32_t f_cpu = 16000000;
    uint32_t top = f_cpu / (2 * 1 * frequency);
    byte csBits = _BV(CS10);

    if (top > 65535) {
        csBits = _BV(CS11);
        top = f_cpu / (2 * 8 * frequency);
    }
    if (top > 65535) {
        csBits = _BV(CS11) | _BV(CS10);
        top = f_cpu / (2 * 64 * frequency);
    }
    if (top > 65535) {
        csBits = _BV(CS12);
        top = f_cpu / (2 * 256 * frequency);
    }
    if (top > 65535) {
        csBits = _BV(CS12) | _BV(CS10);
        top = f_cpu / (2 * 1024 * frequency);
    }

    if (top > 65535) top = 65535;
    if (top < 1) top = 1;

    timer1Top = (uint16_t)top;

    TCCR1A = _BV(COM1A1) | _BV(WGM11);
    TCCR1B = _BV(WGM13) | csBits;
    ICR1 = timer1Top;
}


void setPWM(byte userPercent) {
    if (userPercent > 100) userPercent = 100;

    byte actualPWM = map(userPercent, 0, 100, MIN_FAN_PWM, MAX_FAN_PWM);

    if (TARGET_FREQ_HZ == 490) {
        byte dutyCycle = map(actualPWM, 0, 100, 0, 255);
        analogWrite(PWM_PIN, dutyCycle);
    } else {
        uint16_t compareValue = (uint32_t)timer1Top * actualPWM / 100;
        OCR1A = compareValue;
    }
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

    configurePWM(TARGET_FREQ_HZ);

    // Fail-safe default: Start at 100% until a valid reading arrives
    setPWM(100);

    Serial.println("==========================================");
    Serial.println("  Fan Controller (With Thermistor Safety)");
    Serial.println("==========================================");
}


void loop() {
    if (millis() - lastTempCheck >= 3000) {
        lastTempCheck = millis();

        int rawADC = analogRead(TEMP_PIN);

        // --- Safety Check: Open Circuit or Short Circuit ---
        if (rawADC <= ADC_FAULT_LOW || rawADC >= ADC_FAULT_HIGH) {
            setPWM(100); // Override to MAX SPEED

            Serial.print("SAFETY FAULT DETECTED! ADC: ");
            Serial.print(rawADC);
            if (rawADC <= ADC_FAULT_LOW) Serial.println(" (Short to GND / Over-Temp)");
            else Serial.println(" (Sensor Disconnected / Open)");

            return;
        }

        float tempF = calculateTempF(rawADC);

        // --- Safety Check: Unreasonable Temperatures ---
        if (tempF < 30.0 || tempF > 150.0) {
            setPWM(100); // Override to MAX SPEED

            Serial.print("SAFETY FAULT: Temperature out of bounds (");
            Serial.print(tempF, 1);
            Serial.println(" F). Overriding to 100% PWM.");

            return;
        }

        // --- Normal Operation ---
        float clampedTemp = constrain(tempF, TEMP_MIN_F, TEMP_MAX_F);
        byte targetPercent = map(clampedTemp * 10, TEMP_MIN_F * 10, TEMP_MAX_F * 10, 0, 100);

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