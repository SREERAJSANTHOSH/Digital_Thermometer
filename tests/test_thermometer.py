import unittest

from DigitalThermometer import (
    Quality,
    SmartThermometer,
    Trend,
    classify_quality,
    classify_trend,
    forecast_temperature,
    lcd_preview,
    render_sparkline,
    trimmed_mean,
)


class SmartThermometerTests(unittest.TestCase):
    def test_trimmed_mean_rejects_two_extremes(self):
        filtered, spread = trimmed_mean([26] * 14 + [0, 255])
        self.assertEqual(filtered, 26)
        self.assertEqual(spread, 255)

    def test_quality_thresholds(self):
        self.assertEqual(classify_quality(1), Quality.HIGH)
        self.assertEqual(classify_quality(3), Quality.MEDIUM)
        self.assertEqual(classify_quality(4), Quality.LOW)

    def test_trend_thresholds(self):
        self.assertEqual(classify_trend(281, 261), Trend.RISING)
        self.assertEqual(classify_trend(241, 261), Trend.FALLING)
        self.assertEqual(classify_trend(266, 261), Trend.STABLE)

    def test_flat_sparkline_is_centered(self):
        self.assertEqual(render_sparkline([261] * 16), "▄" * 16)

    def test_sparkline_is_auto_ranged_and_right_aligned(self):
        sparkline = render_sparkline([100, 200])
        self.assertEqual(len(sparkline), 16)
        self.assertTrue(sparkline.endswith("▁█"))

    def test_least_squares_forecast(self):
        self.assertEqual(forecast_temperature([100, 110, 120, 130]), 280)
        self.assertEqual(forecast_temperature([400, 390, 380, 370]), 220)

    def test_lcd_layout_is_exactly_sixteen_columns(self):
        thermometer = SmartThermometer()
        measurement = None
        for center in (26, 28, 30, 32):
            measurement = thermometer.process([center] * 16)

        self.assertIsNotNone(measurement)
        self.assertTrue(
            measurement.forecast_tenths_c
            > measurement.temperature_tenths_c
        )
        self.assertTrue(
            all(len(line) == 16 for line in lcd_preview(measurement).splitlines())
        )

    def test_fault_clears_chart_history(self):
        thermometer = SmartThermometer()
        thermometer.process([26] * 16)
        fault = thermometer.process([200] * 16)
        self.assertTrue(fault.sensor_fault)
        self.assertEqual(fault.sparkline, " " * 16)


if __name__ == "__main__":
    unittest.main()

