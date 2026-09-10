#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <stdio.h>

void convert_bw(SDL_Surface *img) {
    if (img == NULL) return;
    int is_lock = 0;
    if (SDL_MUSTLOCK(img)) {
        SDL_LockSurface(img); //requests the system to lock the memory to manipulate it
        is_lock = 1;
    }

    for (int y = 0; y < img->h; y++) { //raverse the image pixel by pixel
        for (int x = 0; x < img->w; x++) {
            Uint8 *pixel = (Uint8 *)img->pixels + y * img->pitch + x * img->format->BytesPerPixel;
            /*
             *Uint8 = Unsigned Integer 8-bits
             *pitch = line size
             *BytesPerPixel = pixel size
             */

            Uint32 pixel_value = *(Uint32 *)pixel;
            Uint8 r, g, b;
            SDL_GetRGB(pixel_value, img->format, &r, &g, &b);

            Uint8 gris = (Uint8)(0.299 * r + 0.587 * g + 0.114 * b);

            Uint32 new_pixel = SDL_MapRGB(img->format, gris, gris, gris);
            *(Uint32 *)pixel = new_pixel;
        }
    }
    if (is_lock) {
        SDL_UnlockSurface(img);
    }
}

SDL_Window* init_window(int width, int height) {
    if (width < 1 || height < 1) return NULL;

    if (SDL_Init(SDL_INIT_VIDEO) < 0) return NULL;

    SDL_Window *window = SDL_CreateWindow(
        "World solver",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        width,
        height,
        SDL_WINDOW_SHOWN
        );

    return window;
}

void render_image(SDL_Renderer *renderer, SDL_Surface *surface) {
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface); // surface = CPU, editable ; texture = GPU, displayable

    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL); // NULL = auto width and height
    SDL_RenderPresent(renderer); // to display
}

int main(int argc, char *argv[]) {
    SDL_Window *window = init_window(800, 600);
    if (window == NULL) return 1;
    if (argc > 2) return 1;

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED); // -1 = auto pilote, accelerated = GPU, not CPU

    SDL_Surface *image = IMG_Load(argv[1]);
    if (image == NULL) return 1;

    convert_bw(image);

    int running = 1;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = 0;
            }
        }

        render_image(renderer, image);
    }

    SDL_FreeSurface(image);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    IMG_Quit();
    SDL_Quit();

    return 0;
}