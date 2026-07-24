# Contributing to Digital Thermometer

Thank you for your interest in contributing! This project welcomes improvements to the firmware, reference model, tests, and documentation.

## Getting Started

### Prerequisites

- **Python 3.10+** for the reference model and tests
- **Keil µVision with C51 toolchain** for firmware development
- **Proteus Design Suite** for circuit simulation (optional)

### Setting Up the Development Environment

```bash
# Clone the repository
git clone https://github.com/SREERAJSANTHOSH/Digital_Thermometer.git
cd Digital_Thermometer

# Create a virtual environment (recommended)
python -m venv .venv
source .venv/bin/activate  # On Windows: .venv\Scripts\activate

# Install development dependencies
pip install -e ".[dev]"

# Run the tests
python -m pytest tests/ -v

# Run the linter
ruff check .
```

## How to Contribute

### Reporting Issues

- Use the [bug report template](.github/ISSUE_TEMPLATE/bug_report.md) for bugs
- Use the [feature request template](.github/ISSUE_TEMPLATE/feature_request.md) for new ideas
- Check existing issues before opening a new one

### Submitting Changes

1. Fork the repository
2. Create a feature branch: `git checkout -b feature/your-feature-name`
3. Make your changes
4. Run the tests: `python -m pytest tests/ -v`
5. Run the linter: `ruff check .`
6. Commit with a clear message: `git commit -m "Add: description of change"`
7. Push to your fork: `git push origin feature/your-feature-name`
8. Open a pull request

### Code Style

- **Python**: Follow PEP 8. Use `ruff` for linting. Keep functions focused and well-documented.
- **C (8051)**: Match the existing Keil C51 style. Use descriptive names, document all hardware interactions, and keep ISR code minimal.
- **Documentation**: Use clear, concise language. Keep technical accuracy.

### Testing

- All Python changes must include or update tests
- Tests should be deterministic and not depend on external hardware
- Run the full suite before submitting: `python -m pytest tests/ -v`

## Project Structure

```
DigitalThermometer.c      — 8051 embedded firmware
DigitalThermometer.py     — Desktop Python reference model
tests/                    — Automated test suite
assets/                   — Demo GIF and banner
docs/                     — Extended documentation
```

## License

By contributing, you agree that your contributions will be licensed under the [MIT License](LICENSE).
