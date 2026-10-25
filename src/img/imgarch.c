#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <stdio.h>

void convert_bw(SDL_Surface *img, Uint8 c_val, int window_size) {
    if (img == NULL) return;
    int is_lock = 0;
    if (SDL_MUSTLOCK(img)) {
        SDL_LockSurface(img); //requests the system to lock the memory to manipulate it
        is_lock = 1;
    }

    // get img size
    int w = img->w;
    int h = img->h;

    Uint8 *gray = malloc(w * h * sizeof(Uint8)); // for the gray img

    if (gray == NULL) {
        if (is_lock) SDL_UnlockSurface(img);
        return;
    }

    for (int y = 0; y < h; y++) { //lines of img
        for (int x = 0; x < w; x++) { //col of img
            Uint8 *pixel = (Uint8 *)img->pixels + y * img->pitch + x * img->format->BytesPerPixel;
            /*
             *Uint8 = Unsigned Integer 8-bits
             *pitch = line size
             *BytesPerPixel = pixel size
             */

            Uint32 pixel_value = *(Uint32 *)pixel;

            Uint8 r, g, b;
            SDL_GetRGB(pixel_value, img->format, &r, &g, &b);

            gray[y * w + x] = (Uint8)(0.299 * r + 0.587 * g + 0.114 *b);
        }
    }

    int half_w = window_size / 2;

    // adaptive image scanning
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int sum = 0; //sum gray levels
            int count = 0; //nb of pix found

            for (int ky = -half_w; ky <= half_w; ky++) { // scanning neighbors
                for (int kx = -half_w; kx <= half_w; kx++) {
                    // absolute coo of neighbor
                    int ny = y + ky;
                    int nx = x +kx;

                    if (nx >= 0 && nx < w && ny >= 0 && ny < h) {
                        sum += gray[ny * w + nx];
                        count ++;
                    }
                }
            }
            int local_mean = sum / count;

            Uint8 val; //final value

            if (gray[y * w + x] < local_mean - c_val) {
                val = 0;
            } else {
                val = 255;
            }

            Uint8 *pixel = (Uint8 *)img->pixels + y * img->pitch + x * img->format->BytesPerPixel;
            Uint32 new_pixel = SDL_MapRGB(img->format, val, val, val);
            *(Uint32 *)pixel = new_pixel;
        }
    }

    free(gray);
    if (is_lock) {
        SDL_UnlockSurface(img);
    }
}





SDL_Surface* rotate_img(SDL_Surface *src, double angle_degre) {
    double rad = angle_degre * M_PI / 180.0;
    double cos_a = cos(rad);
    double sin_a = sin(rad);

    int w = src->w;
    int h = src->h;

    SDL_Surface *dest = SDL_CreateRGBSurfaceWithFormat(0, w, h, src->format->BitsPerPixel, src->format->format);
    if (dest == NULL) return NULL;

    SDL_LockSurface(src);
    SDL_LockSurface(dest);

    double cx = w / 2.0;
    double cy = h / 2.0;

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            double src_x = cos_a * (x - cx) + sin_a * (y - cy) + cx;
            double src_y = -sin_a * (x - cx) + cos_a * (y - cy) + cy;

            Uint8 *dest_pixel = (Uint8 *)dest->pixels + y * dest->pitch + x * dest->format->BytesPerPixel;

            if (src_x >= 0 && src_x < w - 1 && src_y >= 0 && src_y < h - 1) {
                int x0 = (int)src_x;
                int y0 = (int)src_y;
                Uint8 *src_pixel = (Uint8 *)src->pixels + y0 * src->pitch + x0 * src->format->BytesPerPixel;
                Uint32 pixel_value = *(Uint32 *)src_pixel;
                *(Uint32 *)dest_pixel = pixel_value;
            } else {
                Uint32 blanc = SDL_MapRGB(dest->format, 255, 255, 255);
                *(Uint32 *)dest_pixel = blanc;
            }
        }
    }

    SDL_UnlockSurface(src);
    SDL_UnlockSurface(dest);

    return dest;
}







double find_angle(SDL_Surface *img) {

    /*
     * Find the best angle by counting the black pixels on each line
     */

    double best_angle = 0.0;
    double max_score = -1.0;

    for (double angle = -30.0; angle <= 30.0; angle += 1.0) {
        SDL_Surface *temp = rotate_img(img, angle);

        double score = 0; //total blck pix for each angle

        int w = temp->w; //temp rotate img h & w
        int h = temp->h;

        SDL_LockSurface(temp);

        for (int y = 0; y < h; y++) {
            int blck_pix_line = 0;

            for (int x = 0; x < w; x++) {
                Uint8 *pixel = (Uint8 *)temp->pixels + y * temp->pitch + x * temp->format->BytesPerPixel;
                Uint32 pix_value = *(Uint32 *)pixel;

                Uint8 r, g, b;

                SDL_GetRGB(pix_value, temp->format, &r, &g, &b);

                if (r < 128) {
                    blck_pix_line++;
                }
            }

            score += blck_pix_line * blck_pix_line;
        }

        SDL_UnlockSurface(temp);
        SDL_FreeSurface(temp);

        if (score > max_score) {
            max_score = score;
            best_angle = angle;
        }
    }
    return best_angle;
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

    convert_bw(image, 10 ,10);

    double angle = find_angle(image);
    SDL_Surface *rotated = rotate_img(image, angle);
    SDL_FreeSurface(image);
    image = rotated;


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