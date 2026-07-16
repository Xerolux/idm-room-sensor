# Strategy Critical Margin

Calculate each room's dew point from its paired temperature and humidity
measurements. Select the highest dew point, which is equivalent to the smallest
margin for a shared pipe/surface temperature.

Do not select the highest humidity independently: a warmer room with lower
relative humidity can still have the higher dew point. The example package
keeps each room's temperature and humidity paired throughout selection.

## Status

- [x] Implemented in the example package
- [x] Mathematical regression test included
- [ ] Bench tested
- [ ] Real-device tested
