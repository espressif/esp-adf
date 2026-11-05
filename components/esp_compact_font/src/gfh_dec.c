/**
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include "gfh_dec.h"

#include <string.h>

#define GFH_REDIRECT  0x80000000u

/* Same coverage as (v * 255 + maxv / 2) / maxv. A table avoids that divide. */
static const uint8_t gfh_opa1[2]  = {0, 255};
static const uint8_t gfh_opa2[4]  = {0, 85, 170, 255};
static const uint8_t gfh_opa4[16] = {
    0, 17, 34, 51, 68, 85, 102, 119,
    136, 153, 170, 187, 204, 221, 238, 255};

static int cmp_sym(const void *a, const void *b, const uint8_t *lens)
{
    int sa = *(const int *)a;
    int sb = *(const int *)b;
    if (lens[sa] != lens[sb]) {
        return lens[sa] < lens[sb] ? -1 : 1;
    }
    return (sa > sb) - (sa < sb);
}

/* Insertion sort: 256 symbols, ordered by (length, symbol). */
static int order_syms(const uint8_t *lens, int nsym, int *order)
{
    int n = 0;
    for (int s = 0; s < nsym; s++) {
        if (!lens[s]) {
            continue;
        }
        int j = n;
        while (j > 0 && cmp_sym(&order[j - 1], &s, lens) > 0) {
            order[j] = order[j - 1];
            j--;
        }
        order[j] = s;
        n++;
    }
    return n;
}

int gfh_plan(const uint8_t *lens, int nsym, gfh_plan_t *out, int *scratch)
{
    int maxlen = 0;
    int n = 0;
    int *extra;
    if (!lens || !out || !scratch || nsym <= 0 || nsym > GFH_NSYM) {
        return -1;
    }
    for (int s = 0; s < nsym; s++) {
        if (!lens[s]) {
            continue;
        }
        if (lens[s] > 20) {
            return -1;
        }
        n++;
        if (lens[s] > maxlen) {
            maxlen = lens[s];
        }
    }
    if (n == 0 || maxlen <= 0 || maxlen > 20) {
        return -1;
    }
    {
        uint32_t kraft = 0;
        for (int s = 0; s < nsym; s++) {
            if (lens[s]) {
                kraft += 1u << (maxlen - lens[s]);
            }
        }
        if (kraft > (1u << maxlen)) {
            return -1;
        }
    }
    memset(out, 0, sizeof *out);
    out->maxlen = maxlen;
    if (maxlen <= GFH_PEEK12) {
        out->peek = maxlen;
        out->tab_n = 1 << maxlen;
        out->sec_n = 0;
        return 0;
    }
    /* 12-bit primary. Longer codes hang off a secondary block per prefix. */
    extra = scratch;
    int order[GFH_NSYM];
    int count = order_syms(lens, nsym, order);
    unsigned code = 0;
    int prev = 0;
    for (int i = 0; i < (1 << GFH_PEEK12); i++) {
        extra[i] = -1;
    }
    for (int i = 0; i < count; i++) {
        int L = lens[order[i]];
        code <<= (L - prev);
        if (L > GFH_PEEK12) {
            unsigned prefix = code >> (L - GFH_PEEK12);
            int e = L - GFH_PEEK12;
            if ((int)prefix < (1 << GFH_PEEK12) && e > extra[prefix]) {
                extra[prefix] = e;
            }
        }
        code++;
        prev = L;
    }
    int sec = 0;
    for (int i = 0; i < (1 << GFH_PEEK12); i++) {
        if (extra[i] < 0) {
            continue;
        }
        sec += 1 << extra[i];
    }
    out->peek = GFH_PEEK12;
    out->tab_n = 1 << GFH_PEEK12;
    out->sec_n = sec;
    return 0;
}

int gfh_build(const gfh_plan_t *plan, uint32_t *tab, uint32_t *sec,
              const uint8_t *lens, int nsym, int *scratch)
{
    int order[GFH_NSYM];
    int count;
    unsigned code = 0;
    int prev = 0;
    int sec_used = 0;
    int *extra_of;
    int *base_of;

    if (!plan || !tab || !lens || !scratch || plan->tab_n <= 0) {
        return -1;
    }
    if (plan->sec_n && !sec) {
        return -1;
    }
    extra_of = scratch;
    base_of = scratch + (1 << GFH_PEEK12);
    count = order_syms(lens, nsym, order);
    if (count <= 0) {
        return -1;
    }
    memset(tab, 0, (size_t)plan->tab_n * sizeof *tab);
    if (sec && plan->sec_n) {
        memset(sec, 0, (size_t)plan->sec_n * sizeof *sec);
    }
    for (int i = 0; i < (1 << GFH_PEEK12); i++) {
        extra_of[i] = -1;
        base_of[i] = -1;
    }

    if (plan->sec_n == 0) {
        for (int i = 0; i < count; i++) {
            int s = order[i];
            int L = lens[s];
            int step, base;
            if (L <= 0 || L > plan->peek) {
                return -1;
            }
            code <<= (L - prev);
            if (code >= (1u << L)) {
                return -1;
            }
            step = 1 << (plan->peek - L);
            base = (int)(code << (plan->peek - L));
            if (base < 0 || base + step > plan->tab_n) {
                return -1;
            }
            for (int k = 0; k < step; k++) {
                tab[base + k] = ((uint32_t)s << 8) | (uint32_t)L;
            }
            code++;
            prev = L;
        }
        return 0;
    }

    for (int i = 0; i < count; i++) {
        int L = lens[order[i]];
        if (L <= 0 || L > 20) {
            return -1;
        }
        code <<= (L - prev);
        if (code >= (1u << L)) {
            return -1;
        }
        if (L > GFH_PEEK12) {
            unsigned prefix = code >> (L - GFH_PEEK12);
            int e = L - GFH_PEEK12;
            if (prefix >= (1u << GFH_PEEK12)) {
                return -1;
            }
            if (e > extra_of[prefix]) {
                extra_of[prefix] = e;
            }
        }
        code++;
        prev = L;
    }
    for (int p = 0; p < (1 << GFH_PEEK12); p++) {
        if (extra_of[p] < 0) {
            continue;
        }
        base_of[p] = sec_used;
        sec_used += 1 << extra_of[p];
        tab[p] = GFH_REDIRECT | ((uint32_t)base_of[p] << 8) | (uint32_t)extra_of[p];
    }
    if (sec_used != plan->sec_n) {
        return -1;
    }

    code = 0;
    prev = 0;
    for (int i = 0; i < count; i++) {
        int s = order[i];
        int L = lens[s];
        code <<= (L - prev);
        if (code >= (1u << L)) {
            return -1;
        }
        if (L <= GFH_PEEK12) {
            int step = 1 << (GFH_PEEK12 - L);
            int base = (int)(code << (GFH_PEEK12 - L));
            if (base < 0 || base + step > plan->tab_n) {
                return -1;
            }
            for (int k = 0; k < step; k++) {
                if (tab[base + k] & GFH_REDIRECT) {
                    return -1;
                }
                tab[base + k] = ((uint32_t)s << 8) | (uint32_t)L;
            }
        } else {
            unsigned prefix = code >> (L - GFH_PEEK12);
            int extra;
            int rlen = L - GFH_PEEK12;
            unsigned rest = code & ((1u << rlen) - 1u);
            int step;
            int base;
            if (prefix >= (1u << GFH_PEEK12)) {
                return -1;
            }
            extra = extra_of[prefix];
            if (extra < rlen || base_of[prefix] < 0) {
                return -1;
            }
            step = 1 << (extra - rlen);
            base = base_of[prefix] + (int)(rest << (extra - rlen));
            if (base < 0 || base + step > plan->sec_n) {
                return -1;
            }
            for (int k = 0; k < step; k++) {
                sec[base + k] = ((uint32_t)s << 8) | (uint32_t)L;
            }
        }
        code++;
        prev = L;
    }
    return 0;
}

/* 32-bit accumulator. A 64-bit one is a helper call per symbol on this core,
 * and 2-bit and 4-bit glyphs have two and four times as many symbols. */
static inline int gfh_symbol(const uint32_t *tab, const uint32_t *sec, int sec_n, int peek,
                             uint32_t mask, uint32_t *acc, int *nb,
                             const uint8_t **pp, const uint8_t *end, uint32_t *sym)
{
    const uint8_t *p = *pp;
    uint32_t a = *acc;
    uint32_t look, e;
    int n = *nb;
    int L;

    while (n <= 24 && p < end) {
        a = (a << 8) | *p++;
        n += 8;
    }
    if (n >= peek) {
        look = (a >> (n - peek)) & mask;
    }
    else if (n > 0) {
        look = (a << (peek - n)) & mask;
    } else {
        return -1;
    }
    e = tab[look];
    if (e & GFH_REDIRECT) {
        int extra = (int)(e & 255u);
        uint32_t base = (e >> 8) & 0x7fffffu;
        int want = peek + extra;
        uint32_t wide, rest;

        if (!sec || extra <= 0 || extra > 16) {
            return -1;
        }
        while (n < want && p < end) {
            a = (a << 8) | *p++;
            n += 8;
        }
        if (n >= want) {
            wide = (a >> (n - want)) & ((1u << want) - 1u);
        }
        else if (n > 0) {
            wide = (a << (want - n)) & ((1u << want) - 1u);
        } else {
            return -1;
        }
        rest = wide & ((1u << extra) - 1u);
        if (base + rest >= (uint32_t)sec_n) {
            return -1;
        }
        e = sec[base + rest];
    }
    L = (int)(e & 255u);
    if (L <= 0 || n < L) {
        return -1;
    }
    *sym = e >> 8;
    *acc = a;
    *nb = n - L;
    *pp = p;
    return 0;
}

/* bpp is a constant at each call site, so the pixel shift unrolls. */
static inline int __attribute__((always_inline))
gfh_run(const uint32_t *tab, const uint32_t *sec, int sec_n, int peek,
        const uint8_t *src, int src_n, int w, int h, int bpp, uint8_t *dst)
{
    const int span = 8 / bpp;
    const unsigned pix_mask = (1u << bpp) - 1u;
    const uint8_t *opa = bpp == 1 ? gfh_opa1 : bpp == 2 ? gfh_opa2
                                                        : gfh_opa4;
    const uint8_t *p = src;
    const uint8_t *end = src + src_n;
    uint32_t acc = 0;
    uint32_t mask = (1u << peek) - 1u;
    uint8_t prev[192];
    uint8_t *row = dst;
    int nb = 0;
    int x = 0;
    int left = w * h;

    if (w > (int)sizeof prev) {
        return -1;
    }
    memset(prev, 0, (size_t)w);
    while (left > 0) {
        uint32_t sym;
        int n;

        if (gfh_symbol(tab, sec, sec_n, peek, mask, &acc, &nb, &p, end, &sym) != 0) {
            return -1;
        }
        n = span < left ? span : left;
        if (x + n <= w) {
            int k;
            for (k = 0; k < span && k < n; k++) {
                unsigned idx = (sym >> (k * bpp)) & pix_mask;
                idx ^= prev[x + k];
                prev[x + k] = (uint8_t)idx;
                row[x + k] = opa[idx];
            }
            x += n;
            left -= n;
            if (x == w) {
                x = 0;
                row += w;
            }
        } else {
            int k;
            for (k = 0; k < n; k++) {
                unsigned idx = (sym >> (k * bpp)) & pix_mask;
                idx ^= prev[x];
                prev[x] = (uint8_t)idx;
                row[x] = opa[idx];
                if (++x == w) {
                    x = 0;
                    row += w;
                }
            }
            left -= n;
        }
    }
    return 0;
}

int gfh_decode(const uint32_t *tab, const uint32_t *sec, int sec_n, int peek,
               const uint8_t *src, int src_n, int w, int h, int bpp, uint8_t *dst)
{
    if (!tab || !src || !dst || w <= 0 || h <= 0 || peek <= 0 || peek > 20) {
        return -1;
    }
    if (bpp == 1) {
        return gfh_run(tab, sec, sec_n, peek, src, src_n, w, h, 1, dst);
    }
    if (bpp == 2) {
        return gfh_run(tab, sec, sec_n, peek, src, src_n, w, h, 2, dst);
    }
    if (bpp == 4) {
        return gfh_run(tab, sec, sec_n, peek, src, src_n, w, h, 4, dst);
    }
    return -1;
}

#ifdef GFH_DEC_TEST
#include <stdio.h>
#include <stdlib.h>

static int load_file(const char *path, uint8_t **out, size_t *n)
{
    FILE *f = fopen(path, "rb");
    long sz;
    if (!f) {
        return -1;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return -1;
    }
    sz = ftell(f);
    rewind(f);
    *out = malloc((size_t)sz);
    if (!*out || fread(*out, 1, (size_t)sz, f) != (size_t)sz) {
        fclose(f);
        return -1;
    }
    fclose(f);
    *n = (size_t)sz;
    return 0;
}

int main(int argc, char **argv)
{
    uint8_t *file = NULL, *expect = NULL;
    size_t flen = 0, elen = 0;
    uint32_t magic, count, index_off, lens_off, lens_len, data_off;
    uint16_t flags;
    int bpp;
    int entry;
    gfh_plan_t plan;
    uint32_t *tab, *sec;
    int *scratch;
    const uint8_t *lens;
    const uint8_t *exp;
    int bad = 0;

    if (argc != 3) {
        fprintf(stderr, "usage: gfh_test font.gfh expect.bin\n");
        return 2;
    }
    if (load_file(argv[1], &file, &flen) || load_file(argv[2], &expect, &elen)) {
        fprintf(stderr, "read failed\n");
        return 2;
    }
    if (flen < 28) {
        return 2;
    }
    memcpy(&magic, file, 4);
    memcpy(&flags, file + 6, 2);
    memcpy(&count, file + 8, 4);
    memcpy(&index_off, file + 12, 4);
    memcpy(&lens_off, file + 16, 4);
    memcpy(&lens_len, file + 20, 4);
    memcpy(&data_off, file + 24, 4);
    if (magic != 0x31484647u || lens_len != GFH_NSYM) {
        return 2;
    }
    bpp = (flags >> 8) & 0xF;
    if (bpp == 0 && (flags & 1)) {
        bpp = 2;
    }
    entry = (flags & 2) ? 16 : 20;
    if (bpp != 1 && bpp != 2 && bpp != 4) {
        return 2;
    }
    lens = file + lens_off;
    scratch = calloc(GFH_SCRATCH_INTS, sizeof(int));
    if (!scratch || gfh_plan(lens, GFH_NSYM, &plan, scratch) != 0) {
        return 2;
    }
    tab = calloc((size_t)plan.tab_n, 4);
    sec = plan.sec_n ? calloc((size_t)plan.sec_n, 4) : NULL;
    if (gfh_build(&plan, tab, sec, lens, GFH_NSYM, scratch) != 0) {
        return 2;
    }
    exp = expect;
    for (uint32_t i = 0; i < count; i++) {
        const uint8_t *ent = file + index_off + (size_t)i * (size_t)entry;
        int w, h;
        uint16_t n;
        uint32_t off, cp;
        uint8_t *dst;
        memcpy(&cp, ent, 4);
        if (entry == 16) {
            int8_t sw, sh;
            memcpy(&off, ent + 4, 4);
            memcpy(&n, ent + 8, 2);
            memcpy(&sw, ent + 13, 1);
            memcpy(&sh, ent + 14, 1);
            w = sw;
            h = sh;
        } else {
            uint16_t sw, sh;
            memcpy(&sw, ent + 10, 2);
            memcpy(&sh, ent + 12, 2);
            memcpy(&off, ent + 14, 4);
            memcpy(&n, ent + 18, 2);
            w = sw;
            h = sh;
        }
        if (w == 0 || h == 0) {
            continue;
        }
        dst = malloc((size_t)w * (size_t)h);
        if (gfh_decode(tab, sec, plan.sec_n, plan.peek,
                       file + data_off + off, n, w, h, bpp, dst) != 0) {
            fprintf(stderr, "decode fail U+%04X\n", cp);
            bad++;
            free(dst);
            break;
        }
        if (exp + (size_t)w * h > expect + elen ||
            memcmp(dst, exp, (size_t)w * h) != 0) {
            fprintf(stderr, "mismatch U+%04X %dx%d\n", cp, w, h);
            bad++;
            free(dst);
            break;
        }
        exp += (size_t)w * h;
        free(dst);
    }
    printf("gfh %s glyphs %u peek %d table %d sec %d bad %d\n",
           argv[1], count, plan.peek, plan.tab_n, plan.sec_n, bad);
    return bad ? 1 : 0;
}
#endif  /* GFH_DEC_TEST */
