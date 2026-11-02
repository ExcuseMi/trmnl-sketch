/* The latency measurement of the drawing path (sklat.h) wired into the app: ink noted when drawn, timed when pushed.
   SPDX-License-Identifier: MIT */
#include "sketch.h"
#include "sklat.h"

void sk_lat_ink(sketch_t *s, uint32_t now)
{
    if (!s->ink_pend) s->ink_pend = 1, s->ink_t = now; /* the oldest ink not yet handed to the panel */
}

void sk_lat_issued(sketch_t *s, uint32_t now)
{
    if (!s->ink_pend) return;
    lat_add(&s->lat_ev, (uint32_t)(now - s->ink_t));
    s->ink_pend = 0, s->lat_new = 1;
}

void sk_lat_push(sketch_t *s, uint32_t ms) { lat_add(&s->lat_push, ms); }

static void lat_show(sketch_t *s, uint32_t now)
{
    char t[64];
    lat_text(t, sizeof(t), &s->lat_ev, &s->lat_push);
    sk_toast(s, t, now);
}

void sk_lat_tick(sketch_t *s, uint32_t now)
{
    if (s->lat_new && !s->lat_next) s->lat_next = now + SK_LAT_LOG_MS;
    if (!s->lat_next || (int32_t)(now - s->lat_next) < 0) return;
    if (!s->lat_new) { /* nothing was drawn in the last period */
        s->lat_next = 0;
        return;
    }
    int m, p, n = lat_stats(&s->lat_ev, &m, &p), pm, pp;
    lat_stats(&s->lat_push, &pm, &pp);
    if (s->env.log) s->env.log(s->env.user, m, p, pm, pp, n);
    if (s->dbg && !s->tip && s->mode == SK_DRAW) lat_show(s, now);
    s->lat_new = 0, s->lat_next = now + SK_LAT_LOG_MS;
}

void sk_lat_debug(sketch_t *s, uint32_t now)
{
    s->dbg = !s->dbg;
    if (s->dbg) lat_show(s, now);
    else sk_toast(s, "Debug off", now);
}
