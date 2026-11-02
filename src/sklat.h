/* Drawing latency, measured on the device: rolling statistics of "pen event to push issued" and "push duration". The
   last SK_LAT_N samples of each; median and p95 on demand. Pure: the app feeds it milliseconds. SPDX-License-Identifier: MIT */
#ifndef SKLAT_H
#define SKLAT_H
#include <stddef.h>
#include <stdint.h>

#define SK_LAT_N 128
#define SK_LAT_LOG_MS 10000 /* while drawing, a line in the log this often */

typedef struct {
    uint16_t v[SK_LAT_N];
    int n, head; /* samples held (at most SK_LAT_N), next slot */
} lat_t;

void lat_add(lat_t *l, uint32_t ms); /* capped at 65535 */
/* The median and the 95th percentile (nearest rank) of the samples held; 0 0 when none. Returns how many samples */
int lat_stats(const lat_t *l, int *med, int *p95);
/* "ev>push 40/78 ms  push 3/9 ms  n 87": med/p95 of both, the event count; cut to cap */
void lat_text(char *buf, size_t cap, const lat_t *ev, const lat_t *push);
#endif
