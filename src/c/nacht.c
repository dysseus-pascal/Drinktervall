#include "nacht.h"

// Der laengste Schlaf, der in den letzten 24 Stunden endete. Ein kurzes
// Nickerchen am Nachmittag ist keine Nacht; von mehreren Abschnitten zaehlt
// der laengste.
static Nacht s_nacht;

static bool prv_abschnitt(HealthActivity activity, time_t start, time_t end, void *context) {
  if (activity != HealthActivitySleep) return true;
  if (end - start > s_nacht.ende - s_nacht.beginn) {
    s_nacht.beginn = start;
    s_nacht.ende = end;
  }
  return true;
}

Nacht nacht_lesen(void) {
  const time_t jetzt = time(NULL);
  memset(&s_nacht, 0, sizeof(s_nacht));

  // MIT &, NICHT MIT ==: die Maske kann neben "verfuegbar" weitere Bits
  // tragen.
  if (health_service_metric_accessible(HealthMetricSleepSeconds, jetzt - 86400, jetzt)
      & HealthServiceAccessibilityMaskAvailable) {
    health_service_activities_iterate(HealthActivitySleep, jetzt - 86400, jetzt,
                                      HealthIterationDirectionPast, prv_abschnitt, NULL);
    if (s_nacht.ende > s_nacht.beginn) {
      const HealthValue erholsam =
          health_service_sum(HealthMetricSleepRestfulSeconds, s_nacht.beginn, s_nacht.ende);
      s_nacht.erholsam_s = erholsam > 0 ? (uint32_t)erholsam : 0;
    }
  }

  // DER RUHEPULS IST DER MITTLERE PULS DER NACHT. Einen eigenen Ruhepuls
  // kennt das SDK nicht (kein HealthMetricRestingHeartRateBPM); was die Uhr
  // im Schlaf misst, ist die naechstliegende Zahl dazu - und dieselbe, die
  // andere Uhren als Ruhepuls zeigen.
  if (s_nacht.ende > s_nacht.beginn &&
      (health_service_metric_averaged_accessible(HealthMetricHeartRateBPM, s_nacht.beginn,
                                                 s_nacht.ende, HealthServiceTimeScopeOnce)
       & HealthServiceAccessibilityMaskAvailable)) {
    const HealthValue puls = health_service_aggregate_averaged(
        HealthMetricHeartRateBPM, s_nacht.beginn, s_nacht.ende,
        HealthAggregationAvg, HealthServiceTimeScopeOnce);
    if (puls > 0 && puls < 250) s_nacht.ruhepuls = (uint16_t)puls;
  }
  return s_nacht;
}
