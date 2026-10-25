#define _DEFAULT_SOURCE
#include "explorer.h"

#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#define PAD       16
#define HEADER_H  48
#define FOOTER_H  64
#define BTN_W     140
#define BTN_H     40
#define SCROLL_STEP 3

typedef struct {
    char *name;
    int is_dir;
    SDL_Texture *tex;
    int tw, th;
} Entry;

struct Explorer {
    SDL_Renderer *renderer;
    TTF_Font *font;
    SDL_Rect area;
    SDL_Rect list;
    SDL_Rect cancel;
    char cwd[PATH_MAX];
    char chosen[PATH_MAX * 2];
    Entry *entries;
    int count;
    int scroll;
    int row_h;
    int hover;
    SDL_Texture *title_tex;
    int title_w, title_h;
    SDL_Texture *cancel_tex;
    int cancel_w, cancel_h;
};

static int is_image(const char *name) {
    const char *dot = strrchr(name, '.');
    if (dot == NULL) return 0;
    return strcasecmp(dot, ".png") == 0 || strcasecmp(dot, ".jpg") == 0
        || strcasecmp(dot, ".jpeg") == 0 || strcasecmp(dot, ".bmp") == 0;
}

static int rank(const Entry *e) {
    if (strcmp(e->name, "..") == 0) return 0;
    return e->is_dir ? 1 : 2;
}

static int cmp_entry(const void *a, const void *b) {
    const Entry *x = a, *y = b;
    int rx = rank(x), ry = rank(y);
    if (rx != ry) return rx - ry;
    return strcasecmp(x->name, y->name);
}

static void free_entries(Explorer *e) {
    for (int i = 0; i < e->count; i++) {
        free(e->entries[i].name);
        if (e->entries[i].tex) SDL_DestroyTexture(e->entries[i].tex);
    }
    free(e->entries);
    e->entries = NULL;
    e->count = 0;
}

static SDL_Texture *make_text(Explorer *e, const char *text, SDL_Color color, int *w, int *h) {
    *w = 0;
    *h = 0;
    if (e->font == NULL || text[0] == '\0') return NULL;

    SDL_Surface *s = TTF_RenderUTF8_Blended(e->font, text, color);
    if (s == NULL) return NULL;

    SDL_Texture *t = SDL_CreateTextureFromSurface(e->renderer, s);
    *w = s->w;
    *h = s->h;
    SDL_FreeSurface(s);
    return t;
}

static void update_title(Explorer *e) {
    if (e->title_tex) SDL_DestroyTexture(e->title_tex);
    e->title_tex = NULL;
    if (e->font == NULL) return;

    char buf[PATH_MAX + 8];
    const char *tail = e->cwd;
    int max_w = e->area.w - 2 * PAD;
    int w = 0;

    snprintf(buf, sizeof(buf), "%s", e->cwd);
    while (*tail && TTF_SizeUTF8(e->font, buf, &w, NULL) == 0 && w > max_w) {
        tail++;
        while ((*tail & 0xC0) == 0x80) tail++;
        snprintf(buf, sizeof(buf), "...%s", tail);
    }

    SDL_Color white = {255, 255, 255, 255};
    e->title_tex = make_text(e, buf, white, &e->title_w, &e->title_h);
}

static int add_entry(Explorer *e, int *cap, const char *name, int is_dir) {
    if (e->count == *cap) {
        int new_cap = *cap * 2;
        Entry *grown = realloc(e->entries, sizeof(Entry) * (size_t)new_cap);
        if (grown == NULL) return -1;
        e->entries = grown;
        *cap = new_cap;
    }
    char *copy = strdup(name);
    if (copy == NULL) return -1;

    Entry *en = &e->entries[e->count++];
    en->name = copy;
    en->is_dir = is_dir;
    en->tex = NULL;
    en->tw = 0;
    en->th = 0;
    return 0;
}

static int load_dir(Explorer *e, const char *path) {
    char resolved[PATH_MAX];
    if (realpath(path, resolved) == NULL) return -1;

    DIR *d = opendir(resolved);
    if (d == NULL) return -1;

    free_entries(e);
    int cap = 64;
    e->entries = malloc(sizeof(Entry) * (size_t)cap);
    if (e->entries == NULL) {
        closedir(d);
        return -1;
    }
    snprintf(e->cwd, sizeof(e->cwd), "%s", resolved);

    if (strcmp(resolved, "/") != 0) add_entry(e, &cap, "..", 1);

    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (de->d_name[0] == '.') continue;

        char full[PATH_MAX * 2];
        struct stat st;
        snprintf(full, sizeof(full), "%s/%s", resolved, de->d_name);
        if (stat(full, &st) != 0) continue;

        int dir = S_ISDIR(st.st_mode);
        if (!dir && !(S_ISREG(st.st_mode) && is_image(de->d_name))) continue;
        if (add_entry(e, &cap, de->d_name, dir) != 0) break;
    }
    closedir(d);

    qsort(e->entries, (size_t)e->count, sizeof(Entry), cmp_entry);
    e->scroll = 0;
    e->hover = -1;
    update_title(e);
    return 0;
}

static int visible_rows(const Explorer *e) {
    int n = e->list.h / e->row_h;
    return n < 1 ? 1 : n;
}

static void scroll_by(Explorer *e, int rows) {
    int max_scroll = e->count - visible_rows(e);
    if (max_scroll < 0) max_scroll = 0;
    e->scroll += rows;
    if (e->scroll > max_scroll) e->scroll = max_scroll;
    if (e->scroll < 0) e->scroll = 0;
}

static int row_at(const Explorer *e, SDL_Point p) {
    if (!SDL_PointInRect(&p, &e->list)) return -1;
    int idx = e->scroll + (p.y - e->list.y) / e->row_h;
    return idx < e->count ? idx : -1;
}

static void join_path(const Explorer *e, const char *name, char *out, size_t size) {
    if (strcmp(e->cwd, "/") == 0)
        snprintf(out, size, "/%s", name);
    else
        snprintf(out, size, "%s/%s", e->cwd, name);
}

static ExplorerStatus activate(Explorer *e, int idx) {
    char target[PATH_MAX * 2];
    join_path(e, e->entries[idx].name, target, sizeof(target));

    if (e->entries[idx].is_dir) {
        load_dir(e, target);
        return EXPLORER_OPEN;
    }
    snprintf(e->chosen, sizeof(e->chosen), "%s", target);
    return EXPLORER_CHOSEN;
}

Explorer *explorer_create(SDL_Renderer *renderer, TTF_Font *font, SDL_Rect area, const char *start_dir) {
    Explorer *e = calloc(1, sizeof(Explorer));
    if (e == NULL) return NULL;

    e->renderer = renderer;
    e->font = font;
    e->area = area;
    e->hover = -1;
    e->row_h = (font ? TTF_FontHeight(font) : 20) + 10;

    e->list.x = area.x + PAD;
    e->list.y = area.y + HEADER_H;
    e->list.w = area.w - 2 * PAD;
    e->list.h = area.h - HEADER_H - FOOTER_H;

    e->cancel.x = area.x + area.w - PAD - BTN_W;
    e->cancel.y = area.y + area.h - FOOTER_H + (FOOTER_H - BTN_H) / 2;
    e->cancel.w = BTN_W;
    e->cancel.h = BTN_H;

    const char *start = start_dir;
    if (start == NULL || start[0] == '\0') start = getenv("HOME");
    if (start == NULL) start = "/";

    if (load_dir(e, start) != 0 && load_dir(e, "/") != 0) {
        free(e);
        return NULL;
    }

    SDL_Color white = {255, 255, 255, 255};
    e->cancel_tex = make_text(e, "Annuler", white, &e->cancel_w, &e->cancel_h);
    return e;
}

void explorer_destroy(Explorer *e) {
    if (e == NULL) return;
    free_entries(e);
    if (e->title_tex) SDL_DestroyTexture(e->title_tex);
    if (e->cancel_tex) SDL_DestroyTexture(e->cancel_tex);
    free(e);
}

ExplorerStatus explorer_handle_event(Explorer *e, const SDL_Event *event) {
    switch (event->type) {
    case SDL_MOUSEWHEEL:
        scroll_by(e, -event->wheel.y * SCROLL_STEP);
        break;

    case SDL_MOUSEMOTION: {
        SDL_Point p = {event->motion.x, event->motion.y};
        e->hover = row_at(e, p);
        break;
    }

    case SDL_KEYDOWN:
        if (event->key.keysym.sym == SDLK_ESCAPE) return EXPLORER_CANCELLED;
        if (event->key.keysym.sym == SDLK_BACKSPACE) {
            char parent[PATH_MAX * 2];
            join_path(e, "..", parent, sizeof(parent));
            load_dir(e, parent);
        }
        if (event->key.keysym.sym == SDLK_PAGEDOWN) scroll_by(e, visible_rows(e));
        if (event->key.keysym.sym == SDLK_PAGEUP) scroll_by(e, -visible_rows(e));
        break;

    case SDL_MOUSEBUTTONDOWN: {
        if (event->button.button != SDL_BUTTON_LEFT) break;
        SDL_Point p = {event->button.x, event->button.y};
        if (SDL_PointInRect(&p, &e->cancel)) return EXPLORER_CANCELLED;
        int idx = row_at(e, p);
        if (idx >= 0) return activate(e, idx);
        break;
    }
    }
    return EXPLORER_OPEN;
}

static void fill(SDL_Renderer *r, SDL_Rect rect, Uint8 R, Uint8 G, Uint8 B) {
    SDL_SetRenderDrawColor(r, R, G, B, 255);
    SDL_RenderFillRect(r, &rect);
}

void explorer_render(Explorer *e) {
    SDL_Renderer *r = e->renderer;

    fill(r, e->area, 30, 30, 30);

    SDL_Rect header = {e->area.x, e->area.y, e->area.w, HEADER_H};
    fill(r, header, 45, 45, 45);
    if (e->title_tex) {
        SDL_Rect dst = {e->area.x + PAD, e->area.y + (HEADER_H - e->title_h) / 2, e->title_w, e->title_h};
        SDL_RenderCopy(r, e->title_tex, NULL, &dst);
    }

    SDL_RenderSetClipRect(r, &e->list);
    int rows = e->list.h / e->row_h + 1;
    for (int i = 0; i < rows; i++) {
        int idx = e->scroll + i;
        if (idx >= e->count) break;

        SDL_Rect row = {e->list.x, e->list.y + i * e->row_h, e->list.w, e->row_h};
        if (idx == e->hover) fill(r, row, 60, 90, 140);

        Entry *en = &e->entries[idx];
        if (en->tex == NULL) {
            char label[300];
            if (en->is_dir && strcmp(en->name, "..") != 0)
                snprintf(label, sizeof(label), "%s/", en->name);
            else
                snprintf(label, sizeof(label), "%s", en->name);

            SDL_Color color = en->is_dir ? (SDL_Color){240, 200, 90, 255} : (SDL_Color){255, 255, 255, 255};
            en->tex = make_text(e, label, color, &en->tw, &en->th);
        }
        if (en->tex) {
            SDL_Rect dst = {row.x + 8, row.y + (e->row_h - en->th) / 2, en->tw, en->th};
            SDL_RenderCopy(r, en->tex, NULL, &dst);
        }
    }
    SDL_RenderSetClipRect(r, NULL);

    int visible = visible_rows(e);
    if (e->count > visible) {
        int max_scroll = e->count - visible;
        int thumb_h = e->list.h * visible / e->count;
        if (thumb_h < 24) thumb_h = 24;
        SDL_Rect thumb = {
            e->list.x + e->list.w - 6,
            e->list.y + (e->list.h - thumb_h) * e->scroll / max_scroll,
            6,
            thumb_h
        };
        fill(r, thumb, 120, 120, 120);
    }

    SDL_Rect footer = {e->area.x, e->area.y + e->area.h - FOOTER_H, e->area.w, FOOTER_H};
    fill(r, footer, 45, 45, 45);
    fill(r, e->cancel, 170, 60, 60);
    if (e->cancel_tex) {
        SDL_Rect dst = {
            e->cancel.x + (e->cancel.w - e->cancel_w) / 2,
            e->cancel.y + (e->cancel.h - e->cancel_h) / 2,
            e->cancel_w,
            e->cancel_h
        };
        SDL_RenderCopy(r, e->cancel_tex, NULL, &dst);
    }
}

const char *explorer_path(const Explorer *e) {
    return e->chosen;
}
