<div align="center">
  <img src="./assets/digital-thermometer-banner.svg" width="100%" alt="Digital Thermometer - 8051, LM35, ADC0808 and 16x2 LCD"/>
</div>

<p align="center">
  <img src="https://img.shields.io/badge/MCU-8051-0b1220?style=flat-square&labelColor=0b1220&color=0ea5e9" alt="8051 MCU"/>
  <img src="https://img.shields.io/badge/SENSOR-LM35-0b1220?style=flat-square&labelColor=0b1220&color=14b8a6" alt="LM35 sensor"/>
  <img src="https://img.shields.io/badge/ADC-ADC0808-0b1220?style=flat-square&labelColor=0b1220&color=38bdf8" alt="ADC0808"/>
  <img src="https://img.shields.io/badge/LANGUAGE-Embedded_C-0b1220?style=flat-square&labelColor=0b1220&color=22d3ee" alt="Embedded C"/>
  <img src="https://img.shields.io/badge/DISPLAY-16x2_LCD-0b1220?style=flat-square&labelColor=0b1220&color=2dd4bf" alt="16x2 LCD"/>
  <img src="https://img.shields.io/badge/SMART_FILTER-16_SAMPLES-0b1220?style=flat-square&labelColor=0b1220&color=06b6d4" alt="16-sample smart filter"/>
</p>

## Overview

This project implements a digital thermometer around an **8051-compatible microcontroller**. An **LM35** converts temperature into an analog voltage, an **ADC0808** digitizes that signal, and the microcontroller writes the measured value to a **16x2 character LCD**.

The firmware is written for the Keil C51-style 8051 environment and demonstrates ADC handshaking, timer-generated ADC clocking, 8-bit LCD control, continuous sensor acquisition, and a compact smart-measurement pipeline designed for a resource-constrained MCU.

> **Educational prototype:** this project is not a calibrated medical thermometer and must not be used for clinical decisions.

## System Architecture

```mermaid
flowchart LR
    A[Ambient temperature] --> B[LM35 sensor]
    B -->|Analog voltage| C[ADC0808]
    C -->|8-bit data on Port 1| D[8051 MCU]
    D -->|Data on Port 3| E[16x2 LCD]
    D -->|Timer 0 clock and control| C
```

## How It Works

1. The LM35 produces a voltage proportional to temperature, nominally **10 mV per °C**.
2. ADC0808 channel 0 is selected by driving `CHA`, `CHB`, and `CHC` low.
3. The 8051 generates the ADC clock by toggling `P2.2` from the Timer 0 interrupt.
4. `ALE` and `START` begin a conversion, while `EOC` indicates when conversion is complete.
5. `OE` enables the ADC output, and the 8-bit result is read through Port 1.
6. Sixteen readings form one measurement window. The minimum and maximum are rejected, and the remaining 14 readings are averaged.
7. The firmware converts the filtered ADC result to Celsius, grades signal quality, detects temperature direction, and updates the LCD.

## Smart Measurement Layer

The original project displayed one raw ADC reading. This version adds a distinctive telemetry layer while staying lightweight enough for an 8051:

| Capability | Behaviour |
| --- | --- |
| **Spike rejection** | Removes one minimum and one maximum value from every 16-sample window |
| **Noise reduction** | Averages the remaining 14 readings without storing or sorting an array |
| **Measurement quality** | Reports `HIGH`, `MED`, or `LOW` from the sample spread |
| **Live trend** | Reports `RISING`, `FALLING`, or `STABLE` compared with the previous valid window |
| **Sensor protection** | Replaces the reading with `SENSOR FAULT` when the calculated value exceeds the LM35 operating range |
| **Integer-only conversion** | Avoids floating-point overhead in the embedded firmware |

Example LCD output:

```text
TEMP: 26.1°C
Q:HIGH STABLE
```

## Hardware

| Component | Purpose |
| --- | --- |
| 8051-compatible MCU | Controls acquisition, conversion timing, and display output |
| LM35 | Analog temperature sensor with a nominal 10 mV/°C response |
| ADC0808 | Converts the LM35 analog voltage into an 8-bit digital value |
| 16x2 LCD | Displays the measured temperature |
| 5 V supply and support components | Powers the digital and analog sections |

## Pin Mapping

| Signal | 8051 connection | Function |
| --- | --- | --- |
| ADC data bus | `P1` | Reads the 8-bit conversion result |
| LCD data bus | `P3` | Sends commands and display data |
| `RS`, `RW`, `EN` | `P2.5`, `P2.6`, `P2.7` | LCD control |
| `ALE`, `OE`, `START`, `EOC` | `P2.3`, `P2.4`, `P2.1`, `P2.0` | ADC0808 control and status |
| ADC clock | `P2.2` | Timer 0 generated clock |
| `CHC`, `CHB`, `CHA` | `P0.7`, `P0.6`, `P0.5` | ADC channel selection |

## Temperature Conversion and Calibration

For an ideal 8-bit ADC:

```text
ADC count N ≈ (LM35 voltage / ADC reference voltage) × 255
Temperature °C ≈ N × ADC reference voltage × 100 / 255
```

The firmware performs this conversion with integer arithmetic. `ADC_VREF_MV` is set to **2560 mV** by default, giving approximately 10 mV per ADC count and therefore close to 1 °C per count with an LM35. Change this constant to the reference voltage measured on the real circuit.

Calibrate the completed circuit against a trusted thermometer before treating its output as an accurate measurement.

## Build and Simulate

### Requirements

- **Keil µVision with the C51 toolchain**, or another compiler supporting `reg51.h`, `sbit`, and the C51 interrupt syntax
- **Proteus Design Suite** for circuit simulation
- An 8051, LM35, ADC0808, and 16x2 LCD circuit matching the pin table above

### Steps

1. Create an 8051 C project in Keil µVision.
2. Add [`DigitalThermometer.c`](./DigitalThermometer.c) to the target.
3. Select the correct 8051-compatible device and enable HEX-file generation.
4. Build the project and load the generated HEX file into the MCU in Proteus.
5. Wire the hardware according to the pin mapping and start the simulation.
6. Confirm that the ADC reference matches `ADC_VREF_MV` in the source.
7. Adjust the LM35 input temperature and verify the temperature, quality, and trend fields on the LCD.

### Run the desktop reference model

The Python file mirrors the embedded measurement algorithm and can be run without 8051 hardware:

```bash
python DigitalThermometer.py
```

It processes example 16-sample windows and prints a two-line LCD preview. It is useful for checking filtering, conversion, quality, trend, and fault logic before changing the embedded firmware.

> A Proteus schematic and compiled HEX file are not currently included in this repository.

## Repository Contents

| File | Status |
| --- | --- |
| [`DigitalThermometer.c`](./DigitalThermometer.c) | Primary 8051 firmware with smart filtering, quality grading, trend detection, and fault handling |
| [`DigitalThermometer.py`](./DigitalThermometer.py) | Runnable desktop reference model of the measurement pipeline |

## Recommended Improvements

- Add the Proteus project and a verified circuit schematic
- Store a two-point calibration in external EEPROM
- Add a push-button page for minimum, maximum, and session-average temperature
- Add UART output for long-term temperature logging
- Document the bill of materials, ADC reference, oscillator, and power supply
- Include tested HEX output and photographs of the completed hardware

## Author

**Sreeraj S** — Electronics and Communication Engineer  
[LinkedIn](https://www.linkedin.com/in/sreeraj-santhosh-64a285243/) · [GitHub](https://github.com/SREERAJSANTHOSH)
