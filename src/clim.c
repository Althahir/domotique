#include "domotique.h"
#include "socket_client.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <windows.h>

/* ---------------------------------------------------------------------------
   FONCTIONS UTILITAIRES
   --------------------------------------------------------------------------- */

/* --- Décodage d'une chaîne URL-encodée (ex : "Nom%20Du%20Device" → "Nom Du Device")
   - %xx est converti en caractère ASCII (ex : %20 = espace)
   - les + sont remplacés par des espaces
   - tous les autres caractères sont copiés tels quels
*/
static void urldecode(char *dst, const char *src) {
    char a, b;
    while (*src) {
        if ((*src == '%') &&
            ((a = src[1]) && (b = src[2])) &&
            (isxdigit(a) && isxdigit(b))) { // si % suivi de deux caractères hexadécimaux
            a = (a <= '9') ? a - '0' : (tolower(a) - 'a' + 10);
            b = (b <= '9') ? b - '0' : (tolower(b) - 'a' + 10);
            *dst++ = 16 * a + b; // conversion hexadécimale vers un caractère
            src += 3;
        } else if (*src == '+') { // + → espace
            *dst++ = ' ';
            src++;
        } else {
            *dst++ = *src++; // caractère normal
        }
    }
    *dst = '\0';
}

/* --- Convertit une chaîne binaire "00010111" en tableau de 8 octets (0x00 ou 0x01)
   - Chaque caractère '1' devient 1, chaque '0' devient 0
   - Utilisé pour former la trame binaire à envoyer au simulateur
*/
static void input_to_bytes(const char *input, unsigned char *buffer) {
    for (int i = 0; i < 8; i++) buffer[i] = (input[i] == '1') ? 1 : 0;
}

/* --- Convertit une adresse IP texte "192.168.0.100" en 4 octets
   - Chaque segment de l’adresse devient un octet du buffer
*/
static void ip_to_bytes(const char *ip, unsigned char *buffer) {
    unsigned int a, b, c, d;
    sscanf(ip, "%u.%u.%u.%u", &a, &b, &c, &d);
    buffer[0] = (unsigned char)a;
    buffer[1] = (unsigned char)b;
    buffer[2] = (unsigned char)c;
    buffer[3] = (unsigned char)d;
}

/* ---------------------------------------------------------------------------
   GESTION DU FICHIER JSON D'ÉTAT (lecture / écriture)
   Ces fonctions manipulent le fichier "STATE_JSON_PATH" contenant les états
   et modes des appareils (ON/OFF, mode chaud/froid, etc.)
   --------------------------------------------------------------------------- */

/* --- Lecture complète d’un fichier en mémoire (retourne un buffer alloué dynamiquement) */
static char *read_file_all(const char *path, long *out_size) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);
    char *buf = (char*)malloc(size + 1);
    if (!buf) { fclose(f); return NULL; }
    fread(buf, 1, size, f);
    buf[size] = '\0';
    fclose(f);
    if (out_size) *out_size = size;
    return buf;
}

/* --- Écriture complète d’un buffer dans un fichier */
static int write_file_all(const char *path, const char *buf, size_t len) {
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    size_t w = fwrite(buf, 1, len, f);
    fclose(f);
    return (w == len);
}

/* --- Lecture du champ "mode" (0 ou 1) pour un appareil donné dans le JSON --- */
static int json_get_mode(const char *device, int *mode_out) {
    long sz = 0;
    char *json = read_file_all(STATE_JSON_PATH, &sz);
    if (!json) return 0;

    /* Recherche du bloc correspondant à l’appareil */
    char search[256];
    snprintf(search, sizeof(search), "\"%s\"", device);
    char *dev = strstr(json, search);
    if (!dev) { free(json); return 0; }

    /* Recherche de la clé "mode" dans ce bloc */
    char *block = strchr(dev, '{');
    if (!block) { free(json); return 0; }

    char *k = strstr(block, "\"mode\"");
    if (!k) { free(json); return 0; }
    k = strchr(k, ':');
    if (!k) { free(json); return 0; }
    k++;

    /* Ignore les espaces et sauts de ligne */
    while (*k == ' ' || *k == '\t' || *k == '\r' || *k == '\n') k++;

    /* Lecture du chiffre 0 ou 1 */
    if (*k == '0' || *k == '1') {
        *mode_out = (*k == '1') ? 1 : 0;
        free(json);
        return 1;
    }
    free(json);
    return 0;
}

/* --- Écriture du champ "mode" dans le JSON (met à jour le 0/1 existant) --- */
static int json_set_mode(const char *device, int mode_value) {
    long sz = 0;
    char *json = read_file_all(STATE_JSON_PATH, &sz);
    if (!json) return 0;

    /* Même logique que pour json_get_mode, mais on modifie le caractère */
    char search[256];
    snprintf(search, sizeof(search), "\"%s\"", device);
    char *dev = strstr(json, search);
    if (!dev) { free(json); return 0; }

    char *block = strchr(dev, '{');
    if (!block) { free(json); return 0; }

    char *k = strstr(block, "\"mode\"");
    if (!k) { free(json); return 0; }
    k = strchr(k, ':');
    if (!k) { free(json); return 0; }
    k++;
    while (*k == ' ' || *k == '\t' || *k == '\r' || *k == '\n') k++;

    /* Remplace la valeur du mode (0 ou 1) directement dans le texte JSON */
    if (*k == '0' || *k == '1') {
        *k = (mode_value == 1) ? '1' : '0';
        int ok = write_file_all(STATE_JSON_PATH, json, strlen(json));
        free(json);
        return ok;
    }

    free(json);
    return 0;
}

/* ---------------------------------------------------------------------------
   FONCTION PRINCIPALE
   Ce programme CGI est exécuté sur le serveur à chaque requête HTTP :
   Il reçoit une URL avec des paramètres (QUERY_STRING) et agit sur une climatisation.
   --------------------------------------------------------------------------- */
int main(void) {
    const char *group = "clim";             // Groupe d’appareils (ici, les climatiseurs)
    const char *sim_host = "192.168.56.1";  // Adresse IP du simulateur de la domotique
    int sim_port = get_simulator_port();    // Récupère le port configuré (dans un autre module)
    if (sim_port <= 0) sim_port = 49995;    // Valeur par défaut si non trouvée

    /* Variables pour les données extraites */
    char *qs = getenv("QUERY_STRING");      // Récupère la chaîne de requête (ex: ?device=Salon&action=on)
    char decoded_device[MAX_LEN] = "";      // Nom décodé de l’appareil
    char ip_auto[16] = "";                  // IP de l’automate associé
    char input_auto[9] = "";                // Entrées automates sous forme binaire
    char current_state[8] = "";             // État actuel (ON/OFF)

    /* (1) Récupère le nom du device depuis la Query String */
    if (qs) {
        char *p = strstr(qs, "device="); // Recherche le paramètre "device"
        if (p) {
            p += 7; // saute "device="
            char enc[MAX_LEN];
            char *end = strchr(p, '&'); // cherche le prochain paramètre
            if (end) { strncpy(enc, p, end - p); enc[end - p] = '\0'; }
            else { strcpy(enc, p); }
            urldecode(decoded_device, enc); // décodage des %20, etc.
        }
    }

    /* (2) Lit les informations de l’appareil depuis le JSON ou la config */
    if (!get_device_info(group, decoded_device, ip_auto, input_auto, current_state)) {
        // Si l’appareil n’existe pas → message d’erreur HTML
        // html_header("Erreur Clim");
        printf("<p style='color:red;'>❌ Climatisation \"%s\" introuvable (groupe %s).</p>", decoded_device, group);
        // html_footer();
        return 0;
    }

    /* (3) Récupère le mode courant dans le JSON (0 = froid, 1 = chaud) */
    int mode_val = 0;
    json_get_mode(decoded_device, &mode_val);

    /* (4) Prépare la trame binaire de 14 octets à envoyer au simulateur */
    unsigned char msg[14] = {0};
    ip_to_bytes(ip_auto, msg);           // [0..3] : IP de l’automate
    input_to_bytes(input_auto, msg + 4); // [4..11] : Entrées automates (8 bits)
    msg[12] = (strcmp(current_state, "ON") == 0) ? 1 : 0; // [12] : état actuel (1 = ON)
    msg[13] = (unsigned char)mode_val;                    // [13] : mode actuel (0 = clim, 1 = chauffage)

    /* (5) Traite l’action demandée dans l’URL */
    if (qs && strstr(qs, "action=on")) {
        // Allumer la climatisation
        set_device_state_json(group, decoded_device, "ON");
        msg[12] = 1;
    } else if (qs && strstr(qs, "action=off")) {
        // Éteindre la climatisation
        set_device_state_json(group, decoded_device, "OFF");
        msg[12] = 0;
    } else if (qs && strstr(qs, "action=cool")) {
        // Passer en mode froid
        json_set_mode(decoded_device, 0);
        msg[13] = 0;
    } else if (qs && strstr(qs, "action=heat")) {
        // Passer en mode chaud
        json_set_mode(decoded_device, 1);
        msg[13] = 1;
    }

    /* (6) Envoi de la trame binaire 3 fois au simulateur (fiabilité) */
    for (int i = 0; i < 3; i++) {
        send_to_simulator_binary(sim_host, sim_port, msg, sizeof(msg)); // envoi des 14 octets
        Sleep(50); // pause de 50 ms entre les envois
    }



    return 0;
}
