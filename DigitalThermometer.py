"""Desktop reference model for the 8051 thermal-telemetry firmware.

The module mirrors the embedded trimmed-mean filter, calibration, measurement
quality, trend detection, auto-ranging sparkline, least-squares forecast, and
fault handling. It does not access 8051 hardware registers.
"""

from __future__ import annotations

from collections import deque
from dataclasses import dataclass
from enum import Enum
from typing import Iterable, Sequence


ADC_VREF_MV = 2560
SAMPLE_COUNT = 16
TREND_THRESHOLD_TENTHS_C = 10
MAX_VALID_TENTHS_C = 1500
SPARK_LENGTH = 16
MIN_FORECAST_POINTS = 4
FORECAST_HORIZON_WINDOWS = 15
MAX_FORECAST_CHANGE_TENTHS_C = 200
BLOCKS = "▁▂▃▄▅▆▇█"


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
    forecast_tenths_c: int
    quality: Quality
    trend: Trend
    sparkline: str
    sensor_fault: bool

    @property
    def temperature_c(self) -> float:
        return self.temperature_tenths_c / 10

    @property
    def forecast_c(self) -> float:
        return self.forecast_tenths_c / 10


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


def _truncate_division(numerator: int, denominator: int) -> int:
    """Match C integer division, which truncates toward zero."""
    if denominator <= 0:
        raise ValueError("denominator must be positive")
    if numerator >= 0:
        return numerator // denominator
    return -((-numerator) // denominator)


def forecast_temperature(history: Sequence[int]) -> int:
    """Project 15 windows ahead using a bounded least-squares trend."""
    if not history:
        return 0
    if len(history) < MIN_FORECAST_POINTS:
        return history[-1]

    n = len(history)
    sum_x = sum(range(n))
    sum_y = sum(history)
    sum_xy = sum(index * value for index, value in enumerate(history))
    sum_x2 = sum(index * index for index in range(n))
    numerator = n * sum_xy - sum_x * sum_y
    denominator = n * sum_x2 - sum_x * sum_x

    if denominator == 0:
        return history[-1]

    change = _truncate_division(
        numerator * FORECAST_HORIZON_WINDOWS,
        denominator,
    )
    change = max(
        -MAX_FORECAST_CHANGE_TENTHS_C,
        min(MAX_FORECAST_CHANGE_TENTHS_C, change),
    )
    return max(0, min(MAX_VALID_TENTHS_C, history[-1] + change))


def render_sparkline(history: Sequence[int], width: int = SPARK_LENGTH) -> str:
    """Return a right-aligned, auto-ranging Unicode thermal sparkline."""
    visible = list(history[-width:])
    if not visible:
        return " " * width

    minimum = min(visible)
    maximum = max(visible)
    span = maximum - minimum

    if span == 0:
        chart = BLOCKS[3] * len(visible)
    else:
        levels = [
            ((value - minimum) * 7 + span // 2) // span
            for value in visible
        ]
        chart = "".join(BLOCKS[level] for level in levels)

    return chart.rjust(width)


class SmartThermometer:
    """Stateful reference implementation of the thermal telemetry pipeline."""

    def __init__(self, vref_mv: int = ADC_VREF_MV) -> None:
        if vref_mv <= 0:
            raise ValueError("ADC reference voltage must be positive")
        self.vref_mv = vref_mv
        self._previous_temperature: int | None = None
        self._history: deque[int] = deque(maxlen=SPARK_LENGTH)

    def process(self, samples: Iterable[int]) -> Measurement:
        sample_window = tuple(samples)
        filtered_adc, spread = trimmed_mean(sample_window)
        temperature = adc_to_tenths_c(filtered_adc, self.vref_mv)
        fault = temperature > MAX_VALID_TENTHS_C

        if fault:
            self._previous_temperature = None
            self._history.clear()
            forecast = temperature
            trend = Trend.STABLE
        else:
            trend = classify_trend(temperature, self._previous_temperature)
            self._history.append(temperature)
            forecast = forecast_temperature(tuple(self._history))
            self._previous_temperature = temperature

        return Measurement(
            filtered_adc=filtered_adc,
            spread=spread,
            temperature_tenths_c=temperature,
            forecast_tenths_c=forecast,
            quality=classify_quality(spread),
            trend=trend,
            sparkline=render_sparkline(tuple(self._history)),
            sensor_fault=fault,
        )


def lcd_preview(measurement: Measurement) -> str:
    """Return the exact two-line, 16-column dashboard layout."""
    if measurement.sensor_fault:
        return " SENSOR FAULT   \n CHECK LM35/ADC "

    trend_symbol = {
        Trend.RISING: "^",
        Trend.FALLING: "v",
        Trend.STABLE: "=",
    }[measurement.trend]
    quality_symbol = {
        Quality.HIGH: "H",
        Quality.MEDIUM: "M",
        Quality.LOW: "L",
    }[measurement.quality]
    first = (
        f"{measurement.temperature_c:.1f}C>"
        f"{measurement.forecast_c:.1f}C"
        f"{trend_symbol}{quality_symbol}"
    )
    return f"{first[:16]:<16}\n{measurement.sparkline[:16]:<16}"


def main() -> None:
    thermometer = SmartThermometer()
    offsets = (0, 0, -1, 0, 1, 0, 0, -1, 0, 1, 0, 0, -1, 0, 1, 0)
    sample_windows = (
        [26 + offset for offset in offsets],
        [28 + offset for offset in offsets],
        [30 + offset for offset in offsets],
        [32 + offset for offset in offsets],
        [34 + offset for offset in offsets],
        [36 + offset for offset in offsets],
    )

    for index, samples in enumerate(sample_windows, start=1):
        measurement = thermometer.process(samples)
        print(
            f"Window {index}: ADC={measurement.filtered_adc}, "
            f"spread={measurement.spread}, "
            f"forecast={measurement.forecast_c:.1f}°C"
        )
        print(lcd_preview(measurement))
        print("-" * 16)


if __name__ == "__main__":
    main()

