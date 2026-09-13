#include <stdio.h>
#include <stdlib.h>


int get_nb_lines(FILE *f) {
    /*
     *count th nb of lines
     */
    if (f == NULL) return -1;

    char *line = NULL;
    size_t linecap = 0;
    int count = 0;
    while (getline(&line, &linecap, f) > 0) count++;

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
    ssize_t count = getline(&line, &linecap, f);

    free(line);
    rewind(f);
    if (count <= 0) return -1;
    return (int)count / 2;
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
    while (getline(&line, &linecap, f) > 0 && l < nb_l) {
        for (int c = 0; c < nb_c; c++) {
            tab[l][c] = line[c * 2];
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

int main() {
    FILE *f = fopen("a", "r");
    if (f == NULL) return 1;

    int nb_l = get_nb_lines(f);
    if (nb_l < 0) return 1;


    int nb_c = get_nb_cols(f);
    if (nb_c < 0) return 1;


    char **tab = create_tab(f, nb_c, nb_l);

    fclose(f);
    free_tab(tab, nb_l);
    return 0;
}