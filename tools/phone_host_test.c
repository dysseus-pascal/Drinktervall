// Der Weg zum Telefon (phone.c) und die eigenen Getraenke (coffee.c) - auf
// dem Rechner, mit einer Attrappe fuer AppMessage und Zeitgeber
// (tools/host/attrappe.c).
//
//   sh tools/phone_host_test.sh
//
// Was hier leicht falsch und teuer ist:
//
//   - TAG ZUM ZIEL (1.19): GOAL_DAY ist der Tag, zu dem Zaehler und Ziel
//     gehoeren - nicht der der Uhrzeit.
//   - DER GROESSTE FALL PASST (Audit M9): drei eigene Getraenke mit langen
//     Namen, grossen Werten und Erinnerung, dazu Glas und eigenes Getraenk
//     unterwegs. Bis 1.19 schnitt die Uhr den Text still nach 83 Zeichen ab:
//     im Fall unten fehlten dem dritten Getraenk Koffein und Erinnerung
//     (im Emulator: die Konfigseite zeigte danach Koffein 0, Erinnerung aus).
//   - DIE FRAGE NACH DER ZEIT (Audit M10): steht die Uhr hinter ihrem
//     gemerkten Tag, traegt die Meldung UHRZEIT; bestaetigt die Antwort des
//     Telefons die Uhr, geht der Stand mit dem berichtigten Tag hinaus.
//   - Ein Glas oder Kaffee ist erst mit der BESTAETIGUNG verbraucht - und nur,
//     wenn seine Felder wirklich in der Nachricht standen.
//   - Was nicht passt oder nicht hinausgeht, steht im Log.
//   - Namen werden nie mitten in einem UTF-8-Zeichen gekuerzt.
//   - FELDER IN ANDERER FORM (Audit N1): ein Text oder eine schmale Zahl vom
//     Telefon darf nicht als int32 gelesen werden - bis 1.20 wurde aus beidem
//     Soll 16.
//   - NACH FUENF FEHLVERSUCHEN (Audit N3) versucht ein neuer Anlass es
//     wieder - bis 1.20 wartete ein neues Glas bis zum naechsten Start.
//
// Exitcode 0 = alles wie zugesagt.
#include <pebble.h>
#include <sys/wait.h>
#include <unistd.h>
#include "phone.h"
#include "schedule.h"
#include "coffee.h"
#include "strings.h"

time_t stub_jetzt;
void main_window_refresh(void) {}

static int s_fehler;
static void pruefe(const char *was, bool ok) {
  printf("%s %s\n", ok ? "  ok    " : "  FEHLER", was);
  if (!ok) s_fehler++;
}

static Tuple *feld(uint32_t key) {
  return attrappe_letzte() ? dict_find(attrappe_letzte(), key) : NULL;
}
static bool hat(uint32_t key) { return feld(key) != NULL; }
static int32_t zahl(uint32_t key) {
  Tuple *t = feld(key);
  return t ? t->value->int32 : -999;
}
static const char *text(uint32_t key) {
  Tuple *t = feld(key);
  return t && t->type == TUPLE_CSTRING ? t->value->cstring : "";
}
static bool im_log(const char *was) { return strstr(attrappe_log_text, was) != NULL; }

// Ortszeit = UTC (der Test laeuft mit TZ=UTC).
static time_t um(int jahr, int monat, int tag, int std, int min) {
  struct tm t = { .tm_year = jahr - 1900, .tm_mon = monat - 1, .tm_mday = tag,
                  .tm_hour = std, .tm_min = min };
  return timegm(&t);
}

// Persist-Fach des Tages aus config.h, mit Wert festgenagelt.
#define FACH_TAG 1

// App frisch starten zur Uhrzeit `t`, Persist bleibt (neu = leeren).
static void start(time_t t, bool neu) {
  if (neu) attrappe_persist_leeren();
  attrappe_nachrichten_leeren();
  attrappe_log_leeren();
  while (attrappe_zeitgeber_offen()) attrappe_zeitgeber_ablaufen();
  attrappe_nachrichten_leeren();
  stub_jetzt = t;
  strings_refresh();
  schedule_init();
  coffee_init();
  phone_init();
}

static void abschnitt_tag(void) {
  printf("\nTag zum Ziel (1.19)\n");
  start(um(2026, 10, 4, 9, 0), true);
  schedule_raise_goal();
  phone_send_next();
  pruefe("GOAL_DAY ist heute, GLASSES das erhoehte Ziel",
         zahl(MESSAGE_KEY_GOAL_DAY) == 20261004 && zahl(MESSAGE_KEY_GLASSES) == 9);
  attrappe_ack();
  // Neustart, die Uhr steht kurz auf gestern 23:50: Ziel und Tag bleiben die
  // des gemerkten Tages - und dieser Tag geht hinaus, nicht der der Uhr.
  start(um(2026, 10, 3, 23, 50), false);
  phone_send_next();
  pruefe("Uhr zu frueh: GOAL_DAY ist der gemerkte Tag", zahl(MESSAGE_KEY_GOAL_DAY) == 20261004);
  pruefe("Uhr zu frueh: GLASSES ist dessen Ziel", zahl(MESSAGE_KEY_GLASSES) == 9);
}

static void zeit_vom_telefon(time_t t) {
  DictionaryIterator *ein = attrappe_eingang_beginn();
  dict_write_int32(ein, MESSAGE_KEY_UHRZEIT, (int32_t)t);
  attrappe_eingang_zustellen();
}

static void abschnitt_vorlauf(void) {
  printf("\nVorlauf (M10): Frage nach der Zeit, der Tag, der hinausgeht\n");
  start(um(2026, 10, 3, 8, 0), true);
  phone_send_next();
  pruefe("Uhr auf dem gemerkten Tag: keine Frage", !hat(MESSAGE_KEY_UHRZEIT));
  start(um(2026, 10, 4, 9, 0), false);           // Uhr einen Tag voraus
  start(um(2026, 10, 3, 9, 30), false);          // zurueckgestellt
  schedule_set_count(schedule_count() + 1);
  phone_note_drink();
  phone_send_next();
  pruefe("zurueckgestellt: die Meldung fragt, mit der Zeit der Uhr",
         zahl(MESSAGE_KEY_UHRZEIT) == (int32_t)um(2026, 10, 3, 9, 30));
  pruefe("unbestaetigt: GOAL_DAY ist der gemerkte Tag", zahl(MESSAGE_KEY_GOAL_DAY) == 20261004);
  attrappe_ack();
  while (attrappe_zeitgeber_offen()) attrappe_zeitgeber_ablaufen();
  attrappe_ack();
  const int vorher = attrappe_gesendet();
  zeit_vom_telefon(um(2026, 10, 3, 9, 40));      // 600 s: die Uhr geht falsch
  pruefe("600 s Abweichung: der gemerkte Tag bleibt", persist_read_int(FACH_TAG) == 20261004);
  pruefe("600 s Abweichung: steht im Log", im_log("Telefonzeit weicht -600 s ab"));
  pruefe("600 s Abweichung: keine Meldung", attrappe_gesendet() == vorher);
  zeit_vom_telefon(um(2026, 10, 3, 9, 30) + 5);  // passt
  pruefe("bestaetigt: der Tag ist heute", persist_read_int(FACH_TAG) == 20261003);
  pruefe("bestaetigt: der Stand geht hinaus, mit dem berichtigten Tag",
         attrappe_gesendet() == vorher + 1 && zahl(MESSAGE_KEY_GOAL_DAY) == 20261003 && zahl(MESSAGE_KEY_COUNT) == 1);
  pruefe("bestaetigt: und fragt nicht mehr", !hat(MESSAGE_KEY_UHRZEIT));
}

// Drei eigene Getraenke im groessten Fall: 15 Byte Name, Hoechstwerte.
static const char *const GROESSTE =
    "Proteinshake 15|65535|65535|1439\n"
    "Kokoswasser 150|65535|65535|1439\n"
    "Matcha Latte 15|65535|65535|1439";
// Der Fall aus dem Audit: lange Namen, Werte an der Grenze der Konfigseite,
// Erinnerung 22:00.
static const char *const AUDIT =
    "Proteinshake 15|2000|1000|1320\n"
    "Kokoswasser 150|2000|1000|1320\n"
    "Matcha Latte 15|2000|1000|1320";

static void abschnitt_groesster_fall(void) {
  printf("\nGroesster Fall (M9)\n");
  start(um(2026, 10, 3, 21, 0), true);
  pruefe("Postausgang 448 Byte", attrappe_ausgang_groesse() == 448);
  pruefe("Platz fuer die eigenen Getraenke: 99 Byte", CUSTOM_TEXT_MAX == 99);
  schedule_set_target(16);
  const uint8_t plan[] = { 4, 0x1c, 0x02, 0, 7, 0x58, 0x02, 1, 7, 0x94, 0x02, 2, 7, 0xd0, 0x02, 3, 2 };
  coffee_from_bytes(plan, sizeof(plan));
  custom_from_string(GROESSTE);
  phone_note_custom(custom_drink(0));
  schedule_set_count(schedule_count() + 1);
  phone_note_drink();
  attrappe_log_leeren();
  phone_send_next();
  pruefe("nichts fehlt, nichts im Log", !im_log("unvollstaendig") && !im_log("passen nicht"));
  pruefe("CUSTOM ganz, Byte fuer Byte", strcmp(text(MESSAGE_KEY_CUSTOM), GROESSTE) == 0);
  pruefe("16 Slots", feld(MESSAGE_KEY_SLOTS) && feld(MESSAGE_KEY_SLOTS)->length == 80);
  pruefe("Kaffeeplan mit 4 Kaffees", feld(MESSAGE_KEY_COFFEE) && feld(MESSAGE_KEY_COFFEE)->length == 17);
  pruefe("eigenes Getraenk unterwegs, mit Name",
         zahl(MESSAGE_KEY_COFFEE_KIND) == 4 && strcmp(text(MESSAGE_KEY_DRINK_NAME), "Proteinshake 15") == 0 &&
         zahl(MESSAGE_KEY_DRINK_KCAL) == 65535 && zahl(MESSAGE_KEY_DRINK_MG) == 65535);
  pruefe("Glas unterwegs", hat(MESSAGE_KEY_DRANK_AT) && zahl(MESSAGE_KEY_GLASS_ML) == 300);
  printf("         (Nachricht %u von %u Byte)\n", (unsigned)attrappe_letzte()->belegt,
         (unsigned)attrappe_ausgang_groesse());
  pruefe("groesster Fall: 395 Byte", attrappe_letzte()->belegt == 395);

  custom_from_string(AUDIT);
  attrappe_ack();
  while (attrappe_zeitgeber_offen()) { attrappe_zeitgeber_ablaufen(); attrappe_ack(); }
  phone_send_next();
  pruefe("Fall aus dem Audit: alle drei Zeilen ganz, die dritte mit Koffein 1000 und 22:00 (1320)",
         strcmp(text(MESSAGE_KEY_CUSTOM), AUDIT) == 0);
}

static void abschnitt_vorlauf_groesster_fall(void) {
  printf("\nGroesster Fall mit der Frage nach der Zeit\n");
  start(um(2026, 10, 4, 9, 0), true);            // gemerkt der 04.10. ...
  start(um(2026, 10, 3, 21, 0), false);          // ... die Uhr auf dem 03.10.
  schedule_set_target(16);
  const uint8_t plan[] = { 4, 0x1c, 0x02, 0, 7, 0x58, 0x02, 1, 7, 0x94, 0x02, 2, 7, 0xd0, 0x02, 3, 2 };
  coffee_from_bytes(plan, sizeof(plan));
  custom_from_string(GROESSTE);
  phone_note_custom(custom_drink(0));
  schedule_set_count(schedule_count() + 1);
  phone_note_drink();
  attrappe_log_leeren();
  phone_send_next();
  pruefe("mit der Frage: nichts fehlt, nichts im Log", !im_log("unvollstaendig") && !im_log("passen nicht"));
  pruefe("die Frage ist da", hat(MESSAGE_KEY_UHRZEIT));
  printf("         (Nachricht %u von %u Byte)\n", (unsigned)attrappe_letzte()->belegt,
         (unsigned)attrappe_ausgang_groesse());
  pruefe("groesster Fall mit der Frage: 395 + 11 = 406 Byte", attrappe_letzte()->belegt == 406);
}

static void abschnitt_text_grenze(void) {
  printf("\nText der eigenen Getraenke: ganz oder gar nicht\n");
  start(um(2026, 10, 3, 9, 0), true);
  custom_from_string("Mate|20|80|-1\nCola|140|35|1080");
  char buf[CUSTOM_TEXT_MAX];
  const char *soll = "Mate|20|80|-1\nCola|140|35|1080";
  const size_t n = strlen(soll) + 1;
  pruefe("passt genau: true und ganz", custom_to_string(buf, n) && strcmp(buf, soll) == 0);
  pruefe("ein Byte zu wenig: false", !custom_to_string(buf, n - 1));
  pruefe("viel zu wenig: false", !custom_to_string(buf, 6));
}

static void abschnitt_schlange(void) {
  printf("\nSchlange: verbraucht erst mit der Bestaetigung\n");
  start(um(2026, 10, 3, 9, 0), true);
  phone_note_drink();
  phone_send_next();
  pruefe("Glas faehrt mit", hat(MESSAGE_KEY_DRANK_AT));
  attrappe_nack(APP_MSG_SEND_TIMEOUT);
  pruefe("abgelehnt: steht im Log", im_log("Nachricht nicht angekommen: 2"));
  pruefe("abgelehnt: Glas wartet weiter", phone_pending());
  attrappe_zeitgeber_ablaufen();
  pruefe("nachgefasst, wieder mit dem Glas", attrappe_gesendet() == 2 && hat(MESSAGE_KEY_DRANK_AT));
  attrappe_ack();
  pruefe("bestaetigt: Glas ist weg", !phone_pending());
}

static void abschnitt_passt_nicht(void) {
  printf("\nPasst nicht: Glas bleibt, steht im Log, kein Dauerfeuer\n");
  start(um(2026, 10, 3, 9, 0), true);
  phone_note_drink();
  attrappe_ausgang_begrenzen(150);                // reicht fuer Zahlen, nicht fuer alles
  phone_send_next();
  pruefe("unvollstaendig steht im Log", im_log("Standmeldung unvollstaendig: 2"));
  pruefe("das Glas stand nicht drin", !hat(MESSAGE_KEY_DRANK_AT));
  attrappe_ack();
  pruefe("bestaetigt, aber das Glas wartet weiter", phone_pending());
  pruefe("nachgefasst in 1500 ms, nicht in 150", attrappe_zeitgeber_offen() == 1 && attrappe_zeitgeber_ms(0) == 1500);
  int runden = 0;
  while (attrappe_zeitgeber_offen() && runden < 20) {
    attrappe_ausgang_begrenzen(150);
    attrappe_zeitgeber_ablaufen();
    attrappe_ack();
    runden++;
  }
  pruefe("nach fuenf Anlaeufen Ruhe", runden == 5 && phone_pending());
}

static void abschnitt_kaffee_passt_nicht(void) {
  printf("\nPasst nicht: auch ein eigenes Getraenk bleibt in der Schlange\n");
  start(um(2026, 10, 3, 9, 0), true);
  custom_from_string("Proteinshake 15|120|0|-1");
  phone_note_custom(custom_drink(0));
  attrappe_ausgang_begrenzen(200);                // Zeit und Sorte passen, der Rest nicht
  phone_send_next();
  pruefe("unvollstaendig steht im Log", im_log("Standmeldung unvollstaendig: 2"));
  pruefe("nur ein Teil stand drin", hat(MESSAGE_KEY_COFFEE_AT) && !hat(MESSAGE_KEY_DRINK_MG));
  attrappe_ack();
  pruefe("bestaetigt, aber das Getraenk wartet weiter", phone_pending());
  attrappe_ausgang_begrenzen(0);                  // wieder voller Postausgang
  attrappe_zeitgeber_ablaufen();
  pruefe("beim naechsten Mal ganz", hat(MESSAGE_KEY_DRINK_NAME) && zahl(MESSAGE_KEY_DRINK_KCAL) == 120 &&
         hat(MESSAGE_KEY_DRINK_MG));
  attrappe_ack();
  pruefe("bestaetigt: weg", !phone_pending());
}

static void abschnitt_nicht_abgeschickt(void) {
  printf("\nNicht abgeschickt: Glas bleibt, steht im Log\n");
  start(um(2026, 10, 3, 9, 0), true);
  phone_note_drink();
  attrappe_senden_scheitert(APP_MSG_NOT_CONNECTED);
  phone_send_next();
  pruefe("steht im Log", im_log("Standmeldung nicht abgeschickt: 8"));
  pruefe("Glas wartet weiter", phone_pending());
  pruefe("nachgefasst in 1500 ms", attrappe_zeitgeber_offen() == 1 && attrappe_zeitgeber_ms(0) == 1500);
  attrappe_zeitgeber_ablaufen();
  pruefe("beim Nachfassen faehrt das Glas mit", hat(MESSAGE_KEY_DRANK_AT));
  attrappe_ack();
  pruefe("bestaetigt: weg", !phone_pending());
}

static void abschnitt_namen(void) {
  printf("\nNamen: hoechstens 15 Byte, nie mitten im Zeichen\n");
  start(um(2026, 10, 3, 9, 0), true);
  // [vom Telefon, was auf der Uhr steht] - von Hand an der letzten ganzen
  // Zeichengrenze gekuerzt.
  const char *const faelle[][2] = {
    { "abcdefghijklmn\xc3\xbc", "abcdefghijklmn" },               // ue haette Byte 15 und 16
    { "abcdefghijklm\xe2\x82\xac", "abcdefghijklm" },             // Euro: 3 Byte
    { "abcdefghijkl\xf0\x9f\x98\x80", "abcdefghijkl" },           // Emoji: 4 Byte
    { "abcdefghijk\xf0\x9f\x98\x80x", "abcdefghijk\xf0\x9f\x98\x80" }, // Emoji passt genau
    { "\xc3\x84pfels\xc3\xa4ure \xc3\x96l \xc3\x9c", "\xc3\x84pfels\xc3\xa4ure \xc3\x96" },
  };
  for (unsigned i = 0; i < sizeof(faelle) / sizeof(faelle[0]); i++) {
    char zeile[64];
    snprintf(zeile, sizeof(zeile), "%s|10|0|-1", faelle[i][0]);
    custom_from_string(zeile);
    const CustomDrink *d = custom_drink(0);
    char was[96];
    snprintf(was, sizeof(was), "Fall %u: \"%s\"", i + 1, faelle[i][1]);
    pruefe(was, d && strcmp(d->name, faelle[i][1]) == 0);
  }
}

static void abschnitt_feldform(void) {
  printf("\nFelder in anderer Form (N1): Text und schmale Zahlen\n");
  start(um(2026, 10, 3, 9, 0), true);
  DictionaryIterator *ein = attrappe_eingang_beginn();
  dict_write_cstring(ein, MESSAGE_KEY_TARGET, "12");
  attrappe_eingang_zustellen();
  pruefe("TARGET als Text \"12\": das Soll bleibt 8", schedule_target() == 8);
  pruefe("TARGET als Text: steht im Log", im_log("in falscher Form"));
  attrappe_ack();
  ein = attrappe_eingang_beginn();
  dict_write_uint8(ein, MESSAGE_KEY_TARGET, 6);
  dict_write_int32(ein, MESSAGE_KEY_GLASS_ML, 500);
  attrappe_eingang_zustellen();
  pruefe("TARGET als ein Byte 6, GLASS_ML dahinter: Soll 6", schedule_target() == 6);
  pruefe("und die Glasgroesse 500", schedule_glass_ml() == 500);
  pruefe("die Meldung danach traegt Soll 6", zahl(MESSAGE_KEY_TARGET) == 6);
  attrappe_ack();
  ein = attrappe_eingang_beginn();
  dict_write_int16(ein, MESSAGE_KEY_TARGET, 10);
  dict_write_uint8(ein, MESSAGE_KEY_ANIMATION, 0);
  attrappe_eingang_zustellen();
  pruefe("TARGET als zwei Byte 10: Soll 10", schedule_target() == 10);
  pruefe("ANIMATION als ein Byte 0: aus", !schedule_animation());
}

static void abschnitt_nach_ruhe(void) {
  printf("\nNach fuenf Fehlversuchen (N3): ein neues Glas versucht es wieder\n");
  start(um(2026, 10, 3, 9, 0), true);
  phone_note_drink();
  phone_send_next();
  int runden = 0;
  while (attrappe_unterwegs() && runden < 20) {
    attrappe_nack(APP_MSG_SEND_TIMEOUT);
    attrappe_zeitgeber_ablaufen();
    runden++;
  }
  pruefe("erst 1 + 5 Sendungen, dann Ruhe", attrappe_gesendet() == 6 && attrappe_zeitgeber_offen() == 0);
  phone_note_drink();
  phone_send_next();
  pruefe("neues Glas: es geht hinaus", attrappe_gesendet() == 7);
  attrappe_nack(APP_MSG_SEND_TIMEOUT);
  pruefe("wieder abgelehnt: es wird nachgefasst", attrappe_zeitgeber_offen() == 1);
  attrappe_zeitgeber_ablaufen();
  pruefe("nachgefasst, mit dem aeltesten Glas", attrappe_gesendet() == 8 && hat(MESSAGE_KEY_DRANK_AT));
  attrappe_ack();
  while (attrappe_zeitgeber_offen()) { attrappe_zeitgeber_ablaufen(); attrappe_ack(); }
  pruefe("beide Glaeser sind beim Telefon", !phone_pending());
}

#ifdef TEXT_ZU_KLEIN
// Zweiter Bau mit -DCUSTOM_TEXT_MAX=60 (phone_host_test.sh): der Text der
// eigenen Getraenke passt nicht. Er darf dann NICHT abgeschnitten hinaus -
// das Telefon uebernaehme ihn -, sondern fehlt, und das steht im Log.
static void abschnitt_text_zu_klein(void) {
  printf("\nText passt nicht (Puffer 60 Byte): nichts Abgeschnittenes hinaus\n");
  start(um(2026, 10, 3, 9, 0), true);
  custom_from_string(GROESSTE);
  attrappe_log_leeren();
  phone_send_next();
  pruefe("CUSTOM fehlt", !hat(MESSAGE_KEY_CUSTOM));
  pruefe("steht im Log", im_log("Eigene Getraenke passen nicht in 60 Byte"));
  pruefe("der Rest der Meldung ist da", hat(MESSAGE_KEY_GOAL_DAY) && hat(MESSAGE_KEY_GLASS_ML));
}
static void (*const ABSCHNITTE[])(void) = { abschnitt_text_zu_klein };
#else
// Jeder Abschnitt laeuft in einem eigenen Prozess: so hat er frische
// statische Variablen wie die App bei jedem Start auf der Uhr.
static void (*const ABSCHNITTE[])(void) = {
  abschnitt_tag, abschnitt_vorlauf, abschnitt_vorlauf_groesster_fall, abschnitt_groesster_fall,
  abschnitt_text_grenze,
  abschnitt_schlange, abschnitt_passt_nicht, abschnitt_kaffee_passt_nicht, abschnitt_nicht_abgeschickt,
  abschnitt_namen, abschnitt_feldform, abschnitt_nach_ruhe,
};
#endif

int main(void) {
  int fehler = 0;
  for (unsigned i = 0; i < sizeof(ABSCHNITTE) / sizeof(ABSCHNITTE[0]); i++) {
    fflush(stdout);
    const pid_t kind = fork();
    if (kind == 0) {
      ABSCHNITTE[i]();
      fflush(stdout);
      _exit(s_fehler > 100 ? 100 : s_fehler);
    }
    int status = 0;
    waitpid(kind, &status, 0);
    fehler += WIFEXITED(status) ? WEXITSTATUS(status) : 1;
  }
  printf("%s\n", fehler ? "NICHT BESTANDEN" : "alles bestanden");
  return fehler ? 1 : 0;
}
