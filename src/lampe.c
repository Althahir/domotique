#include "domotique.h"

int main(void) {
    char etat[MAX_LEN];
    const char *group = "lamps";

    /* Récupère query string */
    char *qs = getenv("QUERY_STRING");
    char device[64];
    strcpy(device, "lamp1"); /* valeur par défaut */

    /* chercher device=xxx dans QUERY_STRING */
    if (qs) {
        char *p = strstr(qs, "device=");
        if (p) {
            p += strlen("device=");
            size_t i = 0;
            while (*p && *p != '&' && i < sizeof(device)-1) {
                device[i++] = *p++;
            }
            device[i] = '\0';
        }
    }

    /* Actions */
    if (qs && strstr(qs, "action=on")) {
        set_device_state_json(group, device, "ON");
    } else if (qs && strstr(qs, "action=off")) {
        set_device_state_json(group, device, "OFF");
    }

    /* Lecture de l’état actuel */
    get_device_state_json(group, device, etat, "OFF");

    /* Envoi du HTML */
    html_header("Lampe");

    /* ✅ Debug du chemin JSON */
    printf("<p><em>DEBUG : fichier JSON utilisé → %s</em></p>", STATE_JSON_PATH);

    printf("<div class='wrap'>");
    printf("<h1>Lampe %s</h1>", device);
    printf("<p>État actuel : <span class='%s'>%s</span></p>",
           (strcmp(etat, "ON") == 0) ? "on" : "off", etat);

    printf("<p>");
    printf("<a href='/cgi-bin/lampe.exe?device=%s&action=on'><button>Allumer</button></a>", device);
    printf("<a href='/cgi-bin/lampe.exe?device=%s&action=off'><button>Éteindre</button></a>", device);
    printf("</p>");

    printf("<p><a href='/c/view/light.html'><button>Retour IHM</button></a></p>");
    printf("</div>");

    html_footer();
    return 0;
}
