"""Desktop reference model for the 8051 thermal-telemetry firmware.

The module mirrors the embedded trimmed-mean filter, calibration, measurement
quality, trend detection, auto-ranging sparkline, least-squares forecast,
time-to-threshold ETA, thermal-contact detection, and bounded dead reckoning
during sensor faults. It does not access 8051 hardware registers.
"""

from __future__ import annotations

from collections import deque
from collections.abc import Iterable, Sequence
from dataclasses import dataclass
from enum import Enum

ADC_VREF_MV = 2560
SAMPLE_COUNT = 16
TREND_THRESHOLD_TENTHS_C = 10
MAX_VALID_TENTHS_C = 1500
SPARK_LENGTH = 16
MIN_FORECAST_POINTS = 4
FORECAST_HORIZON_WINDOWS = 15
MAX_FORECAST_CHANGE_TENTHS_C = 200
MEASUREMENT_WINDOW_SECONDS = 4
ALERT_THRESHOLD_TENTHS_C = 370
MAX_ETA_SECONDS = 5940
CONTACT_RISE_TENTHS_C = 15
DEAD_RECKON_WINDOWS = 5
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
    contact: bool = False
    eta_seconds: int | None = None
    show_eta_page: bool = False
    dead_reckon_tenths_c: int | None = None
    dead_reckon_remaining: int = 0

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
    """Match C signed integer division, which truncates toward zero."""
    if denominator == 0:
        raise ZeroDivisionError("denominator must be non-zero")
    quotient = abs(numerator) // abs(denominator)
    if (numerator < 0) != (denominator < 0):
        return -quotient
    return quotient


def fit_slope(history: Sequence[int]) -> tuple[int, int] | None:
    """Return the least-squares slope as ``(numerator, denominator)``."""
    if len(history) < MIN_FORECAST_POINTS:
        return None

    n = len(history)
    sum_x = sum(range(n))
    sum_y = sum(history)
    sum_xy = sum(index * value for index, value in enumerate(history))
    sum_x2 = sum(index * index for index in range(n))
    numerator = n * sum_xy - sum_x * sum_y
    denominator = n * sum_x2 - sum_x * sum_x

    if denominator == 0:
        return None
    return numerator, denominator


def forecast_temperature(history: Sequence[int]) -> int:
    """Project 15 windows ahead using a bounded least-squares trend."""
    if not history:
        return 0

    slope = fit_slope(history)
    if slope is None:
        return history[-1]

    numerator, denominator = slope
    change = _truncate_division(
        numerator * FORECAST_HORIZON_WINDOWS,
        denominator,
    )
    change = max(
        -MAX_FORECAST_CHANGE_TENTHS_C,
        min(MAX_FORECAST_CHANGE_TENTHS_C, change),
    )
    return max(0, min(MAX_VALID_TENTHS_C, history[-1] + change))


def eta_to_threshold_seconds(
    history: Sequence[int],
    threshold: int = ALERT_THRESHOLD_TENTHS_C,
    window_seconds: int = MEASUREMENT_WINDOW_SECONDS,
) -> int | None:
    """Return the conservative ETA until the fitted trend crosses a threshold.

    ``None`` means the fit is unavailable, points away from the threshold,
    already reaches the threshold, or exceeds the 99-minute display limit.
    """
    if window_seconds <= 0:
        raise ValueError("window duration must be positive")

    slope = fit_slope(history)
    if slope is None:
        return None

    numerator, denominator = slope
    if numerator == 0:
        return None

    gap = threshold - history[-1]
    if gap == 0 or (gap > 0) != (numerator > 0):
        return None

    # The signs align, so this is a positive ratio. Round up to avoid
    # displaying an ETA earlier than the fitted threshold crossing.
    distance = abs(gap * denominator)
    rate = abs(numerator)
    windows = (distance + rate - 1) // rate
    seconds = windows * window_seconds
    if seconds <= 0 or seconds > MAX_ETA_SECONDS:
        return None
    return seconds


def detect_contact(history: Sequence[int]) -> bool:
    """Detect a rise of at least 1.5 °C across two measurement windows."""
    if len(history) < 3:
        return False
    return history[-1] >= history[-3] + CONTACT_RISE_TENTHS_C


def dead_reckon_estimate(
    last_valid: int,
    slope: tuple[int, int],
    windows_elapsed: int,
) -> int:
    """Extrapolate from the last valid reading using the cached slope."""
    if windows_elapsed < 0:
        raise ValueError("elapsed windows cannot be negative")
    numerator, denominator = slope
    estimate = last_valid + _truncate_division(
        numerator * windows_elapsed,
        denominator,
    )
    return max(0, min(MAX_VALID_TENTHS_C, estimate))


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
        self._slope: tuple[int, int] | None = None
        self._last_valid: int | None = None
        self._fault_windows = 0
        self._page_toggle = False

    def process(self, samples: Iterable[int]) -> Measurement:
        sample_window = tuple(samples)
        filtered_adc, spread = trimmed_mean(sample_window)
        temperature = adc_to_tenths_c(filtered_adc, self.vref_mv)
        fault = temperature > MAX_VALID_TENTHS_C

        if fault:
            self._fault_windows = min(self._fault_windows + 1, 255)
            estimate: int | None = None
            remaining = 0

            if (
                self._last_valid is not None
                and self._slope is not None
                and self._fault_windows <= DEAD_RECKON_WINDOWS
            ):
                estimate = dead_reckon_estimate(
                    self._last_valid,
                    self._slope,
                    self._fault_windows,
                )
                remaining = DEAD_RECKON_WINDOWS - self._fault_windows

            self._previous_temperature = None
            self._history.clear()
            self._page_toggle = False

            return Measurement(
                filtered_adc=filtered_adc,
                spread=spread,
                temperature_tenths_c=temperature,
                forecast_tenths_c=temperature,
                quality=classify_quality(spread),
                trend=Trend.STABLE,
                sparkline=render_sparkline(()),
                sensor_fault=True,
                dead_reckon_tenths_c=estimate,
                dead_reckon_remaining=remaining,
            )

        self._fault_windows = 0
        trend = classify_trend(temperature, self._previous_temperature)
        self._history.append(temperature)
        history = tuple(self._history)

        contact = detect_contact(history)
        forecast = forecast_temperature(history)
        self._slope = fit_slope(history)
        eta = eta_to_threshold_seconds(history)

        if eta is None:
            self._page_toggle = False
        else:
            self._page_toggle = not self._page_toggle

        self._previous_temperature = temperature
        self._last_valid = temperature

        return Measurement(
            filtered_adc=filtered_adc,
            spread=spread,
            temperature_tenths_c=temperature,
            forecast_tenths_c=forecast,
            quality=classify_quality(spread),
            trend=trend,
            sparkline=render_sparkline(history),
            sensor_fault=False,
            contact=contact,
            eta_seconds=eta,
            show_eta_page=eta is not None and self._page_toggle,
        )


def lcd_preview(measurement: Measurement) -> str:
    """Return the exact two-line, 16-column display layout."""
    if measurement.sensor_fault:
        if measurement.dead_reckon_tenths_c is not None:
            second = (
                f"EST {measurement.dead_reckon_tenths_c / 10:.1f}C "
                f"({measurement.dead_reckon_remaining})"
            )
            return f"{' SENSOR FAULT':<16}\n{second[:16]:<16}"
        return " SENSOR FAULT   \n CHECK LM35/ADC "

    if measurement.show_eta_page and measurement.eta_seconds is not None:
        minutes = measurement.eta_seconds // 60
        seconds = measurement.eta_seconds % 60
        first = (
            f"{ALERT_THRESHOLD_TENTHS_C / 10:.1f}C IN "
            f"{minutes:02d}:{seconds:02d}"
        )
        return f"{first[:16]:<16}\n{measurement.sparkline[:16]:<16}"

    if measurement.contact:
        trend_symbol = "!"
    else:
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
    centers = (26, 28, 30, 32, 34, 36)
    sample_windows = [
        [center + offset for offset in offsets] for center in centers
    ]
    sample_windows.extend(
        (
            [200] * SAMPLE_COUNT,
            [200] * SAMPLE_COUNT,
            [36 + offset for offset in offsets],
        )
    )

    for index, samples in enumerate(sample_windows, start=1):
        measurement = thermometer.process(samples)
        eta = (
            f"{measurement.eta_seconds}s"
            if measurement.eta_seconds is not None
            else "-"
        )
        print(
            f"Window {index}: ADC={measurement.filtered_adc}, "
            f"spread={measurement.spread}, "
            f"forecast={measurement.forecast_c:.1f}°C, ETA={eta}"
        )
        print(lcd_preview(measurement))
        print("-" * 16)


if __name__ == "__main__":
    main()
