# State Machine

The runtime state machine is implemented in
[`firmware/components/idm_bridge/idm_bridge_core.h`](../../firmware/components/idm_bridge/idm_bridge_core.h)
and shared by the ESPHome bridge and the native ESP-IDF firmware. The canonical
states and their meaning are:

| State | Meaning |
| --- | --- |
| `startup_safe` | Initial state before any valid command; effective output is the configured fallback. |
| `active` | A valid in-range command is being applied; effective output tracks the command. |
| `manual_safe` | An operator forced the fallback (`apply_manual_fallback` / the `/api/v1/fallback` endpoint). |
| `stale_safe` | The command stale timeout elapsed without a fresh command; fallback applied. |
| `invalid_safe` | A command was rejected (out of range, non-finite, or below the quality gate); fallback applied. |
| `output_fault_safe` | The analog output write failed and is latched; fallback applied and retried periodically. |

Transitions (authoritative source is `IdmBridgeCore`):

- `startup_safe` → `active` on the first accepted `set_values`.
- `active` → `stale_safe` when `tick()` detects `now - last_command >= stale_timeout`.
- any state → `invalid_safe` on a rejected command (invalid humidity/temperature/quality).
- any state → `output_fault_safe` when `set_output_fault(true)` latches a write failure.
- `output_fault_safe` → `startup_safe` when `set_output_fault(false)` clears the latch.
- any state → `manual_safe` via `apply_manual_fallback()`.

`safe_active()` is true for every state except `active`, i.e. whenever the
effective output is a fallback rather than a live command. The `BridgeError`
enum records the reason (`none`, `stale_input`, `invalid_humidity`,
`invalid_temperature`, `invalid_values`, `invalid_quality`, `output_failure`).

## Status

- [x] Bridge transitions, stale fallback and output-fault recovery unit tested
      (`tests/cpp/idm_bridge_core_test.cpp`, `tests/test_idm_bridge_core.py`)
- [x] Shared by ESPHome and native ESP-IDF targets
- [ ] Hardware fault-injection tested
- [ ] Real-device tested
- [ ] Community reviewed
