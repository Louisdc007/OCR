#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "img.h"

#define WIN_W 1280
#define WIN_H 860
#define BTN_W 200
#define BTN_H 50
#define BAR_H 90
#define FONT_PATH "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
#define FONT_SIZE 20
#define OUTPUT_DIR "."

static const SDL_Rect OPEN_BTN  = {20, WIN_H - BAR_H + 20, BTN_W, BTN_H};
static const SDL_Rect START_BTN = {WIN_W - BTN_W - 20, WIN_H - BAR_H + 20, BTN_W, BTN_H};

typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Surface *original;
    SDL_Surface *processed;
    SDL_Texture *texture;

    TTF_Font *font;
    SDL_Texture *open_label;
    SDL_Texture *start_label;
    SDL_Texture *status_label;
    SDL_Rect open_label_size;
    SDL_Rect start_label_size;
    SDL_Rect status_label_size;
} AppState;

static void set_texture(AppState *app, SDL_Surface *surf) {
    if (app->texture) SDL_DestroyTexture(app->texture);
    app->texture = SDL_CreateTextureFromSurface(app->renderer, surf);
}

static SDL_Texture *make_label(AppState *app, const char *text, SDL_Rect *size) {
    if (app->font == NULL) return NULL;

    SDL_Color white = {255, 255, 255, 255};
    SDL_Surface *s = TTF_RenderUTF8_Blended(app->font, text, white);
    if (s == NULL) return NULL;

    SDL_Texture *t = SDL_CreateTextureFromSurface(app->renderer, s);
    size->w = s->w;
    size->h = s->h;
    SDL_FreeSurface(s);
    return t;
}

static void set_status(AppState *app, const char *text) {
    if (app->status_label) SDL_DestroyTexture(app->status_label);
    app->status_label = make_label(app, text, &app->status_label_size);
}

static SDL_Rect fit_rect(SDL_Texture *tex) {
    SDL_Rect area = {0, 0, WIN_W, WIN_H - BAR_H};
    SDL_Rect dst = area;
    if (tex == NULL) return dst;

    int tw, th;
    SDL_QueryTexture(tex, NULL, NULL, &tw, &th);
    double scale = fmin((double)area.w / tw, (double)area.h / th);
    dst.w = (int)(tw * scale);
    dst.h = (int)(th * scale);
    dst.x = (area.w - dst.w) / 2;
    dst.y = (area.h - dst.h) / 2;
    return dst;
}

static void draw_button(SDL_Renderer *r, SDL_Rect rect, Uint8 R, Uint8 G, Uint8 B) {
    SDL_SetRenderDrawColor(r, R, G, B, 255);
    SDL_RenderFillRect(r, &rect);
}

static void draw_label_centered(SDL_Renderer *r, SDL_Texture *t, SDL_Rect size, SDL_Rect box) {
    if (t == NULL) return;
    SDL_Rect dst = {
        box.x + (box.w - size.w) / 2,
        box.y + (box.h - size.h) / 2,
        size.w,
        size.h
    };
    SDL_RenderCopy(r, t, NULL, &dst);
}

static void render(AppState *app) {
    SDL_SetRenderDrawColor(app->renderer, 30, 30, 30, 255);
    SDL_RenderClear(app->renderer);

    if (app->texture) {
        SDL_Rect dst = fit_rect(app->texture);
        SDL_RenderCopy(app->renderer, app->texture, NULL, &dst);
    }

    draw_button(app->renderer, OPEN_BTN, 60, 120, 200);
    draw_button(app->renderer, START_BTN, 60, 170, 90);

    draw_label_centered(app->renderer, app->open_label,  app->open_label_size,  OPEN_BTN);
    draw_label_centered(app->renderer, app->start_label, app->start_label_size, START_BTN);

    SDL_Rect status_zone = {OPEN_BTN.x + BTN_W, OPEN_BTN.y, START_BTN.x - (OPEN_BTN.x + BTN_W), BTN_H};
    draw_label_centered(app->renderer, app->status_label, app->status_label_size, status_zone);

    SDL_RenderPresent(app->renderer);
}

static void open_image(AppState *app) {
    const char *home = getenv("HOME");
    char cmd[1200];
    snprintf(cmd, sizeof(cmd),
             "zenity --file-selection --title='Choisir une image' --filename='%s/' "
             "--file-filter='Images | *.png *.jpg *.jpeg *.bmp' 2>/dev/null",
             home ? home : "");
    FILE *fp = popen(cmd, "r");
    if (fp == NULL) return;

    char path[1024] = {0};
    if (fgets(path, sizeof(path), fp) == NULL) {
        pclose(fp);
        return;
    }
    pclose(fp);
    path[strcspn(path, "\n")] = '\0';

    SDL_Surface *img = img_load(path);
    if (img == NULL) {
        set_status(app, "Image illisible");
        return;
    }

    if (app->original)  SDL_FreeSurface(app->original);
    if (app->processed) SDL_FreeSurface(app->processed);
    app->original  = img;
    app->processed = NULL;
    set_texture(app, img);
    set_status(app, "Image chargée");
}

static void start_processing(AppState *app) {
    if (app->original == NULL) {
        set_status(app, "Ouvrez d'abord une image");
        return;
    }

    set_status(app, "Traitement en cours...");
    render(app);

    double angle = 0.0;
    SDL_Surface *bw = img_preprocess(app->original, &angle);
    if (bw == NULL) {
        set_status(app, "Erreur de traitement");
        return;
    }
    printf("Angle détecté : %.1f°\n", angle);

    ExtractInfo info;
    int rc = img_extract_letters(bw, OUTPUT_DIR, &info);

    if (app->processed) SDL_FreeSurface(app->processed);
    app->processed = bw;
    set_texture(app, bw);

    if (rc == 0) {
        char msg[128];
        snprintf(msg, sizeof(msg), "Grille %dx%d - %d mots", info.rows, info.cols, info.n_words);
        set_status(app, msg);
    } else {
        set_status(app, "Découpe impossible");
    }
}

int main(void) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL_Init : %s\n", SDL_GetError());
        return 1;
    }

    if (IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG) == 0) {
        fprintf(stderr, "IMG_Init : %s\n", IMG_GetError());
        SDL_Quit();
        return 1;
    }

    if (TTF_Init() < 0) {
        fprintf(stderr, "TTF_Init : %s\n", TTF_GetError());
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    AppState app = {0};

    app.font = TTF_OpenFont(FONT_PATH, FONT_SIZE);
    if (app.font == NULL) {
        fprintf(stderr, "TTF_OpenFont : %s\n", TTF_GetError());
        fprintf(stderr, "Vérifiez FONT_PATH ou installez fonts-dejavu-core\n");
    }

    app.window = SDL_CreateWindow("World solver",
                                  SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                  WIN_W, WIN_H, SDL_WINDOW_SHOWN);
    if (app.window == NULL) {
        fprintf(stderr, "SDL_CreateWindow : %s\n", SDL_GetError());
        if (app.font) TTF_CloseFont(app.font);
        TTF_Quit();
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    app.renderer = SDL_CreateRenderer(app.window, -1, SDL_RENDERER_ACCELERATED);
    if (app.renderer == NULL) {
        fprintf(stderr, "SDL_CreateRenderer : %s\n", SDL_GetError());
        if (app.font) TTF_CloseFont(app.font);
        SDL_DestroyWindow(app.window);
        TTF_Quit();
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    app.open_label  = make_label(&app, "Ouvrir image", &app.open_label_size);
    app.start_label = make_label(&app, "Start traitement", &app.start_label_size);

    int running = 1;
    SDL_Event event;
    render(&app);

    while (running) {
        SDL_WaitEvent(&event);

        switch (event.type) {
        case SDL_QUIT:
            running = 0;
            break;

        case SDL_MOUSEBUTTONDOWN:
            if (event.button.button == SDL_BUTTON_LEFT) {
                SDL_Point p = {event.button.x, event.button.y};
                if (SDL_PointInRect(&p, &OPEN_BTN)) {
                    open_image(&app);
                } else if (SDL_PointInRect(&p, &START_BTN)) {
                    start_processing(&app);
                }
            }
            break;
        }

        render(&app);
    }

    if (app.open_label)   SDL_DestroyTexture(app.open_label);
    if (app.start_label)  SDL_DestroyTexture(app.start_label);
    if (app.status_label) SDL_DestroyTexture(app.status_label);
    if (app.texture)      SDL_DestroyTexture(app.texture);
    if (app.original)     SDL_FreeSurface(app.original);
    if (app.processed)    SDL_FreeSurface(app.processed);
    if (app.font)         TTF_CloseFont(app.font);
    SDL_DestroyRenderer(app.renderer);
    SDL_DestroyWindow(app.window);
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
    return 0;
}
