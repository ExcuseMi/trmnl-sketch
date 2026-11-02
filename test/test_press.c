/* The pressure curve: normal is today's pixels exactly, soft and firm are monotonic and ordered. SPDX-License-Identifier: MIT */
#include "check.h"
#include <math.h>
#include <string.h>
#include "golden.h"
#include "skpress.h"

/* Hashes taken from the build before the curve existed (git e14e7fd), brush, size, tone */
static const struct { int brush, size, tone; unsigned hash; } gold[] = {
    { 0, 9, 0, 2225467022u }, { 0, 20, 3, 2724244762u }, { 1, 9, 0, 1024528740u }, { 1, 20, 3, 1975524280u },
    { 2, 9, 0, 2755467040u }, { 2, 20, 3, 601061113u }, { 3, 9, 0, 505489422u }, { 3, 20, 3, 1226336672u },
    { 4, 9, 0, 2749076364u }, { 4, 20, 3, 130346304u }, { 5, 9, 0, 2112490937u }, { 5, 20, 3, 1985846511u },
};

static void test_default_is_today(void)
{
    for (unsigned i = 0; i < sizeof(gold) / sizeof(gold[0]); i++) CHECK(golden_hash(gold[i].brush, gold[i].size, gold[i].tone) == gold[i].hash);
}

static void test_curves(void)
{
    unsigned prev[PC_N] = { 0, 0, 0 };
    for (unsigned p = 0; p <= 65535; p += 17) {
        CHECK(pc_apply(PC_NORMAL, p) == p); /* the identity, exactly */
        unsigned s = pc_apply(PC_SOFT, p), f = pc_apply(PC_FIRM, p);
        CHECK(s >= p && p >= f);                 /* soft counts a touch for more, firm for less */
        CHECK(s >= prev[PC_SOFT] && f >= prev[PC_FIRM]); /* more pressure never counts for less */
        CHECK(s <= 65535 && f <= 65535);
        prev[PC_SOFT] = s, prev[PC_FIRM] = f;
    }
    CHECK(pc_apply(PC_SOFT, 0) == 0 && pc_apply(PC_FIRM, 0) == 0);
    CHECK(pc_apply(PC_SOFT, 65535) == 65535 && pc_apply(PC_FIRM, 65535) == 65535);
    CHECK(pc_apply(PC_SOFT, 100000) == 65535 && pc_apply(99, 7) == 7); /* out of range: capped; an unknown curve is normal */
    CHECK(pc_parse("soft", 4) == PC_SOFT && pc_parse("firm", 4) == PC_FIRM && pc_parse("normal", 6) == PC_NORMAL);
    CHECK(pc_parse("so", 2) == PC_NORMAL && pc_parse("", 0) == PC_NORMAL && pc_parse("hard", 4) == PC_NORMAL); /* a bad value is the default */
    char t[160];
    pc_preview(t, sizeof(t));
    CHECK(strstr(t, "soft") && strstr(t, "normal 25/50/75") && strstr(t, "firm"));
    char tiny[8];
    pc_preview(tiny, sizeof(tiny)); /* a short buffer is cut, never overrun */
    CHECK(strlen(tiny) <= 7);
}

static int ink(int curve, int brush)
{
    static rig_t r;
    rig_new(&r, APP_ORIENT_BAR_BOTTOM);
    strcpy(r.cfg_pcurve, curve == PC_SOFT ? "soft" : curve == PC_FIRM ? "firm" : "normal");
    sk_cfg_reload(&r.s);
    CHECK(r.s.pcurve == curve);
    r.s.sty.brush = (uint8_t)brush;
    int xy[8] = { 300, 500, 500, 500, 700, 500, 900, 500 };
    rig_stroke(&r, xy, 4, 22000, 0);
    int n = 0;
    for (int i = 0; i < TS * TH; i++)
        for (int b = 0; b < 8; b++) n += !((r.s.page.px[i] >> b) & 1);
    rig_end(&r);
    return n;
}

/* through the app: a soft curve inks more than normal, firm less, for the brushes that read pressure; the setting is read from the form */
static void test_in_the_app(void)
{
    static const int brushes[] = { BR_FINELINER, BR_BRUSHPEN, BR_PENCIL, BR_SPRAY };
    for (unsigned k = 0; k < sizeof(brushes) / sizeof(brushes[0]); k++) {
        int s = ink(PC_SOFT, brushes[k]), n = ink(PC_NORMAL, brushes[k]), f = ink(PC_FIRM, brushes[k]);
        CHECK(s > n && n > f);
    }
}

int main(void)
{
    test_default_is_today();
    test_curves();
    test_in_the_app();
    return CHECK_DONE("test_press");
}
