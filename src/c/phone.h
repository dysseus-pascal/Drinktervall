#pragma once

// AppMessage: das Telefon (src/pkjs) fragt beim Start den Stand ab und
// pflegt daraus die Timeline-Pins (naechste Erinnerung, heute getrunkene
// und verpasste Glaeser).
void phone_init(void);

// Aktuellen Stand ans Telefon schicken: naechste Erinnerung, Tagesziel,
// Zaehler und die heutigen Slots mit Status. Nach jeder Aenderung rufen.
void phone_send_next(void);
