# Source Quality

Each value carries a numeric value, timestamp, quality and validity.

`assess_climate_value` evaluates these conditions in safety order:

- explicit invalid flag
- non-finite numeric value
- configured range
- quality encoding (0…100)
- stale timeout, including 32-bit millisecond wraparound
- configured minimum quality

The result contains a stable status name, wrap-safe age and a single `usable`
flag. A stale timeout of zero disables expiry but not the other validity
checks.

## Status

- [x] Unit tested
- [ ] Bench tested
- [ ] Real-device tested
