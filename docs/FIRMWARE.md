# Firmware Architecture

## Overview

The firmware (`DigitalThermometer.c`) runs on an 8051-compatible MCU and implements a complete thermal telemetry pipeline. It is written in Keil C51 and occupies approximately 4 KB of code space.

## Module Structure

The firmware is organized into logical sections within a single file:

### Hardware Abstraction
- **Pin definitions** — `sbit` declarations for LCD and ADC control lines
- **Configuration constants** — ADC reference, oscillator, timing, and measurement parameters

### ADC Interface
- `adc_init()` — Configures Timer 0 in 8-bit auto-reload mode for the ADC clock
- `adc_read_channel_zero()` — Performs a single ADC conversion with ALE/START/EOC/OE handshake
- `adc_read_trimmed_mean()` — Collects 16 samples, rejects min/max, averages the remaining 14

### Measurement Pipeline
- `adc_to_temperature_x10()` — Converts ADC count to tenths of a degree Celsius
- `classify_quality()` — Grades signal quality from the sample spread
- `classify_trend()` — Compares current vs. previous temperature for direction

### History and Prediction
- `history_push()` / `history_reset()` — Manages a 16-point circular temperature buffer
- `forecast_temperature_x10()` — Least-squares trend projection, 15 windows ahead
- `eta_to_threshold_seconds()` — Time-to-threshold with conservative rounding
- `detect_contact()` — Heuristic for rapid temperature rise (touch detection)
- `dead_reckon_estimate_x10()` — Extrapolation during sensor faults using cached slope

### Display
- `lcd_init()` / `lcd_command()` / `lcd_data()` — Low-level HD44780 interface
- `lcd_load_bar_glyphs()` — Programs 8 CGRAM characters for the sparkline
- `lcd_show_dashboard()` — Renders temperature, forecast, trend, and quality on row 1
- `lcd_show_eta_page()` — Alternating ETA display page
- `lcd_show_fault()` / `lcd_show_fault_with_estimate()` — Fault display states
- `sparkline_render()` — Auto-ranging 16-column bar chart on row 2

### ISR and Timing
- `timer0_isr()` — Toggles ADC clock, increments prescaled tick counter
- `timer0_snapshot()` — Atomic read of the tick counter
- `wait_for_next_measurement_window()` — Blocks until the 4-second window expires

## Configuration Constants

| Constant | Default | Description |
|----------|---------|-------------|
| `ADC_VREF_MV` | 2560 | ADC reference voltage in millivolts |
| `OSCILLATOR_HZ` | 11059200 | MCU crystal frequency |
| `TIMER0_RELOAD` | 0xD4 | Timer 0 reload value for ADC clock |
| `MEASUREMENT_WINDOW_SECONDS` | 4 | Duration of each measurement window |
| `SAMPLE_COUNT` | 16 | ADC samples per measurement window |
| `SPARK_LENGTH` | 16 | History depth (matches LCD columns) |
| `ALERT_THRESHOLD_X10` | 370 | Alert threshold in tenths °C (37.0 °C) |
| `CONTACT_RISE_X10` | 15 | Contact detection threshold (1.5 °C / 2 windows) |
| `DEAD_RECKON_WINDOWS` | 5 | Maximum fault estimation windows |
| `MAX_FORECAST_CHANGE_X10` | 200 | Maximum forecast deviation (±20.0 °C) |

## Build Instructions

1. Open Keil µVision and create a new 8051 C project
2. Add `DigitalThermometer.c` as the only source file
3. Select an AT89S52 or compatible device
4. Enable HEX file generation in the output options
5. Build the project (expected: 0 errors, 0 warnings)
6. Load the HEX file into the Proteus simulation or program the physical MCU
