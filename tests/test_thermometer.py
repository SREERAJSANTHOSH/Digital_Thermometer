import unittest

from src.DigitalThermometer import (
    Quality,
    SmartThermometer,
    Trend,
    classify_quality,
    classify_trend,
    dead_reckon_estimate,
    detect_contact,
    eta_to_threshold_seconds,
    fit_slope,
    forecast_temperature,
    lcd_preview,
    render_sparkline,
    trimmed_mean,
)


class MeasurementCoreTests(unittest.TestCase):
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

    def test_slope_export_matches_forecast_math(self):
        self.assertEqual(fit_slope([100, 110, 120, 130]), (200, 20))
        self.assertIsNone(fit_slope([100, 110, 120]))


class EtaTests(unittest.TestCase):
    def test_eta_exact_value_on_known_ramp(self):
        # +1.0 °C/window, 24.0 °C gap: 24 windows × 4 seconds.
        self.assertEqual(
            eta_to_threshold_seconds([100, 110, 120, 130]),
            96,
        )

    def test_eta_rounds_up_to_avoid_early_prediction(self):
        # +1.1 °C/window leaves a fractional 21.55-window crossing.
        self.assertEqual(
            eta_to_threshold_seconds([100, 111, 122, 133]),
            88,
        )

    def test_eta_none_when_trend_points_away(self):
        self.assertIsNone(eta_to_threshold_seconds([300, 290, 280, 270]))

    def test_eta_none_at_threshold(self):
        self.assertIsNone(eta_to_threshold_seconds([340, 350, 360, 370]))

    def test_eta_none_with_flat_history(self):
        self.assertIsNone(eta_to_threshold_seconds([260, 260, 260, 260]))

    def test_eta_none_when_beyond_display_limit(self):
        history = [100] * 15 + [101]
        self.assertIsNone(eta_to_threshold_seconds(history))


class ContactTests(unittest.TestCase):
    def test_contact_fires_at_exact_boundary(self):
        self.assertTrue(detect_contact([260, 267, 275]))

    def test_contact_fires_above_boundary(self):
        self.assertTrue(detect_contact([260, 260, 280]))

    def test_contact_ignores_slow_rise(self):
        self.assertFalse(detect_contact([260, 260, 274]))

    def test_contact_needs_three_points(self):
        self.assertFalse(detect_contact([260, 280]))


class DeadReckoningTests(unittest.TestCase):
    @staticmethod
    def _rising_thermometer():
        thermometer = SmartThermometer()
        for center in (26, 28, 30, 32):
            thermometer.process([center] * 16)
        return thermometer

    def test_dead_reckon_estimate_value(self):
        self.assertEqual(
            dead_reckon_estimate(321, (400, 20), 1),
            341,
        )

    def test_dead_reckon_estimate_is_clamped(self):
        self.assertEqual(dead_reckon_estimate(1490, (400, 20), 2), 1500)
        self.assertEqual(dead_reckon_estimate(10, (-400, 20), 2), 0)

    def test_fault_shows_bounded_estimate(self):
        thermometer = self._rising_thermometer()
        fault = thermometer.process([200] * 16)
        self.assertTrue(fault.sensor_fault)
        self.assertEqual(fault.dead_reckon_tenths_c, 341)
        self.assertEqual(fault.dead_reckon_remaining, 4)
        self.assertIn("EST 34.1C (4)", lcd_preview(fault))

    def test_dead_reckoning_budget_expires(self):
        thermometer = self._rising_thermometer()
        fault = None
        for _ in range(6):
            fault = thermometer.process([200] * 16)
        self.assertIsNotNone(fault)
        self.assertTrue(fault.sensor_fault)
        self.assertIsNone(fault.dead_reckon_tenths_c)
        self.assertIn("CHECK LM35/ADC", lcd_preview(fault))

    def test_fault_without_slope_does_not_invent_estimate(self):
        thermometer = SmartThermometer()
        thermometer.process([26] * 16)
        fault = thermometer.process([200] * 16)
        self.assertIsNone(fault.dead_reckon_tenths_c)

    def test_recovery_resets_fault_budget(self):
        thermometer = self._rising_thermometer()
        for _ in range(6):
            thermometer.process([200] * 16)
        for center in (26, 28, 30, 32):
            thermometer.process([center] * 16)
        fault = thermometer.process([200] * 16)
        self.assertEqual(fault.dead_reckon_remaining, 4)
        self.assertIsNotNone(fault.dead_reckon_tenths_c)


class DisplayTests(unittest.TestCase):
    def test_eta_page_alternates_with_dashboard(self):
        thermometer = SmartThermometer()
        pages = []
        for center in (26, 27, 28, 29, 30, 31, 32, 33):
            measurement = thermometer.process([center] * 16)
            if measurement.eta_seconds is not None:
                pages.append(measurement.show_eta_page)

        self.assertGreaterEqual(len(pages), 4)
        for previous, current in zip(pages, pages[1:]):
            self.assertNotEqual(previous, current)

    def test_eta_page_layout(self):
        thermometer = SmartThermometer()
        eta_view = None
        for center in (26, 27, 28, 29, 30, 31, 32, 33):
            measurement = thermometer.process([center] * 16)
            if measurement.show_eta_page:
                eta_view = lcd_preview(measurement)
                break

        self.assertIsNotNone(eta_view)
        first, second = eta_view.splitlines()
        self.assertEqual(len(first), 16)
        self.assertEqual(len(second), 16)
        self.assertRegex(first, r"37\.0C IN \d{2}:\d{2}")

    def test_dashboard_layout_is_exactly_sixteen_columns(self):
        thermometer = SmartThermometer()
        measurement = None
        for center in (26, 28, 30):
            measurement = thermometer.process([center] * 16)

        self.assertIsNotNone(measurement)
        self.assertTrue(
            all(len(line) == 16 for line in lcd_preview(measurement).splitlines())
        )

    def test_contact_symbol_overrides_trend(self):
        thermometer = SmartThermometer()
        measurement = None
        for center in (26, 28, 30):
            measurement = thermometer.process([center] * 16)

        self.assertIsNotNone(measurement)
        self.assertTrue(measurement.contact)
        self.assertEqual(measurement.trend, Trend.RISING)
        first_line = lcd_preview(measurement).splitlines()[0]
        self.assertIn("!", first_line)
        self.assertNotIn("^", first_line)

    def test_fault_clears_chart_history(self):
        thermometer = SmartThermometer()
        thermometer.process([26] * 16)
        fault = thermometer.process([200] * 16)
        self.assertTrue(fault.sensor_fault)
        self.assertEqual(fault.sparkline, " " * 16)


if __name__ == "__main__":
    unittest.main()
