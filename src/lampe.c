#include "domotique.h"
#include "socket_client.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <windows.h>   // pour Sleep()

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

/* --- Convertit "00010111" → 8 octets (0x00 ou 0x01) --- */
void input_to_bytes(const char *input, unsigned char *buffer) {
    for (int i = 0; i < 8 && input[i]; i++) {
        buffer[i] = (input[i] == '1') ? 1 : 0;
    }
}

/* --- Convertit "192.168.0.100" → 4 octets --- */
void ip_to_bytes(const char *ip, unsigned char *buffer) {
    unsigned int a, b, c, d;
    sscanf(ip, "%u.%u.%u.%u", &a, &b, &c, &d);
    buffer[0] = (unsigned char)a;
    buffer[1] = (unsigned char)b;
    buffer[2] = (unsigned char)c;
    buffer[3] = (unsigned char)d;
}

int main(void) {
    const char *group = "lamps";
    char device[128] = "lamp1";
    char etat[MAX_LEN] = "";

    /* --- Récupération du device depuis la query string --- */
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

    /* --- Décodage URL --- */
    char decoded_device[128];
    urldecode(decoded_device, device);

    /* --- Lecture du JSON --- */
    char ip_auto[64] = "";
    char input_auto[64] = "";
    char current_state[32] = "";
    get_lamp_info(decoded_device, ip_auto, input_auto, current_state);

    /* --- Paramètres simulateur --- */
    const char *sim_host =  "192.168.56.1";//"127.0.0.1";  
    const int sim_port = 52079;

    /* --- Construction du message binaire (13 octets) --- */
    unsigned char msg[13];
    memset(msg, 0, sizeof(msg));

    ip_to_bytes(ip_auto, msg);            // Octets 0–3 : IP
    input_to_bytes(input_auto, msg + 4);  // Octets 4–11 : bits automate

    /* --- Choix action utilisateur --- */
    /* --- Choix action utilisateur --- */
if (qs && strstr(qs, "action=on")) {
    set_device_state_json(group, decoded_device, "ON");
    strcpy(etat, "ON");
    msg[12] = 1; // état ON

    // 🔁 Envoi 3 fois la même trame (comme le simulateur)
    for (int i = 0; i < 3; i++) {
        send_to_simulator_binary(sim_host, sim_port, msg, sizeof(msg));
        Sleep(50);  // 50 ms entre chaque envoi
    }
}
else if (qs && strstr(qs, "action=off")) {
    set_device_state_json(group, decoded_device, "OFF");
    strcpy(etat, "OFF");
    msg[12] = 0; // état OFF

    for (int i = 0; i < 3; i++) {
        send_to_simulator_binary(sim_host, sim_port, msg, sizeof(msg));
        Sleep(50);
    }
}



    /* --- Génération HTML --- */
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
