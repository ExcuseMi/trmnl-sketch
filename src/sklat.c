/* SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0 */
#include "sklat.h"

void lat_add(lat_t *l, uint32_t ms)
{
    l->v[l->head] = (uint16_t)(ms > 65535 ? 65535 : ms);
    l->head = (l->head + 1) % SK_LAT_N;
    if (l->n < SK_LAT_N) l->n++;
}

int lat_stats(const lat_t *l, int *med, int *p95)
{
    uint16_t s[SK_LAT_N];
    int n = l->n;
    for (int i = 0; i < n; i++) { /* insertion sort of a copy */
        uint16_t x = l->v[i];
        int j = i;
        while (j > 0 && s[j - 1] > x) s[j] = s[j - 1], j--;
        s[j] = x;
    }
    *med = n ? s[(n - 1) / 2 + (n % 2 == 0 ? 1 : 0)] : 0; /* the upper middle of an even count */
    *p95 = n ? s[(n * 95 + 99) / 100 - 1] : 0;            /* nearest rank */
    return n;
}

static char *put_num(char *d, char *end, int v)
{
    char t[12];
    int k = 0;
    do t[k++] = (char)('0' + v % 10), v /= 10; while (v && k < 11);
    while (k && d < end) *d++ = t[--k];
    return d;
}
static char *put_str(char *d, char *end, const char *s)
{
    while (*s && d < end) *d++ = *s++;
    return d;
}

void lat_text(char *buf, size_t cap, const lat_t *ev, const lat_t *push)
{
    if (!cap) return;
    char *d = buf, *end = buf + cap - 1;
    int m, p, n = lat_stats(ev, &m, &p);
    d = put_str(d, end, "ev>push ");
    d = put_num(d, end, m), d = put_str(d, end, "/"), d = put_num(d, end, p);
    lat_stats(push, &m, &p);
    d = put_str(d, end, " ms  push "), d = put_num(d, end, m), d = put_str(d, end, "/"), d = put_num(d, end, p);
    d = put_str(d, end, " ms  n "), d = put_num(d, end, n);
    *d = 0;
}
