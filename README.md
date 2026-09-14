# Drinktervall

Trink-Erinnerung für Pebble (Emery, Flint, Gabbro): acht Gläser Wasser zwischen
8 und 20 Uhr, alle 90 Minuten eine Erinnerung direkt auf der Watch, dazu ein
Pin pro Erinnerung in der Timeline. Farbschema blau/weiss.

Die Oberfläche folgt der **Sprache der Uhr** (Deutsch und Englisch, Englisch als
Rückfall) - siehe Abschnitt [Sprachen](#sprachen).

## Screenshots

**Emery** (200 × 228, Farbe)

| Start | Drei Gläser | Trinkplan | Trinken | Erinnerung |
|---|---|---|---|---|
| ![Start](screenshots/emery/01-start.png) | ![Hauptscreen](screenshots/emery/02-hauptscreen.png) | ![Trinkplan](screenshots/emery/03-trinkplan.png) | ![Trinken](screenshots/emery/04-trinken.png) | ![Erinnerung](screenshots/emery/05-erinnerung.png) |

**Flint** (144 × 168, schwarz/weiss)

| Start | Drei Gläser | Trinkplan | Trinken | Erinnerung |
|---|---|---|---|---|
| ![Start](screenshots/flint/01-start.png) | ![Hauptscreen](screenshots/flint/02-hauptscreen.png) | ![Trinkplan](screenshots/flint/03-trinkplan.png) | ![Trinken](screenshots/flint/04-trinken.png) | ![Erinnerung](screenshots/flint/05-erinnerung.png) |

**Gabbro** (260 × 260, rund)

| Start | Drei Gläser | Trinkplan | Trinken | Erinnerung |
|---|---|---|---|---|
| ![Start](screenshots/gabbro/01-start.png) | ![Hauptscreen](screenshots/gabbro/02-hauptscreen.png) | ![Trinkplan](screenshots/gabbro/03-trinkplan.png) | ![Trinken](screenshots/gabbro/04-trinken.png) | ![Erinnerung](screenshots/gabbro/05-erinnerung.png) |

**Eigenes Pin-Symbol geht zurzeit nicht.** Die App bringt ihr Glas als
Timeline-Ressource mit (`tools/make_glass_icon.py` zeichnet es leer und voll in
25, 50 und 80 Pixeln, `package.json` veröffentlicht es unter `publishedMedia`
als `GLASS_DRUNK` und `GLASS_FULL`). Die Uhr löst das auch auf, im Emulator
nachgewiesen. Die Telefon-App von Core Devices fängt den Timeline-Aufruf aber
selbst ab und kennt nur Namen aus dem System-Satz; einen unbekannten Namen lässt
sie stillschweigend weg, worauf die Uhr ihr Standardsymbol für `genericPin`
zeichnet, eine Flagge. Nachzulesen in `RemoteTimelineEmulator.kt` und
`TimelineIcon.kt`, offener Fehlerbericht: coredevices/mobileapp Issue 275.
Deshalb stehen in `src/pkjs/index.js` vorerst System-Symbole; die Ressourcen
bleiben liegen, ein Namenswechsel plus erhöhtes `LOOK_VERSION` genügt später.

Erzeugt mit `tools/screenshots.sh <plattform> <projektordner>` (Emulator; Trinken und
Erinnerung stammen aus einem Testbuild mit Zeitlupe und Wakeup nach 60 s).

## Bedienung

**Hauptscreen** - im Stil der Pebble-Timeline: weisser Grund, schwarze Schrift,
rechts die dunkelblaue Seitenleiste mit dem Glas-Symbol oben und den
Tasten-Hinweisen. Aufgebaut wie ein Timeline-Eintrag: kleine Uhrzeit, die
nächste Erinnerung in der LECO-Ziffernschrift, darunter "Glas n von Ziel" und
"n getrunken". Der Pegel (getrunkene Gläser / Tagesziel) steigt als hellblaues
Band mit dunkler Wasserlinie von unten über den Inhalt (animiert). Einen Zähler nach
unten gibt es bewusst nicht: ein getrunkenes Glas lässt sich nicht
zurücknehmen. Das Tagesziel beginnt bei 8 und lässt sich mit der unteren
Taste für den laufenden Tag erhöhen.

| Taste  | Aktion                          |
|--------|---------------------------------|
| Oben   | Trinkplan des Tages: Liste wie eine kleine Timeline, Zeit in LECO,
|        | Seitenleiste dunkel für vergangene und hell für kommende Slots,
|        | weisse Pfeilkerbe am gewählten Eintrag |
| Mitte  | Glas getrunken: ein Vollbild-Fenster zeigt ein Glas mit Gesicht,
|        | das aufploppt, sich leert, ins Zentrum schrumpft und in einem
|        | Strahlenkranz zerplatzt; danach steigen Zähler und Pegel. Vom
|        | Hauptscreen aus bleibt die App danach offen |
| Unten  | Tagesziel um ein Glas erhöhen (nur heute, morgen wieder 8), damit
|        | sich über das Ziel hinaus weiter loggen lässt |

**Erinnerung** - wie ein Pin-Detail der Timeline, aber ganz in Weiss: oben das
Glas aus der Trink-Animation und die Uhrzeit in LECO, darunter die schwarze
Trennlinie und die Karte mit dem Aufruf, rechts die schwarze Aktionsleiste mit
Häkchen (Getrunken) und Zz (Später). Erscheint zur geplanten Zeit von selbst (Wakeup), vibriert dreimal im Abstand von 20 s und bleibt stehen, bis eine Taste gedrückt wird.

| Taste  | Aktion                                  |
|--------|-----------------------------------------|
| Mitte  | Getrunken: Zähler +1, kurze Trink-Animation, dann schliesst sich
|        | die App - die Unterbrechung bleibt so kurz wie möglich |
| Unten  | Später: in 10 Minuten nochmals erinnern, die App schliesst sich sofort |
| Zurück | Schliessen ohne zu zählen, das Glas gilt als verpasst; nach einem
|        | Wakeup-Start beendet sich die App, sonst zurück zum Hauptscreen |

Der Zähler wird um Mitternacht automatisch auf 0 gesetzt. Die App-Glance im
Launcher zeigt "n von 8 Gläsern, nächste HH:MM".

## Zeitplan

Grundraster: 08:00, 09:30, 11:00, 12:30, 14:00, 15:30, 17:00, 18:30. Jede
Erinnerung wird um bis zu 10 Minuten vor- oder nachverlegt (`DT_JITTER_MIN`),
deterministisch aus Datum und Slot, so dass Wakeups, Plan-Liste und Glance
dieselben Zeiten zeigen; das Fenster 8 bis 20 Uhr wird nicht verlassen. Alle
Werte stehen in `src/c/config.h` (`DT_START_HOUR`, `DT_END_HOUR`, `DT_GLASSES`,
`DT_JITTER_MIN`, `DT_SNOOZE_MIN`). `DT_GLASSES` darf 8 nicht überschreiten,
weil Pebble pro App höchstens 8 Wakeup-Events erlaubt. Die App plant bei jedem
Start (auch beim Wakeup-Start) alle Wakeups neu, so dass die 8 Slots immer die
nächsten Erinnerungen über die Tagesgrenze hinweg abdecken.

## Timeline

In der Zukunft steht genau ein Pin für die nächste Erinnerung mit den Aktionen
"Getrunken" (öffnet die App und zählt +1) und "App öffnen". In der
Vergangenheit steht für jeden heutigen Slot, der vorbei ist, ein Pin: "Glas n
getrunken" oder "Glas n verpasst" mit der Aktion "Nachholen", die das Glas
nachträglich zählt. Die kommende Erinnerung trägt `NOTIFICATION_REMINDER`, eine
Hand mit Trinkglas, getrunkene Gläser `GENERIC_CONFIRMATION` (einen Stern) und
verpasste `RESULT_DELETED` (einen Totenkopf). Jeder Slot hat die feste ID
`drinktervall-JJJJMMTT-n`; der Pin der nächsten Erinnerung wird nach dem
Slot zum Getrunken- oder Verpasst-Pin. Die Watch schickt der Telefonseite
(`src/pkjs/index.js`) beim Start, bei jedem Wakeup und nach jeder Änderung
des Zählers den Stand; das JS sendet nur Pins, deren Inhalt sich geändert hat.
Die Pin-Texte gibt es auf Englisch und Deutsch; welche gilt, sagt die Watch mit
`MESSAGE_KEY_LANG` (siehe Abschnitt Sprachen).

Übertragung: Das JS holt per `Pebble.getTimelineToken` einen Token und sendet
die Pins an `https://timeline-api.rebble.io`. Das funktioniert mit der neuen
Pebble-App (Core Devices), sofern sie bei Rebble angemeldet ist. Nur wenn kein
Token zu bekommen ist, wird die lokale Schnittstelle `Pebble.insertTimelinePin`
versucht. Unveränderte Pins werden nach 12 Stunden erneut gesendet. Im Emulator gibt
es keinen Token; die Pins werden dann übersprungen (Log: "timeline: kein Token").

## Sprachen

Die App liest beim Start `i18n_get_system_locale()` und folgt damit der
Einstellung der Uhr unter *Settings -> Display -> Language*. Ausgeliefert werden
**Englisch** und **Deutsch**; jede andere Uhrsprache bekommt Englisch. Einen
eigenen Sprachschalter gibt es bewusst nicht.

| Deutsch | Englisch |
|:--:|:--:|
| ![Hauptscreen auf Deutsch](screenshots/emery/06-sprache-de.png) | ![Hauptscreen auf Englisch](screenshots/emery/07-sprache-en.png) |

Alle Texte der Watch stehen in `src/c/strings_table.h`, eine Zeile je Text:

```
STR(STR_GLASS_N_OF_M, 20, "Glass %d of %d", "Glas %d von %d")
```

Die Datei wird zweimal eingebunden (X-Makro) - einmal für die Aufzählung der
Schlüssel, einmal für die Tabelle. Eine Zeile mit einer Spalte zu wenig ist
deshalb ein **Präprozessorfehler**, kein stiller Rückfall auf die falsche
Sprache. `S(STR_...)` liefert den Text; ein unbekannter Schlüssel oder eine
leere Spalte fällt auf Englisch zurück, statt abzustürzen. Verglichen wird nie
auf `"de_DE"`, sondern auf die ersten zwei Zeichen - ein Sprachpaket darf auch
nur `"de"` liefern.

Die **Texte der Timeline-Pins** stehen nicht dort, sondern in
`src/pkjs/index.js`: sie werden auf dem Telefon gebaut, und das kann die
Uhrsprache nicht von sich aus erfahren. Die Watch schickt sie deshalb als
`MESSAGE_KEY_LANG` mit. Wichtig dabei: die Sprache steht auch in der Signatur,
mit der das JS entdoppelt. Sonst behielte ein Pin, der schon draussen ist, nach
einem Sprachwechsel seinen alten Text - sein Zustand hat sich ja nicht
geändert.

`node tools/strings_check.js` prüft, was der Compiler nicht sieht: leere
englische Spalte, doppelte Schlüssel, Überschreitung eines Zielpuffers in Bytes
(Umlaute zählen doppelt), zwischen den Sprachen abweichende Formatplatzhalter
und Schlüssel, die niemand mehr benutzt. Nicht geprüft werden kann, ob ein
Platzhalter zur C-Aufrufstelle passt - ein Format aus einer Tabelle ist auch
für den Compiler unsichtbar.

**Zum Testen:** Der Emulator meldet `en_US`, ein normaler Lauf zeigt also die
englische Oberfläche. Für die deutsche Seite übersteuert man `strings_refresh()`
vorübergehend in der WSL-Kopie mit `prv_pick_language("de_DE")` und lässt die
Windows-Quelle unangetastet.

Kosten: **+459 Byte** auf flint (9 821 -> 10 280 Byte Abdruck), keine neue
Ressource.

## Farben

`src/c/theme.h`: Hauptscreen LEVEL_LIGHT PictonBlue `#55AAFF` (leer) und
LEVEL_DARK DukeBlue `#0000AA` (Wasser), Schrift OxfordBlue bzw. Weiss.
Listen-Hervorhebung PRIMARY BlueMoon `#0055FF`; Erinnerungs-Screen und
Trink-Animation weiss (FX_BG) mit hellblauem Wasser (FX_WATER PictonBlue). Der Hex-Wert von PRIMARY ist in `src/pkjs/index.js`
(`PIN_COLOR`) von Hand kopiert. Auf Flint (Schwarz/Weiss) ist der Pegel des Hauptscreens schwarz, das Wasser in der Trink-Animation grau gerastert.

## Bauen

Pebble waf verträgt keine Pfade mit Leerzeichen, deshalb wird in WSL unter
`~/drinktervall` gebaut:

```sh
tools/sync_drinktervall.sh <Quellordner>    # Quellen spiegeln + pebble build
pebble install --emulator emery      # oder flint / gabbro
pebble install --phone <IP>          # Developer Connection der Pebble-App
```

Die Skripte liegen unter `tools/` (nach `~` kopieren oder direkt aufrufen):
`sync_drinktervall.sh [<Quellordner>]` spiegelt und baut, `test_screens.sh <plattform>`
macht Screenshots der Screens nach /tmp/drinktervall, `test_wakeup.sh` ist der
Wakeup-Test (Testbuild mit Erinnerung 60 s nach dem Start; setzt
`DT_TEST_WAKEUP` nur in der WSL-Kopie).

Das fertige Paket liegt nach dem Build unter `build/drinktervall.pbw`, eine Kopie
neben dieser README.

## Lizenz

Gemeinfrei, [CC0 1.0](LICENSE). Kopieren, ändern, verkaufen, einbauen — ohne
Bedingung, ohne Namensnennung, ohne Rückfrage.

CC0 statt der Unlicense, weil das Schweizer Urheberrecht einen Verzicht gar
nicht kennt; CC0 trägt für genau diesen Fall eine Ersatzlizenz in sich, die
dasselbe erlaubt.

Alles im Repository ist eigene Arbeit. Das Snooze-Symbol der Aktionsleiste war
es bis 1.6.2 nicht: es stammte aus einer Pebble-App ohne Lizenz. Ersetzt durch
ein eigenes, das `tools/make_snooze_icon.js` erzeugt — damit die Widmung
lückenlos gilt.
