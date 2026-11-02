/* The pressure curve (setting pressure_curve soft|normal|firm): applied to the pen's pressure before any brush uses it
   for width or tone. Normal is the identity, exactly: today's behaviour. SPDX-License-Identifier: MIT */
#ifndef SKPRESS_H
#define SKPRESS_H
#include <stddef.h>

enum { PC_SOFT, PC_NORMAL, PC_FIRM, PC_N };

/* The curved pressure 0..65535: soft y = x (3 - x) / 2 (a light touch counts for more), firm y = x (1 + x) / 2 (it takes a harder press) */
unsigned pc_apply(int curve, unsigned pressure);
const char *pc_name(int curve);
/* The curve named by the n bytes at s; PC_NORMAL when it is none of them */
int pc_parse(const char *s, int n);
/* The curves as a tiny preview for the settings text: what a touch of 25, 50 and 75 % counts as under each */
void pc_preview(char *buf, size_t cap);
#endif
