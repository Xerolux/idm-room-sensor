# Kalibrierung

## Feuchteausgang

1. Ausgang ohne IDM anschließen.
2. 0 %, 25 %, 50 %, 75 % und 100 % vorgeben.
3. Mit einem kalibrierten Multimeter messen.
4. Verstärkung/Offset so abgleichen, dass 0 % = 0,000 V und 100 % = 10,000 V.
5. Danach an der IDM prüfen, ob die angezeigte Feuchte linear folgt.

## Temperaturausgang

1. Einen echten KTY81-210 bei mehreren bekannten Temperaturen vermessen.
2. An der IDM eine Widerstandsdekade zwischen Klemme 43 und 41 statt des Sensors anschließen.
3. Ermitteln, welcher Widerstand welcher angezeigten Temperatur entspricht.
4. Digitalpotentiometer-Codes gegen den realen Widerstand vermessen.
5. Eine Kalibriertabelle Code ↔ Widerstand ↔ Temperatur erstellen.
6. Prüfen, dass bei ESP-Neustart und Busfehler kein unrealistischer niedriger Feuchtewert ausgegeben wird.
