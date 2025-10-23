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
    printf("<html><head><title>Contrôle Domotique</title>");
    printf("<style>"
           "body { font-family: Arial; text-align: center; }"
           ".device { margin: 20px; padding: 10px; border: 1px solid #ccc; border-radius: 10px; display: inline-block; width: 200px; }"
           ".etat { font-weight: bold; }"
           ".on { color: green; }"
           ".off { color: red; }"
           "button { margin: 5px; padding: 10px 20px; font-size: 14px; }"
           "</style>");
    printf("</head><body>");
    printf("<h1>Tableau de bord domotique</h1>");

    // Lampe
    printf("<div class='device lampe'>");
    printf("<h2>Lampe</h2>");
    printf("<p>État : <span class='etat %s'>%s</span></p>",
           strcmp(etat, "ON") == 0 ? "on" : "off", etat);
    printf("<a href='comm.exe?action=on&device=lampe'><button>Allumer</button></a>");
    printf("<a href='comm.exe?action=off&device=lampe'><button>Éteindre</button></a>");
    printf("</div>");

    // Exemple : Store motorisé
    printf("<div class='device store'>");
    printf("<h2>Store motorisé</h2>");
    printf("<p>État : <span class='etat off'>FERMÉ</span></p>");
    printf("<a href='comm.exe?action=ouvrir&device=store'><button>Ouvrir</button></a>");
    printf("<a href='comm.exe?action=fermer&device=store'><button>Fermer</button></a>");
    printf("</div>");

    // Exemple : Clim réversible
    printf("<div class='device clim'>");
    printf("<h2>Climatisation</h2>");
    printf("<p>État : <span class='etat off'>ARRÊT</span></p>");
    printf("<a href='comm.exe?action=on&device=clim'><button>Allumer</button></a>");
    printf("<a href='comm.exe?action=off&device=clim'><button>Éteindre</button></a>");
    printf("</div>");

    printf("</body></html>");
    return 0;
}
