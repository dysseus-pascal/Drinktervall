// Alle Texte der Oberflaeche, eine Zeile je Text.
//
// ACHTUNG, ZWEI DINGE SIND ABSICHT:
//  1. KEIN #pragma once und keine Include-Waechter. Diese Datei wird MEHRFACH
//     eingebunden (X-Makro): einmal fuer die Aufzaehlung der Schluessel in
//     strings.h und einmal fuer die Tabelle in strings.c. Ein Waechter wuerde
//     die zweite Einbindung verschlucken und eine leere Tabelle erzeugen.
//  2. Endung .h, obwohl es kein gewoehnlicher Header ist. Sie hiess frueher
//     .def; Build-Umgebungen, die nur .c und .h in ihren Baum kopieren, haben
//     sie dann nicht gefunden ("strings.def: No such file or directory").
//
//   STR(schluessel, maxbytes, en, de, fr, it, es)
//
//   maxbytes  Groesse des Zielpuffers in BYTES, 0 wenn der Text in keinen
//             festen Puffer kopiert wird. Umlaute zaehlen als zwei Bytes.
//             tools/strings_check.js prueft diese Grenze.
//   en        Englisch. Spalte 0 und zugleich der Rueckfall fuer jede Sprache,
//             die hier keine eigene Spalte hat.
//   de        Deutsch.
//   fr        Franzoesisch.
//   it        Italienisch.
//   es        Spanisch.
//
// Ein Budget von n Bytes heisst: hoechstens n-1 Bytes Text, das letzte
// braucht die abschliessende Null. Ein Format mit %d rechnet mit
// zweistelligen Zahlen (das Soll geht bis 16).
//
// EINE SPRACHE ERGAENZEN: in strings.h die Aufzaehlung StringLang und
// STRINGS_LANG_COUNT erweitern, in strings.c prv_pick_language() den
// Zwei-Buchstaben-Vergleich ergaenzen, hier eine Spalte anfuegen und in
// strings.c die Tabellenzeile um sie erweitern. Fehlt die Spalte in auch nur
// einer Zeile, ist das ein Praeprozessorfehler - kein stiller Rueckfall.
//
// Die Texte der Timeline-Pins stehen NICHT hier, sondern in src/pkjs/index.js:
// sie werden auf dem Telefon gebaut. Die Uhr schickt die Sprache als
// MESSAGE_KEY_LANG mit.

// ---- Hauptscreen ----------------------------------------------------------
STR(STR_NEXT_REMINDER,   0,  "Next reminder",   "Nächste Erinnerung", "Prochain rappel", "Prossimo avviso", "Próximo aviso")
STR(STR_TOMORROW,        0,  "Tomorrow",        "Morgen", "Demain", "Domani", "Mañana")
STR(STR_GOAL_REACHED,    20, "Goal reached",    "Ziel erreicht", "Objectif atteint", "Meta raggiunta", "Meta alcanzada")
STR(STR_GLASS_N_OF_M,    20, "Glass %d of %d",  "Glas %d von %d", "Verre %d sur %d", "Bicchiere %d/%d", "Vaso %d de %d")
STR(STR_N_DONE,          20, "%d done",         "%d getrunken", "Déjà bu : %d", "Bevuti: %d", "Bebidos: %d")

// Seitenleiste, auf Hoehe der drei Tasten. Auf flint nur 30 px breit -
// hoechstens fuenf schmale Zeichen.
STR(STR_HINT_PLAN,       0,  "Plan",            "Plan", "Plan", "Piano", "Plan")
STR(STR_HINT_PLUS_ONE,   0,  "+1",              "+1", "+1", "+1", "+1")
STR(STR_HINT_GOAL_UP,    0,  "Goal+",           "Ziel+", "But+", "Meta+", "Meta+")

// ---- Trinkplan ------------------------------------------------------------
STR(STR_GLASS_N,         12, "Glass %d",        "Glas %d", "Verre %d", "Bicch. %d", "Vaso %d")
STR(STR_STATE_DONE,      0,  "done",            "getrunken", "bu", "bevuto", "bebido")
STR(STR_STATE_NEXT,      0,  "next reminder",   "nächste Erinnerung", "prochain rappel", "prossimo avviso", "próximo aviso")
STR(STR_STATE_MISSED,    0,  "missed",          "verpasst", "manqué", "saltato", "perdido")
STR(STR_STATE_OPEN,      0,  "pending",         "offen", "à venir", "in attesa", "pendiente")

// ---- Erinnerung -----------------------------------------------------------
STR(STR_TIME_FOR_WATER,  0,  "Time for a glass of water!", "Zeit für ein Glas Wasser!", "C'est l'heure d'un verre d'eau !", "È ora di un bicchiere d'acqua!", "¡Hora de un vaso de agua!")
STR(STR_DAILY_GOAL_MET,  24, "Daily goal reached", "Tagesziel erreicht", "Objectif atteint", "Obiettivo raggiunto", "Meta diaria alcanzada")

// ---- App-Glance (Eintrag in der Anwendungsliste der Uhr) ------------------
STR(STR_GLANCE_FMT,      48, "%d of %d glasses, next %s", "%d von %d Gläsern, nächste %s", "%d sur %d verres, prochain %s", "%d di %d bicchieri, prossimo %s", "%d de %d vasos, próximo %s")
