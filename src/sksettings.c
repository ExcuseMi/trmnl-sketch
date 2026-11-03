/* SPDX-License-Identifier: MIT */
#include "sksettings.h"
#include <string.h>
#include "paper.h"

static const char *const turn_v[] = { "Auto", "Off" }, *const area_v[] = { "Fill", "Keep" };
static const char *const paper_key[] = { "blank", "grid", "lines", "dots" };
static const int clean_v[] = { 0, 5, 10, 30, 60 };
#define NCLEAN 5

static void put(char *d, size_t cap, const char *s)
{
    size_t n = 0;
    while (s[n] && n + 1 < cap) d[n] = s[n], n++;
    d[n] = 0;
}

static void num(char *d, size_t cap, int v)
{
    char t[8];
    int k = 0;
    do t[k++] = (char)('0' + v % 10), v /= 10; while (v && k < 7);
    size_t n = 0;
    while (k && n + 1 < cap) d[n++] = t[--k];
    d[n] = 0;
}

void sks_row(const sketch_t *s, int i, char name[32], char val[16])
{
    switch (i) {
    case 0: put(name, 32, "Paper for new pages"), put(val, 16, pp_name(s->paper_def)); break;
    case 1: put(name, 32, "Paper spacing"), num(val, 16, s->paper_sp_def); break;
    case 2: put(name, 32, "Fill gap"), num(val, 16, s->bk_gap); break;
    case 3: put(name, 32, "Tablet turn"), put(val, 16, turn_v[s->turn ? 0 : 1]); break;
    case 4: put(name, 32, "Tablet area"), put(val, 16, area_v[s->fill ? 0 : 1]); break;
    default:
        put(name, 32, "Clean-up after idle");
        if (s->cleanup_s <= 0) put(val, 16, "Off");
        else num(val, 16, s->cleanup_s), put(val + strlen(val), 16 - strlen(val), " s");
    }
}

/* The next (dir 1) or previous value of a sorted list, wrapping; any current value, even one the list lacks, lands on a neighbour */
static int list_step(const int *v, int n, int cur, int dir)
{
    int k = -1;
    if (dir > 0) {
        for (int j = 0; j < n && k < 0; j++)
            if (v[j] > cur) k = j;
        return v[k < 0 ? 0 : k];
    }
    for (int j = n - 1; j >= 0 && k < 0; j--)
        if (v[j] < cur) k = j;
    return v[k < 0 ? n - 1 : k];
}

static int save(sketch_t *s, const char *key, const char *val)
{
    return s->env.cfg_set ? s->env.cfg_set(s->env.user, key, val, (int)strlen(val)) : -1;
}

int sks_step(sketch_t *s, int i, int dir)
{
    char v[16];
    const char *key;
    dir = dir < 0 ? -1 : 1;
    switch (i) {
    case 0:
        s->paper_def = (s->paper_def + PP_N + dir) % PP_N;
        key = "paper", put(v, 16, paper_key[s->paper_def]);
        break;
    case 1: {
        int sp = pp_spacing(s->paper_sp_def);
        sp = dir > 0 ? (sp / 8 + 1) * 8 : ((sp - 1) / 8) * 8;
        s->paper_sp_def = sp > PP_SP_MAX ? PP_SP_MIN : sp < PP_SP_MIN ? PP_SP_MAX : sp;
        key = "paper_sp", num(v, 16, s->paper_sp_def);
        break;
    }
    case 2:
        s->bk_gap = (s->bk_gap + 7 + dir) % 7;
        key = "fill_gap", num(v, 16, s->bk_gap);
        break;
    case 3:
        s->turn = !s->turn;
        key = "tablet_turn", put(v, 16, s->turn ? "auto" : "off");
        break;
    case 4:
        s->fill = !s->fill;
        key = "pen_area", put(v, 16, s->fill ? "fill" : "keep");
        break;
    default:
        s->cleanup_s = list_step(clean_v, NCLEAN, s->cleanup_s, dir);
        key = "cleanup_s", num(v, 16, s->cleanup_s);
    }
    if (i == 3 || i == 4) pm_init(&s->map, s->orient, s->fb.w, s->fb.h, s->um_w, s->um_h, s->fill, s->turn);
    return save(s, key, v) == 0 ? 0 : -1;
}
