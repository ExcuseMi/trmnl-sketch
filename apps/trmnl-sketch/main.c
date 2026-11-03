/* Sketch for the TRMNL X: draw with a USB pen tablet (docs: README.md). An ELF app for the firmware fork's loader
   (app_api.h); everything it decides is in src/sketch.c, tested on the host. SPDX-License-Identifier: MIT */
#include <app_api.h>
#include <stdlib.h>
#include <string.h>
#include "app_orient.h"
#include "sk_cfg.h"
#include "skcfg.h"
#include "sketch.h"
#include "skpush.h"
#include "printjob.h"
#include "printer.h"
#include "icon.h"

APP_INFO("Sketch", 32 * 1024);
APP_ICON(SKETCH_ICON); /* a pencil drawing a curve, 64 x 64 (tools/icon.py) */

static const app_api_t *api;
static sketch_t sk;

/* ---- the file system and settings the controller uses ---- */

static void *f_open(void *c, const char *p, int w)
{
    (void)c;
    return api->file_open(p, w ? APP_FILE_WRITE : APP_FILE_READ);
}
static int f_read(void *c, void *f, void *b, int n) { return (void)c, api->file_read(f, b, (size_t)n); }
static int f_write(void *c, void *f, const void *b, int n) { return (void)c, api->file_write(f, b, (size_t)n); }
static int f_close(void *c, void *f) { return (void)c, api->file_close(f); }
static int f_rename(void *c, const char *a, const char *b) { return (void)c, api->file_rename(a, b); }
static int f_remove(void *c, const char *p) { return (void)c, api->file_remove(p); }
static int f_mkdir(void *c, const char *p) { return (void)c, api->file_mkdir(p); }

typedef struct {
    int (*cb)(const char *, void *);
    void *u;
} list_ctx_t;
static bool on_entry(const char *name, bool is_dir, uint32_t size, void *ctx)
{
    (void)size;
    list_ctx_t *l = ctx;
    return is_dir || l->cb(name, l->u);
}
static int f_list(void *c, const char *dir, int (*cb)(const char *, void *), void *u)
{
    (void)c;
    list_ctx_t l = { cb, u };
    return api->file_list(dir, on_entry, &l);
}
static const sk_fs_t fs = { 0, f_open, f_read, f_write, f_close, f_rename, f_remove, f_mkdir, f_list };

static int cfg_get(void *u, const char *k, char *b, int cap) { return (void)u, skcfg_get(api, k, b, cap); }
static int cfg_set(void *u, const char *k, const char *v, int n) { return (void)u, api->setting_set(k, v, (size_t)n); }

/* One IPP POST per print job (the same adaptor as the Print app's main.c) */
static int f_post(void *u, const char *url, const void *body, size_t n, uint8_t *resp, size_t cap, size_t *rn)
{
    (void)u;
    static const char *const hdr[] = { "Content-Type: application/ipp", 0 };
    app_http_request_t rq = { "POST", url, hdr, body, n, 30000 };
    app_http_response_t rs;
    memset(&rs, 0, sizeof(rs));
    *rn = 0;
    if (!api->net_up()) return PJ_NET_NO_WIFI;
    int rc = api->http_request(&rq, resp, cap, &rs);
    if (rc == APP_ENONET || rs.status == APP_ENONET) return PJ_NET_NO_WIFI;
    if (rc == APP_ETIMEDOUT || rs.status == APP_ETIMEDOUT) return PJ_NET_TIMEOUT;
    if (rs.status <= 0) return PJ_NET_UNREACHABLE;
    *rn = rs.body_len < cap ? rs.body_len : cap;
    return rs.status;
}

/* The default printer: the X's device setting (API 2.11) or the Print app's printer.cfg, one function */
static int f_printer(void *u, pj_printer_t *out) { return (void)u, pj_default_printer(api, out); }

/* The drawing latency, a line in the device log every 10 s while drawing (sklat.h) */
static void f_log(void *u, int ev_med, int ev_p95, int push_med, int push_p95, int n)
{
    (void)u;
    api->log("sketch lat: event to push med %d p95 %d ms, push call med %d p95 %d ms, n %d", ev_med, ev_p95, push_med, push_p95, n);
}

static int have_pen(void) { return api->size >= APP_API_SLOT(50); }

/* The tablet connected now (it may come after the app starts) */
static void read_tablet(void)
{
    app_pen_info_t pi;
    memset(&pi, 0, sizeof(pi));
    if (have_pen()) api->pen_info(&pi);
    sk_set_tablet(&sk, pi.width_um, pi.height_um, pi.vid != 0);
}

/* One sk_flush result to the panel: all partial rectangles as one call over their rows (src/skpush.h: a call costs the
   same however few rows), a flash half or the whole screen clean */
static void push(int n, const rect_t *out)
{
    sk_call_t c[SK_MAX_RECTS];
    int k = n ? skp_calls(n, out, SKP_MODE, api->size >= APP_API_SLOT(SKP_INK_SLOT), c) : 0;
    uint32_t t0 = api->millis();
    int ink = k > 0;
    for (int i = 0; i < k; i++) {
        api->screen_refresh(c[i].whole ? 0 : &c[i].r, c[i].how);
        if (c[i].whole || c[i].how == APP_REFRESH_FULL) ink = 0; /* a flash or a full refresh is not drawing */
    }
    if (ink) sk_lat_push(&sk, api->millis() - t0);
}

int app_main(const app_api_t *a)
{
    api = a;
    uint8_t *fb = api->screen_set_mode(APP_SCREEN_1BIT);
    if (!fb) return APP_ENOMEM;
    int fw = api->screen_width(), fh = api->screen_height(), stride = api->screen_stride();
    int orient = ao_start(api); /* follows the X's position: APP_EVENT_ORIENTATION */
    app_touchbar_t bar;
    ao_layout(api, orient, fw, fh, &bar);
    skcfg_declare(api);
    sk_env_t env = { &fs, 0, cfg_get, cfg_set, { PJ_PWG, f_post, 0 }, f_log, f_printer };
    int rc = sk_init(&sk, &env, fb, fw, fh, stride, orient, &bar);
    if (rc) return rc;
    if (api->size >= APP_API_SLOT(41)) api->touch_hold(1u << APP_TOUCH_MIDDLE);
    if (have_pen() && api->pen_enable(1) != 0) api->log("sketch: pen_enable failed");
    if (!have_pen()) api->log("sketch: firmware without pen support (API 2.7): touch bar only");
    read_tablet();
    int sharing = 0, share_rc = 0;
    uint32_t sec = 0, pen_check = api->millis();
    for (;;) {
        uint32_t now = api->millis();
        rect_t out[SK_MAX_RECTS];
        do push(sk_flush(&sk, api->millis(), out), out); while (sk_flash_busy(&sk)); /* the inverted half, then the clean one */
        if (sk.print_req) sk_run_print(&sk); /* "Printing..." is on the glass; a few rows a turn, the post blocks */
        sk_tick(&sk, now);
        if (sk.share_req > 0 && !sharing) {
            app_webshare_files_t opt = { SK_DIR, ".png", 4u * 1024 * 1024 };
            if (sk.share_req == 2) /* the settings form */
                share_rc = api->size >= APP_API_SLOT(47) ? api->webshare_start_settings(APP_WEBSHARE_AUTO) : APP_ENOTSUP;
            else
                share_rc = api->size >= APP_API_SLOT(37) ? api->webshare_start_files(APP_WEBSHARE_AUTO, &opt) : APP_ENOTSUP;
            sharing = share_rc == 0;
            if (share_rc) api->log("sketch: wi-fi page start %d", share_rc);
            sk.share_req = 0;
            sec = 0;
        } else if (sk.share_req < 0) {
            if (sharing) api->webshare_stop();
            sharing = 0;
            sk.share_req = 0;
        }
        if (sk.mode == SK_SHARE && now / 1000 != sec) { /* the code and the idle time move: read the status each second */
            sec = now / 1000;
            app_webshare_status_t ws;
            memset(&ws, 0, sizeof(ws));
            if (sharing) api->webshare_status(&ws);
            sk_share_update(&sk, share_rc, &ws);
        }
        if ((int32_t)(now - pen_check) >= 2000) { /* a tablet plugged in later */
            pen_check = now;
            read_tablet();
        }
        uint32_t wait = sk_wait_ms(&sk, now);
        if (wait > 500) wait = 500;
        if (sk.mode == SK_SHARE && wait > 250) wait = 250;
        app_any_event_t u;
        int exit_now = 0;
        uint32_t frame_end = 0;
        /* the first event ends the wait; the frame then takes every event of the next SK_FRAME_MS (skframe.h) */
        for (int k = 0; !exit_now; k++) {
            uint32_t w = wait;
            if (k) {
                int32_t left = (int32_t)(frame_end - api->millis());
                if (left <= 0) break;
                w = (uint32_t)left;
            }
            if (!api->input_poll(&u.ev, w)) break;
            now = api->millis();
            if (!k) frame_end = now + SK_FRAME_MS;
            switch (u.ev.type) {
            case APP_EVENT_EXIT: exit_now = 1; break;
            case APP_EVENT_PEN: sk_pen_tilt(&sk, u.pen.tilt_x, u.pen.tilt_y); sk_pen(&sk, u.pen.flags, u.pen.x, u.pen.y, u.pen.pressure, now); break;
            case APP_EVENT_PEN_KEY: sk_pen_key(&sk, u.pen.keys, u.pen.changed, now); break;
            case APP_EVENT_TOUCH: sk_touch(&sk, u.ev.zone, u.ev.gesture, now); break;
            case APP_EVENT_ORIENTATION:
                if (u.ev.key <= APP_ORIENT_BAR_RIGHT && (int)u.ev.key != orient) {
                    orient = (int)u.ev.key;
                    ao_layout(api, orient, fw, fh, &bar);
                    sk_set_orient(&sk, orient, &bar);
                }
                break;
            default: break;
            }
        }
        {
            uint32_t t0 = api->millis();
            sk_frame(&sk, t0); /* the stroke's points of this frame: thinned, stamped, one push */
            sk_frame_cost(&sk, api->millis() - t0);
        }
        if (exit_now || sk.exit_req) break;
        if (sk.setup_req) { /* "Set up printer": Print starts after this app returns, and hands back here with an empty argument */
            if (api->size >= APP_API_SLOT(55) && api->app_launch("trmnl-print", "setup:trmnl-sketch") == 0) break;
            sk.setup_req = 0; /* no such app or an older firmware: stay */
        }
    }
    if (sharing) api->webshare_stop();
    if (have_pen()) api->pen_enable(0);
    sk_save_now(&sk); /* within APP_EXIT_GRACE_MS */
    sk_free(&sk);
    return 0;
}
