#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STATE_FILE "etat.txt"

int main(void) {
    char query[100];
    char etat[10] = "OFF";

    // En-tête HTTP
    printf("Content-type: text/html\n\n");

    // Lecture des paramètres GET
    char *data = getenv("QUERY_STRING");
    if (data != NULL && strstr(data, "action=on")) {
        strcpy(etat, "ON");
    } else if (data != NULL && strstr(data, "action=off")) {
        strcpy(etat, "OFF");
    } else {
        // Lecture de l'état précédent dans le fichier
        FILE *f = fopen(STATE_FILE, "r");
        if (f != NULL) {
            fscanf(f, "%s", etat);
            fclose(f);
        }
    }

    // Sauvegarde du nouvel état
    FILE *f = fopen(STATE_FILE, "w");
    if (f != NULL) {
        fprintf(f, "%s", etat);
        fclose(f);
    }

    // Contenu HTML
    printf("<!DOCTYPE html>");
    printf("<html><head><title>Contrôle Lampe</title></head><body>");
    printf("<h1>Contrôle de la lampe</h1>");
    printf("<p>État actuel : <strong style='color:%s;'>%s</strong></p>",
           strcmp(etat, "ON") == 0 ? "green" : "red", etat);

    printf("<a href='comm.exe?action=on'><button>Allumer</button></a> ");
    printf("<a href='comm.exe?action=off'><button>Éteindre</button></a>");

    printf("</body></html>");
    return 0;
}
