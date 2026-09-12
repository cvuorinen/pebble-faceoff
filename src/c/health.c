#include "health.h"

#if defined(PBL_HEALTH)
static bool prv_accessible(HealthMetric metric) {
  time_t now = time(NULL);

  return health_service_metric_accessible(metric, now, now) &
         HealthServiceAccessibilityMaskAvailable;
}
#endif

bool health_steps_available() {
#if defined(PBL_HEALTH)
  time_t now = time(NULL);

  return health_service_metric_accessible(HealthMetricStepCount,
                                          time_start_of_today(), now) &
         HealthServiceAccessibilityMaskAvailable;
#else
  return false;
#endif
}

bool health_heart_rate_available() {
#if defined(PBL_HEALTH)
  return prv_accessible(HealthMetricHeartRateBPM);
#else
  return false;
#endif
}

void health_steps_string(char *buffer, size_t size) {
#if defined(PBL_HEALTH)
  if (!health_steps_available()) {
    snprintf(buffer, size, "--");
    return;
  }

  int steps = health_service_sum_today(HealthMetricStepCount);
  // Four digits will not fit the column, so anything in the thousands is set
  // in them: 8.2K up to ten thousand, then 12K, never more than four glyphs.
  if (steps < 1000) {
    snprintf(buffer, size, "%d", steps);
  } else if (steps < 10000) {
    snprintf(buffer, size, "%d.%dK", steps / 1000, (steps % 1000) / 100);
  } else {
    snprintf(buffer, size, "%dK", steps / 1000);
  }
#else
  snprintf(buffer, size, "--");
#endif
}

void health_heart_rate_string(char *buffer, size_t size) {
#if defined(PBL_HEALTH)
  // Whatever the watch last sampled of its own accord. The face deliberately
  // does not raise the sampling rate: that is the wearer's battery, and a
  // number on a watchface is not worth spending it on.
  int bpm = health_service_peek_current_value(HealthMetricHeartRateBPM);
  // The metric can read as accessible while the monitor has yet to produce a
  // sample, and zero is not a heart rate anybody wants to be shown.
  if (!health_heart_rate_available() || bpm <= 0) {
    snprintf(buffer, size, "--");
    return;
  }

  snprintf(buffer, size, "%d", bpm);
#else
  snprintf(buffer, size, "--");
#endif
}
