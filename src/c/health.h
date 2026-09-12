#ifndef FACEOFF_HEALTH_H
#define FACEOFF_HEALTH_H

#include <pebble.h>

// Aplite has no HealthService at all, and a heart rate monitor is not implied
// by having one: diorite covers both the Pebble 2 SE and the Pebble 2 HR, so
// the only honest test is at runtime, on the watch the face woke up on.
bool health_steps_available();
bool health_heart_rate_available();

// "8.2K", "834", or "--" when the metric is not there to read. Kept short
// because the column is only as wide as the corner beside the time.
void health_steps_string(char *buffer, size_t size);
void health_heart_rate_string(char *buffer, size_t size);

#endif
