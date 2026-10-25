#ifndef EXPLORER_H
#define EXPLORER_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

typedef struct Explorer Explorer;

typedef enum {
    EXPLORER_OPEN,
    EXPLORER_CHOSEN,
    EXPLORER_CANCELLED
} ExplorerStatus;

Explorer *explorer_create(SDL_Renderer *renderer, TTF_Font *font, SDL_Rect area, const char *start_dir);
void explorer_destroy(Explorer *e);
ExplorerStatus explorer_handle_event(Explorer *e, const SDL_Event *event);
void explorer_render(Explorer *e);
const char *explorer_path(const Explorer *e);

#endif
