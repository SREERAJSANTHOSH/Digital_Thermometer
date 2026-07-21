"""Reference model for the 8051 digital thermometer's smart measurement layer.

This module does not access 8051 hardware registers. It mirrors the filtering,
conversion, quality grading, trend detection, and range checks used by the C
firmware so the measurement logic can be exercised on a normal computer.
"""

from __future__ import annotations

from dataclasses import dataclass
from enum import Enum
from typing import Iterable, Sequence


ADC_VREF_MV = 2560
SAMPLE_COUNT = 16
TREND_THRESHOLD_TENTHS_C = 10
MAX_VALID_TENTHS_C = 1500


class Quality(str, Enum):
    HIGH = "HIGH"
    MEDIUM = "MED"
    LOW = "LOW"


class Trend(str, Enum):
    STABLE = "STABLE"
    RISING = "RISING"
    FALLING = "FALLING"


@dataclass(frozen=True)
class Measurement:
    filtered_adc: int
    spread: int
    temperature_tenths_c: int
    quality: Quality
    trend: Trend
    sensor_fault: bool

    @property
    def temperature_c(self) -> float:
        return self.temperature_tenths_c / 10


def trimmed_mean(samples: Sequence[int]) -> tuple[int, int]:
    """Return a rounded mean after rejecting one minimum and one maximum."""
    if len(samples) != SAMPLE_COUNT:
        raise ValueError(f"exactly {SAMPLE_COUNT} ADC samples are required")

    if any(sample < 0 or sample > 255 for sample in samples):
        raise ValueError("ADC samples must be in the range 0..255")

    minimum = min(samples)
    maximum = max(samples)
    trimmed_sum = sum(samples) - minimum - maximum
    trimmed_count = len(samples) - 2
    filtered_adc = (trimmed_sum + trimmed_count // 2) // trimmed_count
    return filtered_adc, maximum - minimum


def adc_to_tenths_c(adc_count: int, vref_mv: int = ADC_VREF_MV) -> int:
    """Convert an ADC0808 count to 0.1 °C units for an LM35 sensor."""
    if not 0 <= adc_count <= 255:
        raise ValueError("ADC count must be in the range 0..255")
    if vref_mv <= 0:
        raise ValueError("ADC reference voltage must be positive")

    # LM35 sensitivity is 10 mV/°C, so 1 mV equals 0.1 °C.
    return (adc_count * vref_mv + 127) // 255


def classify_quality(spread: int) -> Quality:
    if spread <= 1:
        return Quality.HIGH
    if spread <= 3:
        return Quality.MEDIUM
    return Quality.LOW


def classify_trend(current: int, previous: int | None) -> Trend:
    if previous is None:
        return Trend.STABLE
    if current > previous + TREND_THRESHOLD_TENTHS_C:
        return Trend.RISING
    if previous > current + TREND_THRESHOLD_TENTHS_C:
        return Trend.FALLING
    return Trend.STABLE


class SmartThermometer:
    """Stateful reference implementation of the embedded measurement pipeline."""

    def __init__(self, vref_mv: int = ADC_VREF_MV) -> None:
        if vref_mv <= 0:
            raise ValueError("ADC reference voltage must be positive")
        self.vref_mv = vref_mv
        self._previous_temperature: int | None = None

    def process(self, samples: Iterable[int]) -> Measurement:
        sample_window = tuple(samples)
        filtered_adc, spread = trimmed_mean(sample_window)
        temperature = adc_to_tenths_c(filtered_adc, self.vref_mv)
        fault = temperature > MAX_VALID_TENTHS_C
        trend = classify_trend(temperature, self._previous_temperature)

        if fault:
            self._previous_temperature = None
        else:
            self._previous_temperature = temperature

        return Measurement(
            filtered_adc=filtered_adc,
            spread=spread,
            temperature_tenths_c=temperature,
            quality=classify_quality(spread),
            trend=trend,
            sensor_fault=fault,
        )


def lcd_preview(measurement: Measurement) -> str:
    """Return a two-line text preview matching the 16x2 LCD presentation."""
    if measurement.sensor_fault:
        return " SENSOR FAULT   \n CHECK LM35/ADC "

    first = f"TEMP: {measurement.temperature_c:.1f}°C"
    second = f"Q:{measurement.quality.value:<5}{measurement.trend.value}"
    return f"{first[:16]:<16}\n{second[:16]:<16}"


def main() -> None:
    thermometer = SmartThermometer()
    sample_windows = (
        [26, 26, 25, 26, 26, 27, 26, 26, 25, 26, 26, 26, 27, 26, 26, 26],
        [28, 29, 28, 28, 29, 28, 30, 28, 29, 28, 28, 29, 28, 29, 28, 29],
        [27, 25, 28, 26, 30, 24, 29, 26, 27, 25, 28, 26, 27, 25, 28, 26],
    )

    for index, samples in enumerate(sample_windows, start=1):
        measurement = thermometer.process(samples)
        print(f"Window {index}: ADC={measurement.filtered_adc}, "
              f"spread={measurement.spread}")
        print(lcd_preview(measurement))
        print("-" * 16)


if __name__ == "__main__":
    main()

