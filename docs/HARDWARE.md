# Hardware Documentation

## Components Required

| Component | Specification | Quantity | Notes |
|-----------|--------------|----------|-------|
| 8051-compatible MCU | AT89S52 or equivalent | 1 | 40-pin DIP package recommended |
| LM35 | TO-92 package | 1 | Precision centigrade temperature sensor |
| ADC0808 | 8-bit, 8-channel ADC | 1 | 10 kHz minimum clock requirement |
| 16×2 LCD | HD44780-compatible | 1 | Standard character LCD module |
| Crystal oscillator | 11.0592 MHz | 1 | Standard 8051 frequency |
| Capacitors | 33 pF | 2 | Crystal load capacitors |
| Capacitor | 10 µF electrolytic | 1 | MCU reset circuit |
| Resistor | 10 kΩ | 1 | MCU reset pull-up |
| Potentiometer | 10 kΩ | 1 | LCD contrast adjustment |
| Power supply | 5 V regulated | 1 | Powers all components |

## Pin Mapping

### 8051 MCU Connections

| 8051 Pin | Connected To | Signal | Direction | Description |
|----------|-------------|--------|-----------|-------------|
| P0.5 | ADC0808 pin 25 | CHA | Output | Channel address bit A |
| P0.6 | ADC0808 pin 24 | CHB | Output | Channel address bit B |
| P0.7 | ADC0808 pin 23 | CHC | Output | Channel address bit C |
| P1.0–P1.7 | ADC0808 pins 17–10 | D0–D7 | Input | 8-bit conversion data |
| P2.0 | ADC0808 pin 7 | EOC | Input | End of conversion flag |
| P2.1 | ADC0808 pin 6 | START | Output | Start conversion pulse |
| P2.2 | ADC0808 pin 10 | CLK | Output | ADC clock (~10.5 kHz) |
| P2.3 | ADC0808 pin 22 | ALE | Output | Address latch enable |
| P2.4 | ADC0808 pin 9 | OE | Output | Output enable |
| P2.5 | LCD pin 4 | RS | Output | Register select |
| P2.6 | LCD pin 5 | RW | Output | Read/write select |
| P2.7 | LCD pin 6 | EN | Output | Enable strobe |
| P3.0–P3.7 | LCD pins 7–14 | DB0–DB7 | Output | 8-bit data bus |

### ADC0808 Configuration

- **Channel 0** is used (CHA = CHB = CHC = 0)
- **Reference voltage**: 2.56 V (adjust `ADC_VREF_MV` in firmware to match)
- **Clock source**: Timer 0 generated via P2.2

### LCD Configuration

- **Mode**: 8-bit data bus
- **Custom characters**: All 8 CGRAM slots used for sparkline bar levels

## Circuit Notes

1. **Power supply**: All components operate at 5 V. Use a regulated supply for stable ADC readings.
2. **ADC reference**: The firmware defaults to 2560 mV. Measure the actual reference voltage on pin 12 of the ADC0808 and update `ADC_VREF_MV` accordingly.
3. **Crystal**: The 11.0592 MHz crystal is standard for 8051 UART baud rates and provides the Timer 0 timebase for the ADC clock.
4. **LCD contrast**: Connect the potentiometer wiper to LCD pin 3 (V0) for contrast adjustment.
5. **LM35 placement**: Position the sensor away from heat-generating components for accurate ambient readings.

## Calibration Procedure

1. Power up the circuit and allow 5 minutes for thermal stabilization
2. Measure the ADC reference voltage on ADC0808 pin 12 with a multimeter
3. Update `ADC_VREF_MV` in the firmware to match (in millivolts)
4. Compare the displayed temperature against a calibrated reference thermometer
5. If readings are consistently offset, adjust `ADC_VREF_MV` until they match
6. Verify `OSCILLATOR_HZ` matches the actual crystal frequency for correct timing
