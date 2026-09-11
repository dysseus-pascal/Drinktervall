#pragma once

// AppMessage: das Telefon (src/pkjs) fragt beim Start nach der naechsten
// Erinnerung und legt daraus den Timeline-Pin an.
void phone_init(void);

// Naechste Erinnerung (Zeitpunkt, Slot, Tagesziel) ans Telefon schicken.
void phone_send_next(void);
