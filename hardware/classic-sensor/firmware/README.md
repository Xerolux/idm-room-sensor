# Firmware-Grundlage
ATtiny1616 liest SHT45, begrenzt 0–100 % rF und schreibt MCP4725:
DAC = rF / 100 × 4095. Watchdog aktivieren; bei Sensorfehler sicheren hohen Feuchtewert ausgeben.
