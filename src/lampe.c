#include "domotique.h"
#include "socket_client.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/* --- Fonction pour décoder %20 → espace, %xx → caractère --- */
void urldecode(char *dst, const char *src) {
    char a, b;
    while (*src) {
        if ((*src == '%') &&
            ((a = src[1]) && (b = src[2])) &&
            (isxdigit(a) && isxdigit(b))) {
            a = (a <= '9') ? a - '0' : (tolower(a) - 'a' + 10);
            b = (b <= '9') ? b - '0' : (tolower(b) - 'a' + 10);
            *dst++ = 16 * a + b;
            src += 3;
        } else if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

int main(void) {
    const char *group = "lamps";
    char device[128] = "lamp1";   // valeur par défaut
    char etat[MAX_LEN] = "";

    /* --- Récupère la query string --- */
    char *qs = getenv("QUERY_STRING");
    if (qs) {
        char *p = strstr(qs, "device=");
        if (p) {
            p += strlen("device=");
            size_t i = 0;
            while (*p && *p != '&' && i < sizeof(device) - 1) {
                device[i++] = *p++;
            }
            device[i] = '\0';
        }
    }

    /* --- Décodage de l’URL (%20 -> espace) --- */
    char decoded_device[128];
    urldecode(decoded_device, device);

    /* --- Variables pour les infos automate --- */
    char ip_auto[64] = "";
    char input_auto[64] = "";
    char current_state[32] = "";

    /* Lecture des infos IP / input / état depuis le JSON */
    get_lamp_info(decoded_device, ip_auto, input_auto, current_state);

    /* --- Configuration du simulateur --- */
    const char *sim_host = "192.168.56.1";   
    // const char *sim_host = "127.0.0.1";
    const int sim_port = 57411;              // Port du simulateur

    /* --- Actions utilisateur --- */
    if (qs && strstr(qs, "action=on")) {
        set_device_state_json(group, decoded_device, "ON");
        strcpy(etat, "ON");

        char msg[128];
        snprintf(msg, sizeof(msg), "%s,%s,ON\r\n", ip_auto, input_auto);
        send_to_simulator(sim_host, sim_port, ip_auto, input_auto, "ON");

    } 
    else if (qs && strstr(qs, "action=off")) {
        set_device_state_json(group, decoded_device, "OFF");
        strcpy(etat, "OFF");

        char msg[128];
        snprintf(msg, sizeof(msg), "%s,%s,OFF\r\n", ip_auto, input_auto);
        send_to_simulator(sim_host, sim_port, ip_auto, input_auto, "OFF");

    } 
    else {
        get_device_state_json(group, decoded_device, etat, "OFF");
    }

    /* --- Envoi du HTML --- */
    html_header("Lampe");

    printf("<div class='wrap'>");
    printf("<h2>Lampe %s</h2>", decoded_device);
    printf("<p><em>IP automate :</em> %s<br><em>Entrée automate :</em> %s</p>", 
           ip_auto[0] ? ip_auto : "(non défini)", 
           input_auto[0] ? input_auto : "(non défini)");

    printf("<p>État actuel : <span class='%s'>%s</span></p>",
           (strcmp(etat, "ON") == 0) ? "on" : "off", etat);

    printf("<p>");
    printf("<a href='/cgi-bin/lampe.exe?device=%s&action=on'><button>Allumer</button></a>", device);
    printf("<a href='/cgi-bin/lampe.exe?device=%s&action=off'><button>Éteindre</button></a>", device);
    printf("</p>");

    printf("<p><a href='/c/index.html'><button>Retour IHM</button></a></p>");
    printf("</div>");

    html_footer();
    return 0;
}
