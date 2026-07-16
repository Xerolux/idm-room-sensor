# Validation plan

## Gate A — Documentation review
- Confirm exact IDM model and wiring diagram.
- Confirm GLT register availability and writable metadata.
- Review every assumption in the schematics.

## Gate B — Bench electrical tests
- 14, 24 and 30 V supply tests.
- Reverse polarity and transient protection tests.
- 0–10 V output at 0, 25, 50, 75 and 100 % commands.
- Load tests at 10 kΩ, 5 kΩ and measured IDM input impedance.
- KTY resistance output over -20…60 °C equivalent range.
- Startup, brownout, Wi-Fi loss and I²C fault injection.

## Gate C — IDM input characterization
- Measure KTY input open-circuit voltage.
- Use a calibrated resistance decade to map displayed temperature.
- Verify output-ground topology.
- Confirm humidity scaling using a precision voltage source.

## Gate D — Controlled commissioning
- Independent pipe dew-point switch installed and tested.
- Begin in heating-only observation mode.
- Enable cooling under supervision with data logging.
- Verify safe response to stale values and communications loss.

No checkbox may be marked verified without a dated measurement report.
