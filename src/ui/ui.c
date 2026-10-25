#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "explorer.h"
#include "img.h"

#define WIN_W 1280
#define WIN_H 860
#define BTN_W 200
#define BTN_H 50
#define BAR_H 90
#define FONT_FILE "assets/DejaVuSans-Bold.ttf"
#define FONT_SYSTEM_PATH "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
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

    Explorer *explorer;
} AppState;

static TTF_Font *open_font(void) {
    char path[1024];
    char *base = SDL_GetBasePath();
    if (base) {
        snprintf(path, sizeof(path), "%s%s", base, FONT_FILE);
        SDL_free(base);
        TTF_Font *font = TTF_OpenFont(path, FONT_SIZE);
        if (font) return font;
    }
    return TTF_OpenFont(FONT_SYSTEM_PATH, FONT_SIZE);
}

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
    if (app->explorer) {
        explorer_render(app->explorer);
        SDL_RenderPresent(app->renderer);
        return;
    }

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

static void close_explorer(AppState *app) {
    explorer_destroy(app->explorer);
    app->explorer = NULL;
}

static void open_explorer(AppState *app) {
    if (app->explorer) return;
    SDL_Rect area = {0, 0, WIN_W, WIN_H};
    app->explorer = explorer_create(app->renderer, app->font, area, NULL);
    if (app->explorer == NULL) set_status(app, "Explorateur indisponible");
}

static void load_image(AppState *app, const char *path) {
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

int main(int argc, char *argv[]) {
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

    app.font = open_font();
    if (app.font == NULL) {
        fprintf(stderr, "Police introuvable : copiez DejaVuSans-Bold.ttf dans assets/\n");
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

    if (argc > 1) load_image(&app, argv[1]);

    int running = 1;
    SDL_Event event;
    render(&app);

    while (running) {
        SDL_WaitEvent(&event);

        if (event.type == SDL_QUIT) {
            running = 0;
        } else if (event.type == SDL_DROPFILE) {
            close_explorer(&app);
            load_image(&app, event.drop.file);
            SDL_free(event.drop.file);
        } else if (app.explorer) {
            ExplorerStatus status = explorer_handle_event(app.explorer, &event);
            if (status == EXPLORER_CHOSEN) {
                load_image(&app, explorer_path(app.explorer));
                close_explorer(&app);
            } else if (status == EXPLORER_CANCELLED) {
                close_explorer(&app);
            }
        } else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
            SDL_Point p = {event.button.x, event.button.y};
            if (SDL_PointInRect(&p, &OPEN_BTN)) {
                open_explorer(&app);
            } else if (SDL_PointInRect(&p, &START_BTN)) {
                start_processing(&app);
            }
        }

        if (running) render(&app);
    }

    close_explorer(&app);
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
