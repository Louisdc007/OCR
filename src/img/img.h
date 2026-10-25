
#ifndef IMG_H
#define IMG_H

#include <SDL2/SDL.h>

typedef struct {
    int rows;
    int cols;
    int n_words;
} ExtractInfo;

SDL_Surface *img_load(const char *path);

void convert_bw(SDL_Surface *img, Uint8 c_val, int window_size);
SDL_Surface *rotate_img(SDL_Surface *src, double angle_degre);
double find_angle(SDL_Surface *img);

SDL_Surface *img_preprocess(SDL_Surface *src, double *angle_out);

int img_extract_letters(SDL_Surface *bw, const char *out_root, ExtractInfo *info);

#endif
