/* libc calls the X's loader does not export (its table: elf_loader's esp_elf_symbol.c): the compiler emits memmove for
   overlapping copies, and the X refused the app at start ("needs a function the firmware lacks", device 2026-10-31).
   apps/build.sh's import check is the guard; -fvisibility=hidden keeps these inside the app.
   SPDX-License-Identifier: MIT */
#include <stddef.h>

/* no loop-to-call rewrite here, or the body would call itself */
__attribute__((optimize("no-tree-loop-distribute-patterns"))) void *memmove(void *dst, const void *src, size_t n)
{
    unsigned char *d = dst;
    const unsigned char *s = src;
    if (d < s)
        while (n--) *d++ = *s++;
    else
        while (n--) d[n] = s[n];
    return dst;
}

/* memcmp is not in the loader's table either (found by the same import check, 2026-11-01) */
__attribute__((optimize("no-tree-loop-distribute-patterns"))) int memcmp(const void *a, const void *b, size_t n)
{
    const unsigned char *x = a, *y = b;
    for (; n; n--, x++, y++)
        if (*x != *y) return *x < *y ? -1 : 1;
    return 0;
}

/* memchr and snprintf are not in the loader's table either (import check 2026-11-01, found through the Print app's
   printer.c: pj_printer_parse). snprintf here knows only %s, %d and %%: all printer.c uses. */
#include <stdarg.h>
void *memchr(const void *s, int c, size_t n)
{
    const unsigned char *p = s;
    for (; n; n--, p++)
        if (*p == (unsigned char)c) return (void *)p;
    return 0;
}

int snprintf(char *out, size_t cap, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    size_t n = 0;
#define PUT(ch) do { if (n + 1 < cap) out[n] = (char)(ch); n++; } while (0)
    for (; *fmt; fmt++) {
        if (*fmt != '%') {
            PUT(*fmt);
            continue;
        }
        fmt++;
        if (*fmt == 's') {
            const char *s = va_arg(ap, const char *);
            for (; s && *s; s++) PUT(*s);
        } else if (*fmt == 'd') {
            int v = va_arg(ap, int);
            char d[12];
            int k = 0;
            unsigned u = v < 0 ? 0u - (unsigned)v : (unsigned)v;
            do d[k++] = (char)('0' + u % 10), u /= 10; while (u);
            if (v < 0) PUT('-');
            while (k) PUT(d[--k]);
        } else if (*fmt == '%') {
            PUT('%');
        } else {
            PUT('?'); /* a format this shim does not do */
        }
        if (!*fmt) break;
    }
#undef PUT
    va_end(ap);
    if (cap) out[n < cap ? n : cap - 1] = 0;
    return (int)n;
}
