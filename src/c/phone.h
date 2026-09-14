#pragma once

// AppMessage: das Telefon (src/pkjs) fragt beim Start den Stand ab und
// pflegt daraus die Timeline-Pins (naechste Erinnerung, heute getrunkene
// und verpasste Glaeser).
void phone_init(void);

// Aktuellen Stand ans Telefon schicken: naechste Erinnerung, Tagesziel,
// Zaehler und die heutigen Slots mit Status. Nach jeder Aenderung rufen.
void phone_send_next(void);

// Vermerken, dass gerade ein Glas getrunken wurde. Die NAECHSTE Nachricht
// traegt dann Zeitpunkt und Menge mit; danach ist der Vermerk verbraucht.
//
// WOZU: eine Companion-App auf dem Telefon kann das getrunkene Wasser in eine
// Gesundheitsakte eintragen (Health Connect). Auf der Uhr aendert sich dadurch
// NICHTS - kein Knopf, kein Bildschirm, keine zusaetzliche Uebertragung. Wer
// keine solche App hat, merkt von alldem nichts.
//
// Warum ein Vermerk und nicht einfach immer mitschicken: phone_send_next()
// laeuft auch beim Start und bei jedem Wecker. Stuende die Menge immer dabei,
// trueg die Akte bei jedem Aufwachen ein weiteres Glas ein.
void phone_note_drink(void);
