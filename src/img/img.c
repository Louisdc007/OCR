
#include "img.h"

#include <SDL2/SDL_image.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define ANGLE_MIN        -30.0
#define ANGLE_MAX         30.0
#define ANGLE_STEP        0.5
#define BW_C_VALUE        0
#define AUTO_C_FACTOR     0.33
#define AUTO_C_MIN        25
#define AUTO_C_MAX        80
#define ANGLE_FINE_STEP   0.1

#define MIN_LETTER_H_PX   4
#define MAX_LETTER_RATIO  3.0
#define MIN_LETTER_RATIO  0.3
#define BLOCK_GAP_RATIO   1.4
#define LINE_GAP_RATIO    0.5
#define WORD_GAP_RATIO    1.5
#define LETTER_PAD        2

static inline Uint32 *pix_at(SDL_Surface *s, int x, int y) {
    return (Uint32 *)((Uint8 *)s->pixels + (size_t)y * s->pitch) + x;
}

static inline Uint8 gray_at(SDL_Surface *s, int x, int y) {
    Uint8 r, g, b;
    SDL_GetRGB(*pix_at(s, x, y), s->format, &r, &g, &b);
    return (Uint8)(0.299 * r + 0.587 * g + 0.114 * b);
}

SDL_Surface *img_load(const char *path) {
    SDL_Surface *raw = IMG_Load(path);
    if (raw == NULL) {
        fprintf(stderr, "IMG_Load : %s\n", IMG_GetError());
        return NULL;
    }
    SDL_Surface *img = SDL_ConvertSurfaceFormat(raw, SDL_PIXELFORMAT_RGBA32, 0);
    SDL_FreeSurface(raw);
    return img;
}

void convert_bw(SDL_Surface *img, Uint8 c_val, int window_size) {
    if (img == NULL) return;

    int w = img->w, h = img->h;
    size_t stride = (size_t)w + 1;

    Uint8 *gray = malloc((size_t)w * h);
    unsigned long long *integ = calloc(stride * ((size_t)h + 1), sizeof *integ);
    if (gray == NULL || integ == NULL) {
        free(gray);
        free(integ);
        return;
    }

    for (int y = 0; y < h; y++) {
        unsigned long long row = 0;
        for (int x = 0; x < w; x++) {
            Uint8 g = gray_at(img, x, y);
            gray[(size_t)y * w + x] = g;
            row += g;
            integ[(size_t)(y + 1) * stride + (x + 1)] = integ[(size_t)y * stride + (x + 1)] + row;
        }
    }

    Uint8 *dmap = malloc((size_t)w * h);
    if (dmap == NULL) {
        free(gray);
        free(integ);
        return;
    }

    size_t hist[256] = {0};
    int half = window_size / 2;

    for (int y = 0; y < h; y++) {
        int y0 = y - half < 0 ? 0 : y - half;
        int y1 = y + half >= h ? h - 1 : y + half;
        for (int x = 0; x < w; x++) {
            int x0 = x - half < 0 ? 0 : x - half;
            int x1 = x + half >= w ? w - 1 : x + half;

            unsigned long long sum = integ[(size_t)(y1 + 1) * stride + (x1 + 1)]
                                   - integ[(size_t)y0 * stride + (x1 + 1)]
                                   - integ[(size_t)(y1 + 1) * stride + x0]
                                   + integ[(size_t)y0 * stride + x0];
            int count = (x1 - x0 + 1) * (y1 - y0 + 1);
            int mean = (int)(sum / count);

            int d = mean - gray[(size_t)y * w + x];
            if (d < 0) d = 0;
            dmap[(size_t)y * w + x] = (Uint8)d;
            hist[d]++;
        }
    }

    int c = c_val;
    if (c == 0) {
        size_t total = (size_t)w * h, acc = 0;
        int p98 = 255;
        for (int v = 0; v < 256; v++) {
            acc += hist[v];
            if (acc >= total * 98 / 100) {
                p98 = v;
                break;
            }
        }
        c = (int)(AUTO_C_FACTOR * p98);
        if (c < AUTO_C_MIN) c = AUTO_C_MIN;
        if (c > AUTO_C_MAX) c = AUTO_C_MAX;
    }

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            Uint8 val = dmap[(size_t)y * w + x] > c ? 0 : 255;
            *pix_at(img, x, y) = SDL_MapRGB(img->format, val, val, val);
        }
    }

    free(gray);
    free(integ);
    free(dmap);
}

SDL_Surface *rotate_img(SDL_Surface *src, double angle_degre) {
    double rad = angle_degre * M_PI / 180.0;
    double cos_a = cos(rad);
    double sin_a = sin(rad);

    int w = src->w;
    int h = src->h;

    SDL_Surface *dest = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_RGBA32);
    if (dest == NULL) return NULL;

    Uint32 white = SDL_MapRGB(dest->format, 255, 255, 255);
    double cx = w / 2.0;
    double cy = h / 2.0;

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            double src_x = cos_a * (x - cx) + sin_a * (y - cy) + cx;
            double src_y = -sin_a * (x - cx) + cos_a * (y - cy) + cy;

            if (src_x >= 0 && src_x < w - 1 && src_y >= 0 && src_y < h - 1) {
                *pix_at(dest, x, y) = *pix_at(src, (int)src_x, (int)src_y);
            } else {
                *pix_at(dest, x, y) = white;
            }
        }
    }
    return dest;
}

static double angle_score(const int *xs, const int *ys, size_t n, int w, int h, double angle, double *hist) {
    double cx = w / 2.0, cy = h / 2.0;
    double rad = angle * M_PI / 180.0;
    double c = cos(rad), s = sin(rad);

    memset(hist, 0, (size_t)h * sizeof(double));
    for (size_t i = 0; i < n; i++) {
        double ny = s * (xs[i] - cx) + c * (ys[i] - cy) + cy;
        int row = (int)floor(ny + 0.5);
        if (row >= 0 && row < h) hist[row] += 1.0;
    }

    double score = 0.0;
    for (int y = 0; y < h; y++) score += hist[y] * hist[y];
    return score;
}

double find_angle(SDL_Surface *img) {
    int w = img->w, h = img->h;

    size_t n = 0;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            if (gray_at(img, x, y) < 128) n++;
    if (n == 0) return 0.0;

    int *xs = malloc(n * sizeof(int));
    int *ys = malloc(n * sizeof(int));
    double *hist = malloc((size_t)h * sizeof(double));
    if (xs == NULL || ys == NULL || hist == NULL) {
        free(xs);
        free(ys);
        free(hist);
        return 0.0;
    }

    size_t k = 0;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            if (gray_at(img, x, y) < 128) {
                xs[k] = x;
                ys[k] = y;
                k++;
            }

    double best_angle = 0.0, max_score = -1.0;

    for (double angle = ANGLE_MIN; angle <= ANGLE_MAX + 1e-9; angle += ANGLE_STEP) {
        double score = angle_score(xs, ys, n, w, h, angle, hist);
        if (score > max_score) {
            max_score = score;
            best_angle = angle;
        }
    }

    double coarse = best_angle;
    for (double angle = coarse - ANGLE_STEP; angle <= coarse + ANGLE_STEP + 1e-9; angle += ANGLE_FINE_STEP) {
        double score = angle_score(xs, ys, n, w, h, angle, hist);
        if (score > max_score) {
            max_score = score;
            best_angle = angle;
        }
    }

    free(xs);
    free(ys);
    free(hist);
    return best_angle;
}

SDL_Surface *img_preprocess(SDL_Surface *src, double *angle_out) {
    if (src == NULL) return NULL;

    SDL_Surface *work = SDL_ConvertSurfaceFormat(src, SDL_PIXELFORMAT_RGBA32, 0);
    if (work == NULL) return NULL;

    int win = (work->w < work->h ? work->w : work->h) / 8;
    if (win < 15) win = 15;

    convert_bw(work, BW_C_VALUE, win);
    double angle = find_angle(work);
    SDL_Surface *rotated = rotate_img(work, angle);
    SDL_FreeSurface(work);

    if (angle_out) *angle_out = angle;
    return rotated;
}

typedef struct {
    int x, y, w, h;
} Box;

static int cmp_int(const void *a, const void *b) {
    int x = *(const int *)a, y = *(const int *)b;
    return (x > y) - (x < y);
}

static int median_int(const int *v, int n) {
    int *tmp = malloc((size_t)n * sizeof(int));
    if (tmp == NULL) return 0;
    memcpy(tmp, v, (size_t)n * sizeof(int));
    qsort(tmp, (size_t)n, sizeof(int), cmp_int);
    int m = tmp[n / 2];
    free(tmp);
    return m;
}

static int mkdir_p(const char *path) {
    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    if (mkdir(tmp, 0755) != 0 && errno != EEXIST) return -1;
    return 0;
}

static int find_components(const Uint8 *mask, int w, int h, Box **out) {
    size_t total = (size_t)w * h;
    Uint8 *seen = calloc(total, 1);
    int *stack = malloc(total * sizeof(int));
    int cap = 256, n = 0;
    Box *boxes = malloc((size_t)cap * sizeof(Box));
    if (seen == NULL || stack == NULL || boxes == NULL) {
        free(seen);
        free(stack);
        free(boxes);
        return -1;
    }

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            size_t idx = (size_t)y * w + x;
            if (!mask[idx] || seen[idx]) continue;

            int sp = 0;
            stack[sp++] = (int)idx;
            seen[idx] = 1;
            int x0 = x, x1 = x, y0 = y, y1 = y;

            while (sp > 0) {
                int cur = stack[--sp];
                int cx = cur % w, cy = cur / w;
                if (cx < x0) x0 = cx;
                if (cx > x1) x1 = cx;
                if (cy < y0) y0 = cy;
                if (cy > y1) y1 = cy;

                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        if (dx == 0 && dy == 0) continue;
                        int nx = cx + dx, ny = cy + dy;
                        if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
                        size_t nidx = (size_t)ny * w + nx;
                        if (mask[nidx] && !seen[nidx]) {
                            seen[nidx] = 1;
                            stack[sp++] = (int)nidx;
                        }
                    }
                }
            }

            if (n == cap) {
                cap *= 2;
                Box *nb = realloc(boxes, (size_t)cap * sizeof(Box));
                if (nb == NULL) {
                    free(seen);
                    free(stack);
                    free(boxes);
                    return -1;
                }
                boxes = nb;
            }
            boxes[n].x = x0;
            boxes[n].y = y0;
            boxes[n].w = x1 - x0 + 1;
            boxes[n].h = y1 - y0 + 1;
            n++;
        }
    }

    free(seen);
    free(stack);
    *out = boxes;
    return n;
}

static const Box *g_boxes;
static int g_axis;

static int key_of(const Box *b, int axis) {
    switch (axis) {
    case 0: return b->x;
    case 1: return b->y;
    case 2: return 2 * b->x + b->w;
    default: return 2 * b->y + b->h;
    }
}

static int cmp_idx(const void *a, const void *b) {
    int ka = key_of(&g_boxes[*(const int *)a], g_axis);
    int kb = key_of(&g_boxes[*(const int *)b], g_axis);
    return (ka > kb) - (ka < kb);
}

static void sort_idx(const Box *b, int *idx, int n, int axis) {
    g_boxes = b;
    g_axis = axis;
    qsort(idx, (size_t)n, sizeof(int), cmp_idx);
}

typedef struct {
    int *idx;
    int n;
} Block;

typedef struct {
    Block *v;
    int n, cap;
} BlockList;

static void block_add(BlockList *bl, int *idx, int n) {
    if (bl->n == bl->cap) {
        bl->cap = bl->cap ? bl->cap * 2 : 8;
        bl->v = realloc(bl->v, (size_t)bl->cap * sizeof(Block));
    }
    bl->v[bl->n].idx = idx;
    bl->v[bl->n].n = n;
    bl->n++;
}

static void largest_gap(const Box *b, int *idx, int n, int axis, int *gap, int *split) {
    sort_idx(b, idx, n, axis);
    *gap = 0;
    *split = -1;
    const Box *f = &b[idx[0]];
    int max_end = axis == 0 ? f->x + f->w : f->y + f->h;
    for (int i = 1; i < n; i++) {
        const Box *c = &b[idx[i]];
        int start = axis == 0 ? c->x : c->y;
        int end = axis == 0 ? c->x + c->w : c->y + c->h;
        int g = start - max_end;
        if (g > *gap) {
            *gap = g;
            *split = i;
        }
        if (end > max_end) max_end = end;
    }
}

static int median_gap(const Box *b, int n, int m_h, int axis) {
    int *gaps = malloc((size_t)n * sizeof(int));
    if (gaps == NULL) return m_h;

    int k = 0;
    for (int i = 0; i < n; i++) {
        int best = -1;
        for (int j = 0; j < n; j++) {
            if (j == i) continue;
            int g;
            if (axis == 0) {
                if (abs(key_of(&b[j], 3) - key_of(&b[i], 3)) >= m_h) continue;
                g = b[j].x - (b[i].x + b[i].w);
            } else {
                if (abs(key_of(&b[j], 2) - key_of(&b[i], 2)) >= m_h) continue;
                g = b[j].y - (b[i].y + b[i].h);
            }
            if (g < 0) continue;
            if (best < 0 || g < best) best = g;
        }
        if (best >= 0) gaps[k++] = best;
    }

    int med = k > 0 ? median_int(gaps, k) : m_h;
    free(gaps);
    return med < 1 ? 1 : med;
}

static void partition(const Box *b, int *idx, int n, int min_gap_x, int min_gap_y, BlockList *out) {
    if (n <= 1) {
        block_add(out, idx, n);
        return;
    }
    int gx, sx, gy, sy;
    largest_gap(b, idx, n, 0, &gx, &sx);
    largest_gap(b, idx, n, 1, &gy, &sy);

    if (gx < min_gap_x && gy < min_gap_y) {
        block_add(out, idx, n);
        return;
    }
    int axis = (double)gx / min_gap_x >= (double)gy / min_gap_y ? 0 : 1;
    int split = axis == 0 ? sx : sy;
    sort_idx(b, idx, n, axis);
    partition(b, idx, split, min_gap_x, min_gap_y, out);
    partition(b, idx + split, n - split, min_gap_x, min_gap_y, out);
}

static int cluster_axis(const Box *b, int *idx, int n, int axis, double thr, int *label) {
    sort_idx(b, idx, n, axis);
    int k = 0;
    int prev = key_of(&b[idx[0]], axis);
    label[idx[0]] = 0;
    for (int i = 1; i < n; i++) {
        int c = key_of(&b[idx[i]], axis);
        if (c - prev > 2.0 * thr) k++;
        label[idx[i]] = k;
        prev = c;
    }
    return k + 1;
}

static Box box_union(Box a, Box c) {
    int x0 = a.x < c.x ? a.x : c.x;
    int y0 = a.y < c.y ? a.y : c.y;
    int x1 = a.x + a.w > c.x + c.w ? a.x + a.w : c.x + c.w;
    int y1 = a.y + a.h > c.y + c.h ? a.y + a.h : c.y + c.h;
    Box r = {x0, y0, x1 - x0, y1 - y0};
    return r;
}

static int save_letter(SDL_Surface *bw, Box b, const char *path) {
    int side = (b.w > b.h ? b.w : b.h) + 2 * LETTER_PAD;
    SDL_Surface *dst = SDL_CreateRGBSurfaceWithFormat(0, side, side, 32, SDL_PIXELFORMAT_RGBA32);
    if (dst == NULL) return -1;

    SDL_FillRect(dst, NULL, SDL_MapRGB(dst->format, 255, 255, 255));
    SDL_Rect src = {b.x, b.y, b.w, b.h};
    SDL_Rect d = {(side - b.w) / 2, (side - b.h) / 2, b.w, b.h};
    SDL_SetSurfaceBlendMode(bw, SDL_BLENDMODE_NONE);
    SDL_BlitSurface(bw, &src, dst, &d);

    int rc = IMG_SavePNG(dst, path);
    SDL_FreeSurface(dst);
    return rc;
}

int img_extract_letters(SDL_Surface *bw, const char *out_root, ExtractInfo *info) {
    if (bw == NULL || out_root == NULL) return -1;
    if (info) memset(info, 0, sizeof *info);

    int w = bw->w, h = bw->h;
    int ret = -1;

    Uint8 *mask = malloc((size_t)w * h);
    if (mask == NULL) return -1;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            mask[(size_t)y * w + x] = gray_at(bw, x, y) < 128;

    Box *all = NULL;
    int nall = find_components(mask, w, h, &all);
    free(mask);
    if (nall <= 0) {
        fprintf(stderr, "Aucune composante trouvée\n");
        free(all);
        return -1;
    }

    int min_h = (int)(h * 0.006);
    if (min_h < MIN_LETTER_H_PX) min_h = MIN_LETTER_H_PX;

    int *hs = malloc((size_t)nall * sizeof(int));
    Box *b = malloc((size_t)nall * sizeof(Box));
    int *idx = malloc((size_t)nall * sizeof(int));
    int *label = calloc((size_t)nall, sizeof(int));
    if (!hs || !b || !idx || !label) goto cleanup;

    int k = 0;
    for (int i = 0; i < nall; i++)
        if (all[i].h >= min_h) hs[k++] = all[i].h;
    if (k == 0) goto cleanup;
    int m_h = median_int(hs, k);

    int nb = 0;
    for (int i = 0; i < nall; i++) {
        if (all[i].h < min_h) continue;
        if (all[i].h > MAX_LETTER_RATIO * m_h || all[i].w > MAX_LETTER_RATIO * m_h) continue;
        if (all[i].h < MIN_LETTER_RATIO * m_h) continue;
        b[nb++] = all[i];
    }
    if (nb == 0) goto cleanup;

    for (int i = 0; i < nb; i++) hs[i] = b[i].h;
    m_h = median_int(hs, nb);

    for (int i = 0; i < nb; i++) idx[i] = i;
    BlockList bl = {0};
    int min_gap_x = (int)(BLOCK_GAP_RATIO * median_gap(b, nb, m_h, 0));
    int min_gap_y = (int)(BLOCK_GAP_RATIO * median_gap(b, nb, m_h, 1));
    if (min_gap_x < 1) min_gap_x = 1;
    if (min_gap_y < 1) min_gap_y = 1;
    partition(b, idx, nb, min_gap_x, min_gap_y, &bl);
    if (bl.n == 0) goto cleanup;

    int grid_blk = 0;
    for (int i = 1; i < bl.n; i++)
        if (bl.v[i].n > bl.v[grid_blk].n) grid_blk = i;

    {
        Block *g = &bl.v[grid_blk];
        for (int i = 0; i < g->n; i++) hs[i] = b[g->idx[i]].h;
        double gh = median_int(hs, g->n);

        int *row_l = calloc((size_t)nb, sizeof(int));
        int *col_l = calloc((size_t)nb, sizeof(int));
        if (!row_l || !col_l) {
            free(row_l);
            free(col_l);
            free(bl.v);
            goto cleanup;
        }
        int rows = cluster_axis(b, g->idx, g->n, 3, LINE_GAP_RATIO * gh, row_l);
        int cols = cluster_axis(b, g->idx, g->n, 2, LINE_GAP_RATIO * gh, col_l);

        Box *cells = calloc((size_t)rows * cols, sizeof(Box));
        if (!cells) {
            free(row_l);
            free(col_l);
            free(bl.v);
            goto cleanup;
        }
        for (int i = 0; i < g->n; i++) {
            int id = g->idx[i];
            Box *c = &cells[(size_t)row_l[id] * cols + col_l[id]];
            *c = (c->w == 0) ? b[id] : box_union(*c, b[id]);
        }

        char dir[1024], path[1100];
        snprintf(dir, sizeof(dir), "%s/grid", out_root);
        mkdir_p(dir);
        for (int r = 0; r < rows; r++) {
            for (int c = 0; c < cols; c++) {
                Box *cell = &cells[(size_t)r * cols + c];
                if (cell->w == 0) continue;
                snprintf(path, sizeof(path), "%s/%d_%d.png", dir, r, c);
                save_letter(bw, *cell, path);
            }
        }
        free(cells);
        free(row_l);
        free(col_l);
        if (info) {
            info->rows = rows;
            info->cols = cols;
        }
    }

    int n_words = 0;
    for (int bi = 0; bi < bl.n; bi++) {
        if (bi == grid_blk) continue;
        Block *blk = &bl.v[bi];
        for (int i = 0; i < blk->n; i++) hs[i] = b[blk->idx[i]].h;
        double bh = median_int(hs, blk->n);

        int nlines = cluster_axis(b, blk->idx, blk->n, 3, LINE_GAP_RATIO * bh, label);
        int *line = malloc((size_t)blk->n * sizeof(int));
        if (!line) continue;

        for (int L = 0; L < nlines; L++) {
            int cnt = 0;
            for (int i = 0; i < blk->n; i++)
                if (label[blk->idx[i]] == L) line[cnt++] = blk->idx[i];
            if (cnt == 0) continue;
            sort_idx(b, line, cnt, 0);

            int start = 0;
            int max_end = b[line[0]].x + b[line[0]].w;
            for (int i = 1; i <= cnt; i++) {
                int cut = (i == cnt) || (b[line[i]].x - max_end > WORD_GAP_RATIO * bh);
                if (cut) {
                    if (i - start >= 2) {
                        n_words++;
                        char dir[1024], path[1100];
                        snprintf(dir, sizeof(dir), "%s/words/%d", out_root, n_words);
                        mkdir_p(dir);
                        for (int j = start; j < i; j++) {
                            snprintf(path, sizeof(path), "%s/%d.png", dir, j - start + 1);
                            save_letter(bw, b[line[j]], path);
                        }
                    }
                    start = i;
                    if (i < cnt) max_end = b[line[i]].x + b[line[i]].w;
                } else {
                    int e = b[line[i]].x + b[line[i]].w;
                    if (e > max_end) max_end = e;
                }
            }
        }
        free(line);
    }
    if (info) info->n_words = n_words;

    printf("Grille : %d lignes x %d colonnes, %d mots\n",
           info ? info->rows : 0, info ? info->cols : 0, n_words);
    free(bl.v);
    ret = 0;

cleanup:
    free(all);
    free(hs);
    free(b);
    free(idx);
    free(label);
    return ret;
}
