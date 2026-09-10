# AquaTakt

Trink-Erinnerung für Pebble (Emery, Flint, Gabbro): acht Gläser Wasser zwischen
8 und 20 Uhr, alle 90 Minuten eine Erinnerung direkt auf der Watch, dazu ein
Pin pro Erinnerung in der Timeline. Farbschema blau/weiss.

## Bedienung

**Hauptscreen** - Glas mit Füllstand, Zähler "n von 8", nächste Erinnerung.

| Taste  | Aktion                          |
|--------|---------------------------------|
| Oben   | Trinkplan des Tages (Liste)     |
| Mitte  | +1 Glas getrunken               |
| Unten  | -1 Glas (Korrektur)             |

**Erinnerung** - erscheint zur geplanten Zeit von selbst (Wakeup), vibriert
dreimal im Abstand von 20 s und schliesst sich nach 60 s wieder.

| Taste  | Aktion                                  |
|--------|-----------------------------------------|
| Mitte  | Getrunken: Zähler +1                    |
| Unten  | Später: in 10 Minuten nochmals erinnern |
| Zurück | Schliessen ohne zu zählen               |

Der Zähler wird um Mitternacht automatisch auf 0 gesetzt. Die App-Glance im
Launcher zeigt "n von 8 Gläsern, nächste HH:MM".

## Zeitplan

Erinnerungen: 08:00, 09:30, 11:00, 12:30, 14:00, 15:30, 17:00, 18:30.
Alle Werte stehen in `src/c/config.h` (`AT_START_HOUR`, `AT_END_HOUR`,
`AT_GLASSES`, `AT_SNOOZE_MIN`). `AT_GLASSES` darf 8 nicht überschreiten,
weil Pebble pro App höchstens 8 Wakeup-Events erlaubt. Die App plant bei jedem
Start (auch beim Wakeup-Start) alle Wakeups neu, so dass die 8 Slots immer die
nächsten Erinnerungen über die Tagesgrenze hinweg abdecken.

## Timeline

Die Telefonseite (`src/pkjs/index.js`) fragt beim Start die Konfiguration von
der Watch ab und legt für heute und morgen je einen Pin pro Erinnerung an.
Angelegte Pin-IDs werden im localStorage gemerkt, damit nur neue Pins gesendet
werden. Jeder Pin hat die Aktionen "Getrunken" (öffnet die App und zählt +1)
und "App öffnen".

Übertragung: Das JS holt per `Pebble.getTimelineToken` einen Token und sendet
die Pins an `https://timeline-api.rebble.io`. Das funktioniert mit der neuen
Pebble-App (Core Devices), sofern sie bei Rebble angemeldet ist. Nur wenn kein
Token zu bekommen ist, wird die lokale Schnittstelle `Pebble.insertTimelinePin`
versucht. Gesendete Pins werden nach 12 Stunden erneut gesendet, damit sie
eine Neuinstallation überstehen. Im Emulator gibt es keinen Token; die Pins
werden dann übersprungen (Log: "timeline: kein Token").

## Farben

`src/c/theme.h`: PRIMARY BlueMoon `#0055FF`, Wasser VividCerulean `#00AAFF`,
Hintergrund Weiss. Der Hex-Wert von PRIMARY ist in `src/pkjs/index.js`
(`PIN_COLOR`) von Hand kopiert. Auf Flint (Schwarz/Weiss) wird das Wasser als
Grauraster gezeichnet.

## Bauen

Pebble waf verträgt keine Pfade mit Leerzeichen, deshalb wird in WSL unter
`~/aquatakt` gebaut:

```sh
~/sync_aqua.sh                       # Quellen spiegeln + pebble build
pebble install --emulator emery      # oder flint / gabbro
pebble install --phone <IP>          # Developer Connection der Pebble-App
```

Test-Skripte: `~/aquatest.sh <plattform>` (Screenshots der Screens),
`~/aquawake.sh` (Testbuild mit Erinnerung 60 s nach dem Start; setzt
`AT_TEST_WAKEUP` nur in der WSL-Kopie).

Das fertige Paket liegt nach dem Build unter `build/aquatakt.pbw`, eine Kopie
neben dieser README.
