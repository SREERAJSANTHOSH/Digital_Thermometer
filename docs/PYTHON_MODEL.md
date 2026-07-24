# Python Reference Model

## Overview

`DigitalThermometer.py` is a desktop reference implementation that mirrors every measurement and display algorithm in the 8051 firmware. It uses no hardware registers and runs on any system with Python 3.10+.

## Quick Start

```bash
# Run the built-in demonstration
python DigitalThermometer.py

# Run the test suite
python -m pytest tests/ -v

# Run with coverage
python -m pytest tests/ -v --cov=DigitalThermometer
```

## API Reference

### Core Functions

#### `trimmed_mean(samples: Sequence[int]) → tuple[int, int]`
Accepts exactly 16 ADC samples (0–255). Returns `(filtered_adc, spread)` after rejecting the minimum and maximum.

#### `adc_to_tenths_c(adc_count: int, vref_mv: int = 2560) → int`
Converts an 8-bit ADC count to temperature in tenths of a degree Celsius.

#### `classify_quality(spread: int) → Quality`
Returns `HIGH` (spread ≤ 1), `MEDIUM` (spread ≤ 3), or `LOW`.

#### `classify_trend(current: int, previous: int | None) → Trend`
Returns `RISING`, `FALLING`, or `STABLE` based on a 1.0 °C threshold.

### Prediction Functions

#### `fit_slope(history: Sequence[int]) → tuple[int, int] | None`
Returns the least-squares slope as `(numerator, denominator)`, or `None` if fewer than 4 points.

#### `forecast_temperature(history: Sequence[int]) → int`
Projects 15 windows ahead with the bounded least-squares trend.

#### `eta_to_threshold_seconds(history, threshold=370, window_seconds=4) → int | None`
Returns the conservative ETA in seconds, or `None` if unavailable.

#### `detect_contact(history: Sequence[int]) → bool`
Returns `True` if the last reading is ≥ 1.5 °C above two windows ago.

#### `dead_reckon_estimate(last_valid, slope, windows_elapsed) → int`
Extrapolates from the last valid reading using a cached slope.

### Display Functions

#### `render_sparkline(history: Sequence[int], width: int = 16) → str`
Returns a right-aligned Unicode sparkline string.

#### `lcd_preview(measurement: Measurement) → str`
Returns the exact two-line, 16-column LCD display text.

### SmartThermometer Class

```python
thermometer = SmartThermometer(vref_mv=2560)
measurement = thermometer.process([26, 26, 25, 26, 27, 26, ...])  # 16 samples
print(lcd_preview(measurement))
```

The `SmartThermometer` class maintains internal state (history, slope, fault counter, page toggle) and produces a `Measurement` dataclass for each 16-sample window.

### Measurement Dataclass

| Field | Type | Description |
|-------|------|-------------|
| `filtered_adc` | int | Trimmed mean of ADC samples |
| `spread` | int | Max − min of the 16-sample window |
| `temperature_tenths_c` | int | Temperature in 0.1 °C units |
| `forecast_tenths_c` | int | Predicted temperature |
| `quality` | Quality | HIGH, MEDIUM, or LOW |
| `trend` | Trend | RISING, FALLING, or STABLE |
| `sparkline` | str | 16-character Unicode sparkline |
| `sensor_fault` | bool | True if reading exceeds LM35 range |
| `contact` | bool | True if contact heuristic triggered |
| `eta_seconds` | int or None | Time to threshold in seconds |
| `show_eta_page` | bool | True on alternating ETA display windows |
| `dead_reckon_tenths_c` | int or None | Estimated temperature during fault |
| `dead_reckon_remaining` | int | Remaining estimation budget |

## Extending the Model

To add new features:

1. Implement the algorithm as a standalone function in `DigitalThermometer.py`
2. Add corresponding fields to the `Measurement` dataclass if needed
3. Integrate into `SmartThermometer.process()`
4. Add tests in `tests/test_thermometer.py`
5. Mirror the logic in `DigitalThermometer.c` for firmware parity
