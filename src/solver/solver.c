#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <sys/types.h>

int free_tab(char **tab, int nb_l);

int count_letters(const char *line) {
    int n = 0;
    for (; *line; line++)
        if (isalpha((unsigned char)*line)) n++;

    return n;
}

int get_nb_lines(FILE *f) {
    /*
     *count th nb of lines
     */
    if (f == NULL) return -1;

    char *line = NULL;
    size_t linecap = 0;
    int count = 0;
    while (getline(&line, &linecap, f) > 0)
        if (count_letters(line) > 0) count ++;
    
    free(line);
    rewind(f);
    if (count <= 0) return -1;
    return count;
}

int get_nb_cols(FILE *f) {
    /*
     *count the nb of letters in
     *the first row (ignoring spaces)
     */
    if (f == NULL) return -1;

    char *line = NULL;
    size_t linecap = 0;
    int count = 0;

    while (getline(&line, &linecap, f) > 0) {
        count = count_letters(line);
        if (count > 0) break;
    }

    free(line);
    rewind(f);
    if (count <= 0) return -1;
    return count;
}


char **alloc_tab(FILE *f, int nb_lines, int nb_cols) {
    if (f == NULL) return NULL;

    char **tab = malloc(sizeof(char *) * nb_lines);
    if (tab == NULL) return NULL;
    for (int i = 0; i < nb_lines; i++) {
        tab[i] = malloc(sizeof(char) * nb_cols);
        if (tab[i] == NULL) {
            for (int j = 0; j < i; j++) {
                free(tab[j]);
            }
            free(tab);
            return NULL;
        }
    }
    return tab;
}


char **create_tab(FILE *f, int nb_c, int nb_l) {
    if (f == NULL) return NULL;
    rewind(f);

    char **tab = alloc_tab(f, nb_l, nb_c);
    if (tab == NULL) return NULL;

    char *line = NULL;
    size_t linecap = 0;

    int l = 0;

    while (l < nb_l && getline(&line, &linecap, f) > 0) {
        int n = count_letters(line);
        if (n == 0) continue;

        if (n != nb_c) {
            fprintf(stderr, "Ligne %d : %d lettres au lieu de %d\n", l, n, nb_c);
            free(line);
            free_tab(tab, nb_l);
            return NULL;
        }

        int c = 0;
        for (const char *p = line; *p; p++) {
            if (isalpha((unsigned char) *p)) tab[l][c++] = (char)toupper((unsigned char)*p);
        }
        l++;
    }

    
    free(line);
    return tab;
}


int free_tab(char **tab, int nb_l) {
    if (tab == NULL) return 1;

    for (int l = 0; l < nb_l; l++) {
        free(tab[l]);
    }
    free(tab);
    return 0;
}


static const int DX[8] = {1, -1, 0, 0, 1, 1, -1, -1};
static const int DY[8] = {0, 0, 1, -1, 1, -1, 1, -1};

int find_word(char **tab, int nb_l, int nb_c, const char *word,
              int *x0, int *y0, int *x1, int *y1) {
    int len = (int)strlen(word);
    if (len == 0) return 0;

    for (int y = 0; y < nb_l; y++) {
        for (int x = 0; x < nb_c; x++) {
            if (tab[y][x] != word[0]) continue;

            for (int d = 0; d < 8; d++) {
                int ex = x + DX[d] * (len - 1);
                int ey = y + DY[d] * (len - 1);
                if (ex < 0 || ex >= nb_c || ey < 0 || ey >= nb_l) continue;

                int k = 1;
                while (k < len && tab[y + DY[d] * k][x + DX[d] * k] == word[k]) k++;

                if (k == len) {
                    *x0 = x;
                    *y0 = y;
                    *x1 = ex;
                    *y1 = ey;
                    return 1;
                }
            }
        }
    }
    return 0;
}


int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage : %s <grille> <mot>\n", argv[0]);
        return 2;
    }

    FILE *f = fopen(argv[1], "r");
    if (f == NULL) {
        perror(argv[1]);
        return 2;
    }

    int nb_l = get_nb_lines(f);
    if (nb_l < 0) {
        fclose(f);
        return 2;
    }

    int nb_c = get_nb_cols(f);
    if (nb_c < 0) {
        fclose(f);
        return 2;
    }

    char **tab = create_tab(f, nb_c, nb_l);
    fclose(f);
    if (tab == NULL) return 2;

    char *word = strdup(argv[2]);
    if (word == NULL) {
        free_tab(tab, nb_l);
        return 2;
    }
    for (char *p = word; *p; p++) *p = (char)toupper((unsigned char)*p);

    int x0, y0, x1, y1;
    int found = find_word(tab, nb_l, nb_c, word, &x0, &y0, &x1, &y1);

    if (found)
        printf("(%d,%d)(%d,%d)\n", x0, y0, x1, y1);
    else
        printf("Not found\n");

    free(word);
    free_tab(tab, nb_l);
    return found ? 0 : 1;
}
