#include "domotique.h"       // Gestion du JSON et gabarit HTML
#include "socket_client.h"   // Communication TCP vers le simulateur
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <windows.h>         // Pour Sleep()

/* ============================================================================
   ⚙️ MODULE : store.c
   Ce programme CGI gère l’ouverture et la fermeture des volets roulants.

   Il communique avec :
   - Le fichier JSON (pour lire et mettre à jour l’état du volet)
   - Le simulateur TCP (pour envoyer la commande binaire)

   Étapes principales :
   1️⃣ Lecture du paramètre `device` dans la requête HTTP (QUERY_STRING)
   2️⃣ Récupération de l’IP, de l’entrée automate et de l’état actuel dans le JSON
   3️⃣ Construction de la trame binaire (13 octets)
   4️⃣ Envoi de la commande vers le simulateur (3 répétitions)
   5️⃣ Génération d’une page HTML récapitulative
   ============================================================================ */


/* ---------------------------------------------------------------------------
   🔹 urldecode()
   Décodage d’une chaîne URL (ex : %20 → espace)
   Utilisée pour décoder le nom du périphérique passé dans la requête CGI.
   --------------------------------------------------------------------------- */
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

/* ---------------------------------------------------------------------------
   🔹 input_to_bytes()
   Convertit une chaîne binaire de 8 caractères ("00010111")
   en 8 octets (valeurs 0x00 ou 0x01).
   --------------------------------------------------------------------------- */
void input_to_bytes(const char *input, unsigned char *buffer) {
    for (int i = 0; i < 8; i++) {
        buffer[i] = (input[i] == '1') ? 1 : 0;
    }
}

/* ---------------------------------------------------------------------------
   🔹 ip_to_bytes()
   Convertit une adresse IPv4 texte (ex : "192.168.0.100") en 4 octets.
   --------------------------------------------------------------------------- */
void ip_to_bytes(const char *ip, unsigned char *buffer) {
    unsigned int a, b, c, d;
    sscanf(ip, "%u.%u.%u.%u", &a, &b, &c, &d);
    buffer[0] = (unsigned char)a;
    buffer[1] = (unsigned char)b;
    buffer[2] = (unsigned char)c;
    buffer[3] = (unsigned char)d;
}


/* ---------------------------------------------------------------------------
   🚀 PROGRAMME PRINCIPAL
   --------------------------------------------------------------------------- */
int main(void) {
    char *qs = getenv("QUERY_STRING");
    char *device_param = NULL;
    char decoded_device[MAX_LEN] = "";

    char ip_auto[16] = "";
    char input_auto[9] = "";
    char current_state[8] = "";

    const char *group = "store";        // Groupe dans le JSON
    const char *sim_host = "192.168.56.1";
    int sim_port = get_simulator_port(); // Lecture dynamique depuis le JSON
    if (sim_port <= 0) sim_port = 49995; // Valeur de secours


    /* -----------------------------------------------------------------------
       1️⃣ Lecture et décodage du paramètre `device`
       Exemple d’appel : /cgi-bin/store.exe?device=Volet_Salon&action=on
       ----------------------------------------------------------------------- */
    if (qs && (device_param = strstr(qs, "device="))) {
        device_param += 7;
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

    /* -----------------------------------------------------------------------
       2️⃣ Lecture des infos du périphérique depuis le JSON
       (IP automate, entrée automate, état actuel)
       ----------------------------------------------------------------------- */
    if (!get_device_info(group, decoded_device, ip_auto, input_auto, current_state)) {
        html_header("Erreur CGI");

        FILE *log = fopen("C:\\xampp\\htdocs\\c\\src\\debug.log", "a");
        if (log) {
            fprintf(log, "[ERREUR STORE.C] Appareil introuvable dans le groupe '%s' : %s\n",
                    group, decoded_device);
            fclose(log);
        }

        printf("<p style='color:red;'>❌ ERREUR : Appareil %s introuvable dans le JSON (groupe %s).</p>",
               decoded_device, group);
        html_footer();
        return 0;
    }

    /* -----------------------------------------------------------------------
       3️⃣ Construction de la trame binaire (13 octets)
       Structure :
       [0–3]  → IP automate
       [4–11] → Entrées automate
       [12]   → État (0 = fermé, 1 = ouvert)
       ----------------------------------------------------------------------- */
    unsigned char msg[13] = {0};
    ip_to_bytes(ip_auto, msg);
    input_to_bytes(input_auto, &msg[4]);

    char etat[16] = "";


    /* -----------------------------------------------------------------------
       4️⃣ Exécution de l’action demandée
       (on → ouvrir le volet / off → fermer le volet)
       ----------------------------------------------------------------------- */
    if (qs && strstr(qs, "action=on")) {
        set_device_state_json(group, decoded_device, "ON");
        strcpy(etat, "OUVERT");
        msg[12] = 1;

        // Envoi 3 fois la trame
        for (int i = 0; i < 3; i++) {
            send_to_simulator_binary(sim_host, sim_port, msg, sizeof(msg));
            Sleep(50);
        }
    }
    else if (qs && strstr(qs, "action=off")) {
        set_device_state_json(group, decoded_device, "OFF");
        strcpy(etat, "FERME");
        msg[12] = 0;

        for (int i = 0; i < 3; i++) {
            send_to_simulator_binary(sim_host, sim_port, msg, sizeof(msg));
            Sleep(50);
        }
    }
    return 0;
}
