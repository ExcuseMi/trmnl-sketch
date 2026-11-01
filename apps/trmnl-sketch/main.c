/* Sketch for the TRMNL X: draw with a USB pen tablet (docs: README.md). An ELF app for the firmware fork's loader
   (app_api.h); everything it decides is in src/sketch.c, tested on the host. SPDX-License-Identifier: MIT */
#include <app_api.h>
#include <stdlib.h>
#include <string.h>
#include "app_orient.h"
#include "sk_cfg.h"
#include "sketch.h"
#include "printjob.h"

APP_INFO("Sketch", 32 * 1024);

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

static int cfg_get(void *u, const char *k, char *b, int cap) { return (void)u, api->setting_get(k, b, (size_t)cap); }
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

static const app_setting_decl_t decl[] = {
    { "printer", "Printer address (IP, or IP:port; the port is 631 if left out)", 0, 0, 64, APP_SETTING_TEXT },
    { "cleanup_s", "Clean the screen after this many idle seconds (0: never)", 0, 0, 600, APP_SETTING_NUMBER },
    { "pen_area", "Tablet area on the page (fill: every tablet edge is a screen edge; keep: no stretching)", "fill|keep", 0, 0, APP_SETTING_CHOICE },
};

static int have_pen(void) { return api->size >= APP_API_SLOT(50); }

/* The tablet connected now (it may come after the app starts) */
static void read_tablet(void)
{
    app_pen_info_t pi;
    memset(&pi, 0, sizeof(pi));
    if (have_pen()) api->pen_info(&pi);
    sk_set_tablet(&sk, pi.width_um, pi.height_um, pi.vid != 0);
}

static void push(int n, const rect_t *out)
{
    if (n == SK_FULL) {
        api->screen_refresh(0, APP_REFRESH_FULL); /* a page or a menu screen opens or closes, clear: clean */
        return;
    }
    for (int i = 0; i < n; i++) {
        app_rect_t r = { out[i].x, out[i].y, out[i].w, out[i].h };
        /* ink and cursors: the fastest the API has, 1-bit mode; the toolbar's strip: clean, only that rectangle */
        api->screen_refresh(&r, out[i].flash ? APP_REFRESH_FULL : APP_REFRESH_PARTIAL);
    }
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
    if (api->size >= APP_API_SLOT(47)) api->settings_declare(decl, sizeof(decl) / sizeof(decl[0])); /* the printer's address, own setting: settings are per app */
    sk_env_t env = { &fs, 0, cfg_get, cfg_set, { PJ_PWG, f_post, 0 } };
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
        if (sk.print_req) sk_run_print(&sk); /* "Printing..." is on the glass; blocks while the job goes out */
        sk_tick(&sk, now);
        if (sk.share_req > 0 && !sharing) {
            app_webshare_files_t opt = { SK_DIR, ".png", 4u * 1024 * 1024 };
            if (sk.share_req == 2) /* the settings form: the printer's address */
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
        uint32_t wait = sk_next_due(&sk, now);
        if (wait > 500) wait = 500;
        if (sk.mode == SK_SHARE && wait > 250) wait = 250;
        app_any_event_t u;
        int exit_now = 0;
        for (int k = 0; k < 64 && !exit_now && api->input_poll(&u.ev, k ? 0 : wait); k++) {
            now = api->millis();
            switch (u.ev.type) {
            case APP_EVENT_EXIT: exit_now = 1; break;
            case APP_EVENT_PEN: sk_pen(&sk, u.pen.flags, u.pen.x, u.pen.y, u.pen.pressure, now); break;
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
        if (exit_now) break;
    }
    if (sharing) api->webshare_stop();
    if (have_pen()) api->pen_enable(0);
    sk_save_now(&sk); /* within APP_EXIT_GRACE_MS */
    sk_free(&sk);
    return 0;
}
