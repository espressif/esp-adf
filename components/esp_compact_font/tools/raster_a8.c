/**
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

/**
 * Raster GB2312 coverage to a raw alpha dump. The GFH and GFZ encoders
 * read this. It does not write a device font.
 *
 *   gcc -O2 -o raster_a8 raster_a8.c $(pkg-config --cflags --libs freetype2)
 *   raster_a8 font.ttc 36 out.ra8 cps.txt
 *
 * File: magic 'RA81', px, count, then count records of
 *   u32 cp, i16 adv, i16 left, i16 top, u16 w, u16 h, w*h alpha bytes.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#define RA8_MAGIC  0x31414852u  /* 'RA81' little endian */

typedef struct {
    uint32_t  cp;
    int16_t   advance, left, top;
    uint16_t  w, h;
    uint8_t  *alpha;
} ent_t;

static int cmp_ent(const void *a, const void *b)
{
    uint32_t ca = ((const ent_t *)a)->cp;
    uint32_t cb = ((const ent_t *)b)->cp;
    return (ca > cb) - (ca < cb);
}

static int load_cp(const char *line, uint32_t *out)
{
    const unsigned char *p = (const unsigned char *)line;

    while (*p && isspace(*p)) {
        p++;
    }
    if (!*p || *p == '#') {
        return 0;
    }
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
        *out = (uint32_t)strtoul((const char *)p, NULL, 16);
        return 1;
    }
    return 0;
}

static int add_glyph(ent_t **ents, int *n, int *cap, uint32_t cp, FT_Face face)
{
    int flags = FT_LOAD_DEFAULT | FT_LOAD_NO_HINTING | FT_LOAD_NO_AUTOHINT;
    FT_GlyphSlot g;
    FT_Bitmap *bm;
    size_t sz;
    uint8_t *alpha;
    ent_t *e;

    if (FT_Load_Char(face, cp, flags) != 0) {
        return 0;
    }
    if (FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL) != 0) {
        return 0;
    }
    g = face->glyph;
    bm = &g->bitmap;
    if ((bm->width == 0) != (bm->rows == 0)) {
        return 0;
    }
    if (*n >= *cap) {
        ent_t *p;
        *cap = *cap ? *cap * 2 : 4096;
        p = realloc(*ents, (size_t)*cap * sizeof(ent_t));
        if (!p) {
            return -1;
        }
        *ents = p;
    }
    sz = (size_t)bm->width * bm->rows;
    alpha = NULL;
    if (sz > 0) {
        alpha = malloc(sz);
        if (!alpha) {
            return -1;
        }
        for (unsigned row = 0; row < bm->rows; ++row) {
            memcpy(alpha + row * bm->width, bm->buffer + row * bm->pitch, bm->width);
        }
    }
    e = &(*ents)[(*n)++];
    e->cp = cp;
    e->advance = (int16_t)(g->advance.x >> 6);
    e->left = (int16_t)(sz ? g->bitmap_left : 0);
    e->top = (int16_t)(sz ? g->bitmap_top : 0);
    e->w = (uint16_t)bm->width;
    e->h = (uint16_t)bm->rows;
    e->alpha = alpha;
    return 1;
}

int main(int argc, char **argv)
{
    const char *font_path;
    const char *out_path;
    const char *cps_path;
    int px;
    FT_Library lib;
    FT_Face face;
    FILE *cf;
    FILE *out;
    ent_t *ents = NULL;
    int n = 0, cap = 0, skipped = 0;
    char line[64];
    uint32_t magic = RA8_MAGIC;
    uint16_t px16, pad = 0;
    uint32_t count;

    if (argc != 5) {
        fprintf(stderr, "usage: %s font.ttc px out.ra8 cps.txt\n", argv[0]);
        return 1;
    }
    font_path = argv[1];
    px = atoi(argv[2]);
    out_path = argv[3];
    cps_path = argv[4];
    if (FT_Init_FreeType(&lib) || FT_New_Face(lib, font_path, 0, &face)) {
        fprintf(stderr, "font open failed: %s\n", font_path);
        return 1;
    }
    if (FT_Set_Pixel_Sizes(face, 0, px) != 0) {
        fprintf(stderr, "FT_Set_Pixel_Sizes failed\n");
        return 1;
    }
    cf = fopen(cps_path, "r");
    if (!cf) {
        perror(cps_path);
        return 1;
    }
    while (fgets(line, sizeof line, cf)) {
        uint32_t cp;
        int rc;
        if (!load_cp(line, &cp)) {
            continue;
        }
        rc = add_glyph(&ents, &n, &cap, cp, face);
        if (rc < 0) {
            fprintf(stderr, "OOM\n");
            return 1;
        }
        if (rc == 0) {
            skipped++;
        }
    }
    fclose(cf);
    qsort(ents, (size_t)n, sizeof(ent_t), cmp_ent);

    out = fopen(out_path, "wb");
    if (!out) {
        perror(out_path);
        return 1;
    }
    px16 = (uint16_t)px;
    count = (uint32_t)n;
    fwrite(&magic, 4, 1, out);
    fwrite(&px16, 2, 1, out);
    fwrite(&pad, 2, 1, out);
    fwrite(&count, 4, 1, out);
    for (int i = 0; i < n; ++i) {
        fwrite(&ents[i].cp, 4, 1, out);
        fwrite(&ents[i].advance, 2, 1, out);
        fwrite(&ents[i].left, 2, 1, out);
        fwrite(&ents[i].top, 2, 1, out);
        fwrite(&ents[i].w, 2, 1, out);
        fwrite(&ents[i].h, 2, 1, out);
        fwrite(ents[i].alpha, 1, (size_t)ents[i].w * ents[i].h, out);
        free(ents[i].alpha);
    }
    fclose(out);
    free(ents);
    FT_Done_Face(face);
    FT_Done_FreeType(lib);
    printf("wrote %s: %d glyphs (skipped %d) at %d px\n", out_path, n, skipped, px);
    return 0;
}
