# Watchdog

Initialize outputs conservatively before network startup and use watchdog plus brownout detection.

## Status

- [x] Implemented and source-reviewed in the native ESP-IDF target
- [x] Enabled in the reproducible firmware build
- [ ] Bench tested
- [ ] Real-device tested

The native target subscribes its control task to the ESP task watchdog, resets
it from the bounded main loop and enables panic-on-timeout plus brownout
detection in `sdkconfig.defaults`. Analog fail-safe output is attempted before
Wi-Fi startup. These source and build checks do not replace the pending
brownout, blocked-task and long-term hardware fault-injection tests.
