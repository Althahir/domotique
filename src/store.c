#include "domotique.h"
#include "socket_client.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <windows.h>   // pour Sleep()

/* --- Fonction pour décoder %20 → espace, %xx → caractère (réutilisée de lampe.c) --- */
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
    for (int i = 0; i < 8; i++) {
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

/* ------------------------------------------------------------------- */
/* PROGRAMME PRINCIPAL                                                 */
/* ------------------------------------------------------------------- */

int main(void) {
    char *qs = getenv("QUERY_STRING");
    char *device_param = NULL;
    char decoded_device[MAX_LEN] = "";

    char ip_auto[16] = "";
    char input_auto[9] = "";
    char current_state[8] = "";

    const char *group = "store";
    const char *sim_host = "192.168.56.1";
    // const int sim_port = 49995;
    int sim_port = get_simulator_port();
if (sim_port <= 0) sim_port = 49995; // valeur de secours


    /* 1. Extraction et décodage du paramètre 'device' */
    if (qs && (device_param = strstr(qs, "device="))) {
        device_param += 7; // Passe 'device='
        char *end = strchr(device_param, '&');

        char encoded_device[MAX_LEN];
        if (end) {
            strncpy(encoded_device, device_param, end - device_param);
            encoded_device[end - device_param] = '\0';
        } else {
            strcpy(encoded_device, device_param);
        }

        urldecode(decoded_device, encoded_device);
    }

    /* 2. Lecture des informations du périphérique (IP et Input) */
    if (!get_device_info(group, decoded_device, ip_auto, input_auto, current_state)) {
        html_header("Erreur CGI");

        FILE *log = fopen("C:\\xampp\\htdocs\\c\\src\\debug.log", "a");
        if (log) {
            fprintf(log, "[ERREUR STORE.C] Appareil introuvable dans le groupe '%s' : %s\n", group, decoded_device);
            fclose(log);
        }

        printf("<p style='color:red;'>❌ ERREUR : Appareil %s introuvable dans le JSON (groupe %s).</p>", decoded_device, group);
        html_footer();
        return 0;
    }

    /* 3. Préparation du message binaire (13 octets) */
    unsigned char msg[13] = {0};

    // Octets 0–3 : IP
    ip_to_bytes(ip_auto, msg);

    // Octets 4–11 : input binaire
    input_to_bytes(input_auto, &msg[4]);

    char etat[16] = "";

    /* 4. Traitement de la requête */
    if (qs && strstr(qs, "action=on")) {
        set_device_state_json(group, decoded_device, "ON");
        strcpy(etat, "OUVERT");
        msg[12] = 1; // Commande ouverture

        for (int i = 0; i < 3; i++) {
            send_to_simulator_binary(sim_host, sim_port, msg, sizeof(msg));
            Sleep(50);
        }
    }
    else if (qs && strstr(qs, "action=off")) {
        set_device_state_json(group, decoded_device, "OFF");
        strcpy(etat, "FERME");
        msg[12] = 0; // Commande fermeture

        for (int i = 0; i < 3; i++) {
            send_to_simulator_binary(sim_host, sim_port, msg, sizeof(msg));
            Sleep(50);
        }
    }

    /* 5. Génération HTML */
    html_header("Contrôle du Volet");
    printf("<div class='wrap'>");
    printf("<h2>Volet %s</h2>", decoded_device);
    printf("<p><em>IP automate :</em> %s<br><em>Entrée automate :</em> %s</p>",
           ip_auto[0] ? ip_auto : "(non défini)",
           input_auto[0] ? input_auto : "(non défini)");

    if (etat[0]) {
        printf("<p style='color:green;'>✅ Commande envoyée : <strong>%s</strong>. Nouvel état : <strong>%s</strong></p>",
               (strstr(qs, "action=on") ? "Ouvrir" : "Fermer"),
               etat);
    } else {
        printf("<p style='color:orange;'>⚠️ Aucune action spécifiée (on/off).</p>");
    }

    printf("<p><a href='/c/cuisine.html'>&larr; Retour au contrôle</a></p>");
    printf("</div>");
    html_footer();

    return 0;
}
