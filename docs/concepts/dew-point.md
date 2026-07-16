# Dew Point

The dew point is the temperature at which water vapour condenses. Radiant cooling surfaces must stay above it with a safety margin.

The firmware and validation tool use the Magnus approximation. Non-finite
inputs, humidity at or below 0 %, humidity above 100 %, and temperatures
outside the formula domain are rejected instead of being silently clamped.
Callers must select their conservative fallback path for such values.

## Status

- [x] Formula and invalid-input unit tests
- [ ] Bench tested
- [ ] Real-device tested
