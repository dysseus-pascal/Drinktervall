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

static int prv_clamp_target(int n) {
  if (n < DT_GLASSES_MIN) return DT_GLASSES_MIN;
  if (n > DT_GLASSES_MAX) return DT_GLASSES_MAX;
  return n;
}

void schedule_init(void) {
  const int32_t today = prv_day_key(time(NULL));
  // Ein fehlender Schluessel liest als 0 und ist damit nie ein JJJJMMTT-Datum.
  const bool same_day = persist_read_int(DT_PERSIST_DAY) == today;
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
  s_goal = same_day ? persist_read_int(DT_PERSIST_GOAL) : s_target;
  s_count = same_day ? persist_read_int(DT_PERSIST_COUNT) : 0;
  if (s_goal < s_target) s_goal = s_target;
  if (s_goal > DT_GOAL_MAX) s_goal = DT_GOAL_MAX;
  if (s_count < 0) s_count = 0;
  if (s_count > s_goal) s_count = s_goal;
  if (!same_day) {
    persist_write_int(DT_PERSIST_DAY, today);
    persist_write_int(DT_PERSIST_COUNT, 0);
    persist_write_int(DT_PERSIST_GOAL, s_target);
  }
}

int schedule_target(void) {
  return s_target;
}

bool schedule_set_target(int target) {
  target = prv_clamp_target(target);
  if (target == s_target) return false;
  s_target = target;
  persist_write_int(DT_PERSIST_TARGET, s_target);

  // Ein neues Soll setzt das heutige Ziel zurueck - wer den Plan aendert,
  // meint den ganzen Tag. Schon getrunkene Glaeser gehen dabei nie verloren:
  // das Ziel faellt nie unter den Zaehler.
  s_goal = s_target > s_count ? s_target : s_count;
  if (s_goal > DT_GOAL_MAX) s_goal = DT_GOAL_MAX;
  persist_write_int(DT_PERSIST_DAY, prv_day_key(time(NULL)));
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
  return s_count;
}

void schedule_set_count(int count) {
  if (count < 0) count = 0;
  if (count > s_goal) count = s_goal;
  s_count = count;
  persist_write_int(DT_PERSIST_DAY, prv_day_key(time(NULL)));
  persist_write_int(DT_PERSIST_COUNT, s_count);
}

int schedule_goal(void) {
  return s_goal;
}

void schedule_raise_goal(void) {
  if (s_goal >= DT_GOAL_MAX) return;
  s_goal++;
  persist_write_int(DT_PERSIST_DAY, prv_day_key(time(NULL)));
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
#define TERMINE_MAX (3 * (DT_GLASSES_MAX + DT_COFFEE_MAX))

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
