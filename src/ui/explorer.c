#define _DEFAULT_SOURCE
#include "explorer.h"

#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#define ROW_H 30
#define HEADER_H 40
#define FOOTER_H 60

typedef struct {
    char name[256];
    int is_dir;
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
};

static int is_image(const char *name) {
    const char *dot = strrchr(name, '.');
    if (dot == NULL) return 0;
    return strcasecmp(dot, ".png") == 0 || strcasecmp(dot, ".jpg") == 0
        || strcasecmp(dot, ".jpeg") == 0 || strcasecmp(dot, ".bmp") == 0;
}

static int cmp_entry(const void *a, const void *b) {
    const Entry *x = a, *y = b;
    if (strcmp(x->name, "..") == 0) return -1;
    if (strcmp(y->name, "..") == 0) return 1;
    if (x->is_dir != y->is_dir) return y->is_dir - x->is_dir;
    return strcasecmp(x->name, y->name);
}

static void add_entry(Explorer *e, const char *name, int is_dir) {
    Entry *grown = realloc(e->entries, sizeof(Entry) * (size_t)(e->count + 1));
    if (grown == NULL) return;
    e->entries = grown;
    snprintf(e->entries[e->count].name, sizeof(e->entries[0].name), "%s", name);
    e->entries[e->count].is_dir = is_dir;
    e->count++;
}

static int load_dir(Explorer *e, const char *path) {
    char resolved[PATH_MAX];
    if (realpath(path, resolved) == NULL) return -1;

    DIR *d = opendir(resolved);
    if (d == NULL) return -1;

    free(e->entries);
    e->entries = NULL;
    e->count = 0;
    e->scroll = 0;
    snprintf(e->cwd, sizeof(e->cwd), "%s", resolved);

    if (strcmp(resolved, "/") != 0) add_entry(e, "..", 1);

    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (de->d_name[0] == '.') continue;

        char full[PATH_MAX * 2];
        struct stat st;
        snprintf(full, sizeof(full), "%s/%s", resolved, de->d_name);
        if (stat(full, &st) != 0) continue;

        if (S_ISDIR(st.st_mode))
            add_entry(e, de->d_name, 1);
        else if (S_ISREG(st.st_mode) && is_image(de->d_name))
            add_entry(e, de->d_name, 0);
    }
    closedir(d);

    qsort(e->entries, (size_t)e->count, sizeof(Entry), cmp_entry);
    return 0;
}

static void draw_text(Explorer *e, const char *text, SDL_Color color, int x, int y) {
    if (e->font == NULL) return;
    SDL_Surface *s = TTF_RenderUTF8_Blended(e->font, text, color);
    if (s == NULL) return;
    SDL_Texture *t = SDL_CreateTextureFromSurface(e->renderer, s);
    SDL_Rect dst = {x, y, s->w, s->h};
    SDL_RenderCopy(e->renderer, t, NULL, &dst);
    SDL_DestroyTexture(t);
    SDL_FreeSurface(s);
}

Explorer *explorer_create(SDL_Renderer *renderer, TTF_Font *font, SDL_Rect area, const char *start_dir) {
    Explorer *e = calloc(1, sizeof(Explorer));
    if (e == NULL) return NULL;

    e->renderer = renderer;
    e->font = font;
    e->area = area;
    e->list = (SDL_Rect){area.x, area.y + HEADER_H, area.w, area.h - HEADER_H - FOOTER_H};
    e->cancel = (SDL_Rect){area.x + area.w - 160, area.y + area.h - FOOTER_H + 10, 140, 40};

    const char *start = start_dir ? start_dir : getenv("HOME");
    if (start == NULL) start = "/";

    if (load_dir(e, start) != 0 && load_dir(e, "/") != 0) {
        free(e);
        return NULL;
    }
    return e;
}

void explorer_destroy(Explorer *e) {
    if (e == NULL) return;
    free(e->entries);
    free(e);
}

ExplorerStatus explorer_handle_event(Explorer *e, const SDL_Event *event) {
    int max_scroll = e->count - e->list.h / ROW_H;
    if (max_scroll < 0) max_scroll = 0;

    if (event->type == SDL_MOUSEWHEEL) {
        e->scroll -= event->wheel.y * 3;
        if (e->scroll < 0) e->scroll = 0;
        if (e->scroll > max_scroll) e->scroll = max_scroll;
    } else if (event->type == SDL_KEYDOWN) {
        if (event->key.keysym.sym == SDLK_ESCAPE) return EXPLORER_CANCELLED;
        if (event->key.keysym.sym == SDLK_BACKSPACE) {
            char parent[PATH_MAX * 2];
            snprintf(parent, sizeof(parent), "%s/..", e->cwd);
            load_dir(e, parent);
        }
    } else if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT) {
        SDL_Point p = {event->button.x, event->button.y};
        if (SDL_PointInRect(&p, &e->cancel)) return EXPLORER_CANCELLED;
        if (!SDL_PointInRect(&p, &e->list)) return EXPLORER_OPEN;

        int idx = e->scroll + (p.y - e->list.y) / ROW_H;
        if (idx >= e->count) return EXPLORER_OPEN;

        char target[PATH_MAX * 2];
        snprintf(target, sizeof(target), "%s/%s", e->cwd, e->entries[idx].name);
        if (e->entries[idx].is_dir) {
            load_dir(e, target);
        } else {
            snprintf(e->chosen, sizeof(e->chosen), "%s", target);
            return EXPLORER_CHOSEN;
        }
    }
    return EXPLORER_OPEN;
}

void explorer_render(Explorer *e) {
    SDL_Color white = {255, 255, 255, 255};
    SDL_Color yellow = {240, 200, 90, 255};

    SDL_SetRenderDrawColor(e->renderer, 30, 30, 30, 255);
    SDL_RenderClear(e->renderer);

    draw_text(e, e->cwd, white, e->area.x + 10, e->area.y + 8);

    SDL_RenderSetClipRect(e->renderer, &e->list);
    for (int i = 0; i < e->list.h / ROW_H + 1 && e->scroll + i < e->count; i++) {
        Entry *en = &e->entries[e->scroll + i];
        draw_text(e, en->name, en->is_dir ? yellow : white, e->list.x + 10, e->list.y + i * ROW_H + 4);
    }
    SDL_RenderSetClipRect(e->renderer, NULL);

    SDL_SetRenderDrawColor(e->renderer, 170, 60, 60, 255);
    SDL_RenderFillRect(e->renderer, &e->cancel);
    draw_text(e, "Annuler", white, e->cancel.x + 30, e->cancel.y + 8);
}

const char *explorer_path(const Explorer *e) {
    return e->chosen;
}
