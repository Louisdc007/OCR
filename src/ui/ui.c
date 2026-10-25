#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>

#include "explorer.h"
#include "img.h"

#define WIN_W 1280
#define WIN_H 860
#define BAR_H 90

static SDL_Renderer *renderer;
static TTF_Font *font;
static SDL_Surface *original;
static SDL_Texture *texture;
static Explorer *explorer;
static char status[128];

static const SDL_Rect OPEN_BTN = {20, WIN_H - BAR_H + 20, 200, 50};
static const SDL_Rect START_BTN = {WIN_W - 220, WIN_H - BAR_H + 20, 200, 50};

static TTF_Font *open_font(void) {
    char path[1024];
    char *base = SDL_GetBasePath();
    TTF_Font *f = NULL;
    if (base) {
        snprintf(path, sizeof(path), "%sassets/DejaVuSans-Bold.ttf", base);
        f = TTF_OpenFont(path, 20);
        SDL_free(base);
    }
    if (f == NULL) f = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 20);
    return f;
}

static void draw_text(const char *text, int x, int y) {
    SDL_Color white = {255, 255, 255, 255};
    if (font == NULL) return;
    SDL_Surface *s = TTF_RenderUTF8_Blended(font, text, white);
    if (s == NULL) return;
    SDL_Texture *t = SDL_CreateTextureFromSurface(renderer, s);
    SDL_Rect dst = {x, y, s->w, s->h};
    SDL_RenderCopy(renderer, t, NULL, &dst);
    SDL_DestroyTexture(t);
    SDL_FreeSurface(s);
}

static void draw_button(SDL_Rect r, int red, int green, int blue, const char *label) {
    SDL_SetRenderDrawColor(renderer, red, green, blue, 255);
    SDL_RenderFillRect(renderer, &r);
    draw_text(label, r.x + 15, r.y + 12);
}

static void render(void) {
    if (explorer) {
        explorer_render(explorer);
        SDL_RenderPresent(renderer);
        return;
    }

    SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
    SDL_RenderClear(renderer);

    if (texture) {
        int tw, th;
        SDL_QueryTexture(texture, NULL, NULL, &tw, &th);
        double scale = (double)(WIN_H - BAR_H) / th;
        if ((double)WIN_W / tw < scale) scale = (double)WIN_W / tw;
        SDL_Rect dst = {(int)((WIN_W - tw * scale) / 2), (int)((WIN_H - BAR_H - th * scale) / 2),
                        (int)(tw * scale), (int)(th * scale)};
        SDL_RenderCopy(renderer, texture, NULL, &dst);
    }

    draw_button(OPEN_BTN, 60, 120, 200, "Ouvrir image");
    draw_button(START_BTN, 60, 170, 90, "Start traitement");
    draw_text(status, OPEN_BTN.x + OPEN_BTN.w + 30, OPEN_BTN.y + 12);
    SDL_RenderPresent(renderer);
}

static void show(SDL_Surface *surf) {
    if (texture) SDL_DestroyTexture(texture);
    texture = SDL_CreateTextureFromSurface(renderer, surf);
}

static void load_image(const char *path) {
    SDL_Surface *img = img_load(path);
    if (img == NULL) {
        snprintf(status, sizeof(status), "Image illisible");
        return;
    }
    if (original) SDL_FreeSurface(original);
    original = img;
    show(original);
    snprintf(status, sizeof(status), "Image chargée");
}

static void start_processing(void) {
    if (original == NULL) {
        snprintf(status, sizeof(status), "Ouvrez d'abord une image");
        return;
    }

    snprintf(status, sizeof(status), "Traitement en cours...");
    render();

    double angle = 0.0;
    SDL_Surface *bw = img_preprocess(original, &angle);
    if (bw == NULL) {
        snprintf(status, sizeof(status), "Erreur de traitement");
        return;
    }

    ExtractInfo info;
    if (img_extract_letters(bw, ".", &info) == 0)
        snprintf(status, sizeof(status), "Grille %dx%d - %d mots", info.rows, info.cols, info.n_words);
    else
        snprintf(status, sizeof(status), "Découpe impossible");

    show(bw);
    SDL_FreeSurface(bw);
}

int main(int argc, char *argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0 || IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG) == 0 || TTF_Init() < 0) {
        fprintf(stderr, "Initialisation SDL : %s\n", SDL_GetError());
        return 1;
    }

    font = open_font();
    if (font == NULL) fprintf(stderr, "Police introuvable : copiez DejaVuSans-Bold.ttf dans assets/\n");

    SDL_Window *window = SDL_CreateWindow("World solver", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                          WIN_W, WIN_H, SDL_WINDOW_SHOWN);
    renderer = window ? SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED) : NULL;
    if (renderer == NULL) {
        fprintf(stderr, "Fenêtre : %s\n", SDL_GetError());
        return 1;
    }

    if (argc > 1) load_image(argv[1]);

    SDL_Event event;
    render();
    while (SDL_WaitEvent(&event) && event.type != SDL_QUIT) {
        if (explorer) {
            ExplorerStatus st = explorer_handle_event(explorer, &event);
            if (st == EXPLORER_CHOSEN) load_image(explorer_path(explorer));
            if (st != EXPLORER_OPEN) {
                explorer_destroy(explorer);
                explorer = NULL;
            }
        } else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
            SDL_Point p = {event.button.x, event.button.y};
            if (SDL_PointInRect(&p, &OPEN_BTN))
                explorer = explorer_create(renderer, font, (SDL_Rect){0, 0, WIN_W, WIN_H}, NULL);
            else if (SDL_PointInRect(&p, &START_BTN))
                start_processing();
        }
        render();
    }

    explorer_destroy(explorer);
    if (texture) SDL_DestroyTexture(texture);
    if (original) SDL_FreeSurface(original);
    if (font) TTF_CloseFont(font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
    return 0;
}
