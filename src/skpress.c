/* SPDX-License-Identifier: MIT */
#include "skpress.h"
#include <string.h>

static const char *const names[PC_N] = { "soft", "normal", "firm" };

const char *pc_name(int c) { return names[c < 0 || c >= PC_N ? PC_NORMAL : c]; }

int pc_parse(const char *s, int n)
{
    for (int c = 0; c < PC_N; c++)
        if ((size_t)n == strlen(names[c]) && !memcmp(s, names[c], (size_t)n)) return c;
    return PC_NORMAL;
}

unsigned pc_apply(int curve, unsigned p)
{
    if (p > 65535) p = 65535;
    if (curve == PC_NORMAL || curve < 0 || curve >= PC_N) return p;
    unsigned long long x = p, one = 65535;
    unsigned long long y = curve == PC_SOFT ? x * (3 * one - x) / (2 * one) : x * (one + x) / (2 * one);
    return (unsigned)(y > one ? one : y);
}

static char *put(char *d, char *end, const char *s)
{
    while (*s && d < end) *d++ = *s++;
    return d;
}

void pc_preview(char *buf, size_t cap)
{
    if (!cap) return;
    char *d = buf, *end = buf + cap - 1;
    d = put(d, end, "a touch of 25, 50, 75 % counts as");
    for (int c = 0; c < PC_N; c++) {
        d = put(d, end, c ? "; " : ": ");
        d = put(d, end, names[c]);
        for (int k = 1; k <= 3; k++) {
            unsigned v = pc_apply(c, 65535u * (unsigned)k / 4 + 1) * 100 / 65535;
            char t[5] = { ' ', (char)('0' + v / 10), (char)('0' + v % 10), 0, 0 };
            if (v >= 100) t[1] = '9', t[2] = '9';
            d = put(d, end, k > 1 ? "/" : " ");
            d = put(d, end, t + 1);
        }
    }
    *d = 0;
}
