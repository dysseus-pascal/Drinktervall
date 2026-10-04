#include "schedule.h"
#include "config.h"
#include "coffee.h"

#define MAX_WAKEUPS 8
#define LEAD_S      30   // Wakeups muessen etwas in der Zukunft liegen
// Cookie der "Spaeter"-Erinnerung; regulaere Slots tragen 0..schedule_target()-1,
// Kaffees SCHEDULE_COOKIE_COFFEE + Platz (siehe schedule.h)
#define COOKIE_SNOOZE 100

static int s_count;
static int s_target = DT_GLASSES_DEFAULT;
static int s_glass_ml = DT_GLASS_ML_DEFAULT;
static int s_goal = DT_GLASSES_DEFAULT;
static bool s_anim = DT_ANIM_DEFAULT;

static int32_t prv_day_key(time_t t) {
  struct tm *lt = localtime(&t);
  return (lt->tm_year + 1900) * 10000 + (lt->tm_mon + 1) * 100 + lt->tm_mday;
}

// Der Tag, zu dem Zaehler und Tagesziel gehoeren (JJJJMMTT). Geschrieben wird
// immer DIESER Tag, nicht der der Uhr - steht die Uhr kurz falsch, soll sie
// keinen falschen Tag in den Speicher tragen.
static int32_t s_day;

// Ein Datum davor hat die Uhr nicht: nach einem Neustart steht sie kurz so,
// bis das Telefon die Zeit stellt.
#define DT_TAG_PLAUSIBEL 20250101

// So weit darf die Uhr vom Telefon abweichen, damit ihre Zeit als bestaetigt
// gilt (siehe schedule_uhr_bestaetigt). Stellt das Telefon die Uhr, liegen
// beide Sekunden auseinander; eine Uhr, die nach einem Neustart auf einer
// alten Zeit steht, Minuten bis Stunden.
#define DT_UHR_ABWEICHUNG_MAX 300

// Tage seit 1970 aus JJJJMMTT - GENAU, nicht geschaetzt (days_from_civil nach
// Howard Hinnant). Bis 1.19 stand hier eine Schaetzung mit 31 Tagen je Monat:
// vom 1. Maerz auf den 28. Februar zurueck waren das 4 statt 1 Tag, und der
// Zweig "mehr als zwei Tage zurueck" griff bei einem kurzen Ruecksprung -
// nach dem Stellen der Uhr waren die Glaeser dann weg.
static int32_t prv_tagnummer(int32_t key) {
  int32_t y = key / 10000;
  const int32_t m = (key / 100) % 100, d = key % 100;
  if (m <= 2) y--;
  const int32_t era = (y >= 0 ? y : y - 399) / 400;
  const int32_t yoe = y - era * 400;
  const int32_t doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1;
  const int32_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + doe - 719468;
}

static int prv_clamp_target(int n) {
  if (n < DT_GLASSES_MIN) return DT_GLASSES_MIN;
  if (n > DT_GLASSES_MAX) return DT_GLASSES_MAX;
  return n;
}

// Ein neuer Tag: Zaehler auf 0, Ziel zurueck auf das Soll.
static void prv_neuer_tag(int32_t heute) {
  s_day = heute;
  s_count = 0;
  s_goal = s_target;
  persist_write_int(DT_PERSIST_DAY, s_day);
  persist_write_int(DT_PERSIST_COUNT, 0);
  persist_write_int(DT_PERSIST_GOAL, s_goal);
}

// NEU IST EIN TAG NUR NACH VORN - und das nicht nur beim Start. Bis 1.19
// wurde der Tag nur in schedule_init geprueft: stand die App ueber
// Mitternacht offen, zaehlte das erste Glas des neuen Tages zum alten.
//
// DIE UHR ALLEIN KANN NICHT ENTSCHEIDEN, WELCHE ZEIT FALSCH WAR. Steht sie
// hinter dem gemerkten Tag, liegt sie entweder jetzt zurueck (Neustart, siehe
// schedule_init) oder sie ging vorher vor und ist jetzt richtig (Audit M10:
// dann zaehlten die Glaeser des echten Tages zum vorausgeeilten Tag und
// standen am echten Folgetag noch da). Fuer die Uhr sehen beide Faelle gleich
// aus. Darum fragt sie in diesem Zustand das Telefon nach seiner Zeit
// (schedule_uhr_fraglich, phone.c) und gibt den gemerkten Tag erst auf, wenn
// das Telefon ihre Zeit bestaetigt (schedule_uhr_bestaetigt).
// NICHT bei einem Glas: wer trinkt, waehrend die Uhr nach einem Neustart auf
// gestern steht, zoege den Tag sonst auf gestern - und das Stellen der Uhr
// waere ein neuer Tag, alle Glaeser waeren weg.
static void prv_tag_pruefen(void) {
  const int32_t heute = prv_day_key(time(NULL));
  if (heute > s_day) prv_neuer_tag(heute);
}

bool schedule_uhr_fraglich(void) {
  const int32_t heute = prv_day_key(time(NULL));
  return heute < s_day && heute >= DT_TAG_PLAUSIBEL;
}

bool schedule_uhr_bestaetigt(uint32_t telefon) {
  const int32_t abweichung = (int32_t)(time(NULL) - (time_t)telefon);
  if (abweichung > DT_UHR_ABWEICHUNG_MAX || abweichung < -DT_UHR_ABWEICHUNG_MAX) {
    // Die Uhr steht (noch) falsch - genau der Neustartfall. Nichts aendern.
    APP_LOG(APP_LOG_LEVEL_INFO, "Telefonzeit weicht %d s ab - Tag bleibt", (int)abweichung);
    return false;
  }
  const int32_t heute = prv_day_key(time(NULL));
  // Kurz vor Mitternacht koennen Uhr und Telefon auf verschiedenen Tagen
  // stehen, obwohl sie nur Sekunden trennen. Dann lieber nichts.
  if (prv_day_key((time_t)telefon) != heute) return false;
  if (!(heute < s_day && heute >= DT_TAG_PLAUSIBEL)) return false;
  APP_LOG(APP_LOG_LEVEL_INFO, "Uhr vom Telefon bestaetigt: Tag %d statt %d, Glaeser bleiben",
          (int)heute, (int)s_day);
  s_day = heute;
  persist_write_int(DT_PERSIST_DAY, s_day);
  return true;
}

void schedule_init(void) {
  // Fehlt das Soll (Erstinstallation oder Stand vor 1.7.0), liest es als 0 und
  // prv_clamp_target zoege es auf DT_GLASSES_MIN - gewollt ist die
  // Voreinstellung.
  const int stored = persist_exists(DT_PERSIST_TARGET) ? persist_read_int(DT_PERSIST_TARGET)
                                                       : DT_GLASSES_DEFAULT;
  s_target = prv_clamp_target(stored);
  if (persist_exists(DT_PERSIST_GLASS)) {
    const int ml = persist_read_int(DT_PERSIST_GLASS);
    if (ml >= DT_GLASS_ML_MIN && ml <= DT_GLASS_ML_MAX) s_glass_ml = ml;
  }
  // Nur lesen, wenn der Schluessel wirklich da ist - sonst laese ein fehlender
  // Eintrag als false und schaltete die Animation ungefragt ab.
  s_anim = persist_exists(DT_PERSIST_ANIM) ? persist_read_bool(DT_PERSIST_ANIM)
                                           : DT_ANIM_DEFAULT;

  const int32_t today = prv_day_key(time(NULL));
  // Ein fehlender Schluessel liest als 0 und ist damit nie ein JJJJMMTT-Datum.
  const int32_t stored_day = persist_read_int(DT_PERSIST_DAY);
  // NEU IST EIN TAG NUR NACH VORN. Nach einem Firmware-Update oder Neustart
  // steht die Uhr kurz auf einer alten Zeit, bis das Telefon sie stellt.
  // Frueher galt jeder andere Tag als neuer: die Glaeser waren weg, und der
  // falsche Tag stand im Speicher (01.10.2026, wie bei SupCycle).
  if (stored_day == 0 || today > stored_day) {
    prv_neuer_tag(today);
    return;
  }
  s_day = stored_day;
  s_goal = persist_read_int(DT_PERSIST_GOAL);
  s_count = persist_read_int(DT_PERSIST_COUNT);
  if (s_goal < s_target) s_goal = s_target;
  if (s_goal > DT_GOAL_MAX) s_goal = DT_GOAL_MAX;
  if (s_count < 0) s_count = 0;
  if (s_count > s_goal) s_count = s_goal;
  // Liegt der gemerkte Tag dagegen weit voraus und geht die Uhr plausibel,
  // war der gemerkte Tag falsch (die Uhr stand einmal in der Zukunft) - dann
  // gilt heute, die Werte bleiben.
  if (stored_day > today && today >= DT_TAG_PLAUSIBEL &&
      prv_tagnummer(stored_day) - prv_tagnummer(today) > 2) {
    s_day = today;
    persist_write_int(DT_PERSIST_DAY, s_day);
  }
}

int schedule_target(void) {
  return s_target;
}

bool schedule_set_target(int target) {
  target = prv_clamp_target(target);
  if (target == s_target) return false;
  // Erst ein neuer Tag, falls einer begonnen hat: das neue Ziel gilt heute.
  prv_tag_pruefen();
  s_target = target;
  persist_write_int(DT_PERSIST_TARGET, s_target);

  // Ein neues Soll setzt das heutige Ziel zurueck - wer den Plan aendert,
  // meint den ganzen Tag. Schon getrunkene Glaeser gehen dabei nie verloren:
  // das Ziel faellt nie unter den Zaehler.
  s_goal = s_target > s_count ? s_target : s_count;
  if (s_goal > DT_GOAL_MAX) s_goal = DT_GOAL_MAX;
  persist_write_int(DT_PERSIST_DAY, s_day);
  persist_write_int(DT_PERSIST_GOAL, s_goal);
  return true;
}

int schedule_glass_ml(void) {
  return s_glass_ml;
}

bool schedule_set_glass_ml(int ml) {
  if (ml < DT_GLASS_ML_MIN || ml > DT_GLASS_ML_MAX) return false;
  if (ml == s_glass_ml) return false;
  s_glass_ml = ml;
  persist_write_int(DT_PERSIST_GLASS, s_glass_ml);
  return true;
}

bool schedule_animation(void) {
  return s_anim;
}

bool schedule_set_animation(bool on) {
  if (on == s_anim) return false;
  s_anim = on;
  persist_write_bool(DT_PERSIST_ANIM, s_anim);
  return true;
}

int schedule_count(void) {
  prv_tag_pruefen();
  return s_count;
}

void schedule_set_count(int count) {
  prv_tag_pruefen();
  if (count < 0) count = 0;
  if (count > s_goal) count = s_goal;
  s_count = count;
  persist_write_int(DT_PERSIST_DAY, s_day);
  persist_write_int(DT_PERSIST_COUNT, s_count);
}

int schedule_goal(void) {
  prv_tag_pruefen();
  return s_goal;
}

int32_t schedule_day(void) {
  prv_tag_pruefen();
  return s_day;
}

void schedule_raise_goal(void) {
  prv_tag_pruefen();
  if (s_goal >= DT_GOAL_MAX) return;
  s_goal++;
  persist_write_int(DT_PERSIST_DAY, s_day);
  persist_write_int(DT_PERSIST_GOAL, s_goal);
}

int schedule_interval_min(void) {
  return ((DT_END_HOUR - DT_START_HOUR) * 60) / s_target;
}

// Ohne mktime: Mitternacht = jetzt minus Sekunden seit lokaler Mitternacht.
// Am Tag einer Sommerzeit-Umstellung kann das um eine Stunde abweichen.
time_t schedule_midnight(time_t t) {
  struct tm *lt = localtime(&t);
  return t - (lt->tm_hour * 3600 + lt->tm_min * 60 + lt->tm_sec);
}

// Deterministischer Versatz in [-DT_JITTER_MIN, +DT_JITTER_MIN] Minuten aus Tag
// und Slot (Integer-Hash), damit Wakeups, Plan-Liste und Glance dieselben
// Zeiten zeigen. Der Faktor muss groesser sein als der groesste Slot-Index,
// sonst faellt der Versatz eines Tages mit dem eines Nachbartags zusammen.
static int prv_jitter_min(time_t midnight, int idx) {
  uint32_t h = (uint32_t)prv_day_key(midnight) * 32u + (uint32_t)idx;
  h ^= h >> 16; h *= 0x7feb352dU; h ^= h >> 15; h *= 0x846ca68bU; h ^= h >> 16;
  return (int)(h % (2 * DT_JITTER_MIN + 1)) - DT_JITTER_MIN;
}

time_t schedule_slot(time_t midnight, int idx) {
  int minutes = DT_START_HOUR * 60 + idx * schedule_interval_min() + prv_jitter_min(midnight, idx);
  if (minutes < DT_START_HOUR * 60) minutes = DT_START_HOUR * 60;
  if (minutes > DT_END_HOUR * 60) minutes = DT_END_HOUR * 60;
  return midnight + (time_t)minutes * 60;
}

int schedule_next(time_t now, time_t *when) {
  time_t midnight = schedule_midnight(now);
  for (int day = 0; day < 2; day++) {
    for (int i = 0; i < s_target; i++) {
      time_t t = schedule_slot(midnight + day * 86400, i);
      if (t > now) {
        if (when) *when = t;
        return i;
      }
    }
  }
  if (when) *when = schedule_slot(midnight + 86400, 0);
  return 0;
}

// Wakeups brauchen eine Minute Abstand zu jedem anderen geplanten Wakeup, auch
// zu unserem eigenen Snooze. Bei E_RANGE deshalb bis zu zweimal um je zwei
// Minuten nach hinten schieben; die Erinnerung kann so bis zu vier Minuten
// spaeter kommen.
static bool prv_schedule(time_t t, int32_t cookie) {
  for (int attempt = 0; attempt < 3; attempt++) {
    WakeupId id = wakeup_schedule(t + attempt * 120, cookie, true);
    if (id >= 0) return true;
    if (id != E_RANGE) return false;
  }
  return false;
}

// Ein geplanter Wecker: wann und wofuer.
typedef struct {
  time_t at;
  int32_t cookie;
} Termin;

// Die naechsten Termine aus Wasser und Kaffee, nach Zeit geordnet. Bis drei
// Tage voraus: mehr als acht Wecker kann die App ohnehin nicht stellen.
#define TERMINE_MAX (3 * (DT_GLASSES_MAX + DT_COFFEE_MAX + DT_CUSTOM_MAX))

static int prv_termine(time_t now, Termin *out) {
  int n = 0;
  const time_t midnight = schedule_midnight(now);
  for (int day = 0; day < 3; day++) {
    const time_t m = midnight + day * 86400;
    for (int i = 0; i < s_target; i++) {
      const time_t t = schedule_slot(m, i);
      if (t > now + LEAD_S) out[n++] = (Termin){ t, i };
    }
    for (int k = 0; k < coffee_count(); k++) {
      const time_t t = coffee_time(m, k);
      if (t > now + LEAD_S) out[n++] = (Termin){ t, SCHEDULE_COOKIE_COFFEE + k };
    }
    for (int k = 0; k < custom_count(); k++) {
      const time_t t = custom_time(m, k);
      if (t > now + LEAD_S) out[n++] = (Termin){ t, SCHEDULE_COOKIE_CUSTOM + k };
    }
  }
  // Einfuegesortierung: hoechstens sechzig Eintraege, und die Wasser-Slots
  // jedes Tages liegen schon in Reihe.
  for (int i = 1; i < n; i++) {
    const Termin x = out[i];
    int j = i - 1;
    while (j >= 0 && out[j].at > x.at) {
      out[j + 1] = out[j];
      j--;
    }
    out[j + 1] = x;
  }
  return n;
}

// Ein "Spaeter" ueberlebt das Neuplanen. Jeder Wecker plant alles neu, und
// mit Kaffee dazwischen kommt ein fremder Wecker oft vor dem eigenen Spaeter -
// ohne diesen Vermerk waere das Spaeter dann still verloren.
typedef struct __attribute__((packed)) {
  uint32_t at;
  int32_t cookie;
} Spaeter;

void schedule_plan_wakeups(time_t snooze_until, int32_t snooze_cookie) {
  time_t now = time(NULL);
  wakeup_cancel_all();
  if (snooze_until > 0) {
    const Spaeter sp = { (uint32_t)snooze_until, snooze_cookie };
    persist_write_data(DT_PERSIST_SNOOZE, &sp, sizeof(sp));
  } else if (persist_exists(DT_PERSIST_SNOOZE)) {
    Spaeter sp;
    if (persist_read_data(DT_PERSIST_SNOOZE, &sp, sizeof(sp)) == sizeof(sp) &&
        (time_t)sp.at > now + LEAD_S) {
      snooze_until = (time_t)sp.at;
      snooze_cookie = sp.cookie;
    } else {
      persist_delete(DT_PERSIST_SNOOZE);
    }
  }
  int n = 0;
  if (snooze_until > now + LEAD_S && prv_schedule(snooze_until, snooze_cookie)) n++;
#ifdef DT_TEST_WAKEUP
  // Nur fuer Emulator-Tests: Erinnerung eine Minute nach dem Start.
  if (prv_schedule(now + 60, 0)) n++;
#endif
  // Wasser und Kaffee teilen sich die acht Wecker der App: die naechsten acht
  // Termine, gleich welcher Art. Beim naechsten Start geht es weiter.
  static Termin termine[TERMINE_MAX];
  const int anzahl = prv_termine(now, termine);
  for (int i = 0; i < anzahl && n < MAX_WAKEUPS; i++) {
    if (prv_schedule(termine[i].at, termine[i].cookie)) n++;
  }
}

int32_t schedule_snooze_cookie(void) {
  return COOKIE_SNOOZE;
}

void schedule_format_time(time_t t, char *buf, size_t len) {
  const bool h24 = clock_is_24h_style();
  struct tm *lt = localtime(&t);
  strftime(buf, len, h24 ? "%H:%M" : "%I:%M", lt);
  // 12-Stunden-Format ohne fuehrende Null
  if (!h24 && buf[0] == '0') memmove(buf, buf + 1, strlen(buf));
}
