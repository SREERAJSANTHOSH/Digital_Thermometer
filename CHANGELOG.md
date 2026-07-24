# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [1.0.0] - 2025-07-24

### Added

- 8051 embedded firmware (`DigitalThermometer.c`) with full measurement pipeline
- 16-sample trimmed-mean filter with spike rejection
- HD44780 CGRAM-based scrolling thermal sparkline on LCD row 2
- Least-squares trend forecast projecting 60 seconds ahead
- Time-to-threshold ETA with conservative rounding
- Thermal-contact detection heuristic (`!` marker)
- Bounded dead-reckoning estimation during sensor faults
- Signal quality grading (HIGH / MED / LOW)
- Desktop Python reference model (`DigitalThermometer.py`) mirroring all firmware logic
- 28-test automated suite covering filtering, charts, forecasts, ETA, contact, and fault recovery
- Animated demo GIF of the two-line LCD dashboard
- Project documentation and README
