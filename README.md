# Arduino Nano Open-Loop 4-Wire PWM Fan Controller

An open-loop, temperature-based 4-wire PWM fan controller built for the Arduino Nano (ATmega328P). This project uses a 100k NTC thermistor to smoothly adjust fan speed according to the detected temperature, driving the fan at a hardware-level 25 kHz PWM frequency (in compliance with the Intel 4-Wire PWM Fan Specification).

## Features

- **Intel Spec 25 kHz PWM Output:** Hardware-driven using ATmega328P Timer1 to eliminate motor hum, coil whine, and switching hash.
- **Dynamic Speed Scaling:** Maps a temperature range of **75°F (0% target speed)** to **85°F (100% target speed)**.
- **Custom Duty Cycle Floor:** Maps 0–100% target input to an actual duty cycle range of **50–100% PWM** to ensure reliable motor operation without stalling.
- **Fail-Safe Fault Detection:** Automatically forces the fan to **100% PWM speed** if:
  - The thermistor wire is disconnected or cut (Open Circuit / ADC high).
  - The thermistor wire shorts to ground (Short Circuit / ADC low).
  - Temperature readings fall outside safe operating limits (<30°F or >150°F).
- **Serial Diagnostics:** Outputs real-time ADC levels, calculated temperatures, target speed percentages, and applied duty cycles at 115200 baud.

## Circuit & Wiring

### Hardware Requirements
- Arduino Nano (ATmega328P)
- 4-Wire PWM Computer Fan (12V)
- 100k NTC Thermistor (B-value: 3950)
- 100kΩ Fixed Resistor (1% tolerance recommended)
- 12V Power Supply (for the fan)

### Pinout Connections

| Component | Component Pin | Arduino Nano Pin | Notes |
| :--- | :--- | :--- | :--- |
| **4-Wire Fan** | PWM Control Line | **D9** | Uses arduino's Timer1 pin |
| | Sense / Tach Line | *Unconnected* | |
| | GND | **GND** | Common ground |
| | +12V Power | *External 12V Supply* | Can share VIn pin (Nano supports 7-12V on this pin) |
| **Thermistor Circuit** | Fixed Resistor (Side 1) | **5V** | Voltage divider source |
| | Fixed Resistor (Side 2) / Thermistor (Leg 1) | **A0** | Analog sensing node |
| | Thermistor (Leg 2) | **GND** | Common ground |

## Installation & Setup

1. **Clone or download** this repository.
2. Open `fan_controller.ino` in the **Arduino IDE**.
3. Select **Arduino Nano** as your board (`Tools > Board > Arduino AVR Boards > Arduino Nano`).
4. Select the appropriate **Processor** (ATmega328P or ATmega328P Old Bootloader depending on your board).
5. Upload the sketch.
6. Open the **Serial Monitor** set to **115200 baud** to monitor temperature and duty cycle output.

## Configuration

You can easily adjust operating parameters near the top of the sketch:

```cpp
// --- Temperature Control Settings ---
const float TEMP_MIN = 75.0; // 0% fan target threshold, in degrees Fahrenheit
const float TEMP_MAX = 85.0; // 100% fan target threshold, in degrees Fahrenheit

// --- Fan Response Boundaries ---
const byte MIN_FAN_PWM = 50;  // 0% target maps to 50% PWM duty cycle. Based on the fan's minimum speed.
const byte MAX_FAN_PWM = 100; // 100% target maps to 100% PWM duty cycle
```

## How It Works

### 1. 25 kHz PWM Generation
Standard `analogWrite()` on the Arduino Nano operates at ~490 Hz, which produces loud, audible motor buzz in some PC fans. This controller configures Timer1 into **Phase-Correct PWM Mode** using `ICR1 = 320` as the TOP counter value:

Frequency = 16 MHz / (2 * 1 * 320) = 25 kHz

This generates a completely silent 25 kHz switching frequency directly on Pin D9.

### 2. Steinhart-Hart Temperature Mapping
The analog voltage on pin `A0` is converted to resistance and calculated using the Steinhart-Hart B-parameter equation:

$$
T_\text{K} = \frac{\beta \cdot T_0}{\beta + T_0 \cdot \ln\left(\frac{R}{R_0}\right)}
$$

> **Note:** Temperatures ($T_\text{K}$ and $T_0$) must be in Kelvin ($T_\text{K} = T_\text{C} + 273.15$). The result is converted to Celsius ($T_\text{C} = T_\text{K} - 273.15$) and then Fahrenheit ($T_\text{F} = T_\text{C} \times \frac{9}{5} + 32$) for output.

The resulting temperature in Fahrenheit is constrained between `TEMP_MIN_F` and `TEMP_MAX_F` and scaled linearly to the target duty cycle range.

## Authors & Credits

- **Hardware & Testing:** Designed, configured, and tested by RandomGgames.
- **Software Implementation:** Developed in collaboration with Gemini.
