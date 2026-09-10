#include "schedule.h"
#include "config.h"

#define MAX_WAKEUPS 8
#define LEAD_S      30   // Wakeups muessen etwas in der Zukunft liegen

static int s_count;

static int32_t prv_day_key(time_t t) {
  struct tm *lt = localtime(&t);
  return (lt->tm_year + 1900) * 10000 + (lt->tm_mon + 1) * 100 + lt->tm_mday;
}

void schedule_init(void) {
  time_t now = time(NULL);
  int32_t today = prv_day_key(now);
  int32_t stored_day = persist_exists(AT_PERSIST_DAY) ? persist_read_int(AT_PERSIST_DAY) : 0;
  s_count = (stored_day == today && persist_exists(AT_PERSIST_COUNT))
      ? persist_read_int(AT_PERSIST_COUNT) : 0;
  if (s_count < 0) s_count = 0;
  if (s_count > AT_GLASSES) s_count = AT_GLASSES;
  if (stored_day != today) {
    persist_write_int(AT_PERSIST_DAY, today);
    persist_write_int(AT_PERSIST_COUNT, 0);
  }
}

int schedule_count(void) {
  return s_count;
}

void schedule_set_count(int count) {
  if (count < 0) count = 0;
  if (count > AT_GLASSES) count = AT_GLASSES;
  s_count = count;
  persist_write_int(AT_PERSIST_DAY, prv_day_key(time(NULL)));
  persist_write_int(AT_PERSIST_COUNT, s_count);
}

// Ohne mktime: Mitternacht = jetzt minus Sekunden seit lokaler Mitternacht.
// Am Tag einer Sommerzeit-Umstellung kann das um eine Stunde abweichen.
time_t schedule_midnight(time_t t) {
  struct tm *lt = localtime(&t);
  return t - (lt->tm_hour * 3600 + lt->tm_min * 60 + lt->tm_sec);
}

time_t schedule_slot(time_t midnight, int idx) {
  return midnight + (time_t)(AT_START_HOUR * 60 + idx * AT_INTERVAL_MIN) * 60;
}

int schedule_next(time_t now, time_t *when) {
  time_t midnight = schedule_midnight(now);
  for (int day = 0; day < 2; day++) {
    for (int i = 0; i < AT_GLASSES; i++) {
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

// Bei E_RANGE (fremdes Wakeup in der Minute) um je 2 Minuten verschieben.
static bool prv_schedule(time_t t, int32_t cookie) {
  for (int attempt = 0; attempt < 3; attempt++) {
    WakeupId id = wakeup_schedule(t + attempt * 120, cookie, true);
    if (id >= 0) return true;
    if (id != E_RANGE) return false;
  }
  return false;
}

void schedule_plan_wakeups(time_t snooze_until) {
  time_t now = time(NULL);
  wakeup_cancel_all();
  int n = 0;
  if (snooze_until > now + LEAD_S && prv_schedule(snooze_until, SCHEDULE_COOKIE_SNOOZE)) n++;
#ifdef AT_TEST_WAKEUP
  // Nur fuer Emulator-Tests: Erinnerung eine Minute nach dem Start.
  if (prv_schedule(now + 60, 0)) n++;
#endif
  time_t midnight = schedule_midnight(now);
  for (int day = 0; day < 3 && n < MAX_WAKEUPS; day++) {
    for (int i = 0; i < AT_GLASSES && n < MAX_WAKEUPS; i++) {
      time_t t = schedule_slot(midnight + day * 86400, i);
      if (t <= now + LEAD_S) continue;
      if (prv_schedule(t, i)) n++;
    }
  }
}

void schedule_format_time(time_t t, char *buf, size_t len) {
  struct tm *lt = localtime(&t);
  strftime(buf, len, clock_is_24h_style() ? "%H:%M" : "%I:%M", lt);
  // 12-Stunden-Format ohne fuehrende Null
  if (!clock_is_24h_style() && buf[0] == '0') memmove(buf, buf + 1, strlen(buf));
}
