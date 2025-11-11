#include "domotique.h"        // Fonctions utilitaires : gestion du JSON, helpers HTML, etc.
#include "socket_client.h"    // Fonctions réseau pour communiquer avec le simulateur
#include <stdio.h>            // Fonctions d’entrée/sortie standard
#include <string.h>           // Manipulation de chaînes de caractères
#include <stdlib.h>           // Fonctions utilitaires (malloc, getenv, atoi, etc.)
#include <ctype.h>            // Fonctions de test de caractères (isxdigit, tolower, etc.)
#include <windows.h>          // Pour la fonction Sleep() (pause en millisecondes)

/* ---------------------------------------------------------------------------
   🔹 Fonction : urldecode()
   Décode une chaîne encodée en URL :
   - "%20" → espace
   - "%xx" → caractère ASCII correspondant
   - "+"   → espace
   Exemple : "Salon%20Est" devient "Salon Est"
   --------------------------------------------------------------------------- */
void urldecode(char *dst, const char *src) {
    char a, b;
    while (*src) {
        if ((*src == '%') &&
            ((a = src[1]) && (b = src[2])) &&
            (isxdigit(a) && isxdigit(b))) {        // si on rencontre % suivi de deux hexadécimaux
            a = (a <= '9') ? a - '0' : (tolower(a) - 'a' + 10);
            b = (b <= '9') ? b - '0' : (tolower(b) - 'a' + 10);
            *dst++ = 16 * a + b;                   // conversion en caractère ASCII
            src += 3;
        } else if (*src == '+') {                  // les + sont remplacés par des espaces
            *dst++ = ' ';
            src++;
        } else {
            *dst++ = *src++;                       // sinon copie directe du caractère
        }
    }
    *dst = '\0';                                   // fin de chaîne
}

/* ---------------------------------------------------------------------------
   🔹 Fonction : input_to_bytes()
   Convertit une chaîne binaire (ex: "00010111") en tableau de 8 octets (valeurs 0 ou 1)
   Chaque caractère '1' → 1, '0' → 0
   Sert à coder les 8 bits d’entrée de l’automate dans la trame envoyée.
   --------------------------------------------------------------------------- */
void input_to_bytes(const char *input, unsigned char *buffer) {
    for (int i = 0; i < 8 && input[i]; i++) {
        buffer[i] = (input[i] == '1') ? 1 : 0;
    }
}

/* ---------------------------------------------------------------------------
   🔹 Fonction : ip_to_bytes()
   Convertit une adresse IP "192.168.0.100" en quatre octets :
   [0] = 192, [1] = 168, [2] = 0, [3] = 100
   Ces octets seront intégrés à la trame binaire pour le simulateur.
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
   ⚙️ Programme principal
   Ce script CGI est exécuté par le serveur lorsqu’un utilisateur agit sur une lampe :
   - Allumage ou extinction via un bouton de l’IHM web
   - Mise à jour du fichier JSON
   - Envoi de la trame correspondante au simulateur
   - Affichage du résultat sous forme de page HTML
   --------------------------------------------------------------------------- */
int main(void) {
    const char *group = "lamps";          // Nom du groupe JSON concerné (section "lamps")
    char device[128] = "lamp1";           // Nom de la lampe par défaut
    char etat[MAX_LEN] = "";              // État actuel ("ON" ou "OFF")

    /* -----------------------------------------------------------------------
       1️⃣ Récupération du nom de l’appareil ("device") depuis la query string
       Exemple d’URL : /cgi-bin/lampe.exe?device=Salon&action=on
       ----------------------------------------------------------------------- */
    char *qs = getenv("QUERY_STRING");    // Lecture des paramètres d’URL passés par le serveur web
    if (qs) {
        char *p = strstr(qs, "device=");  // Recherche du paramètre "device"
        if (p) {
            p += strlen("device=");
            size_t i = 0;
            // Copie la valeur jusqu’à '&' ou fin de chaîne
            while (*p && *p != '&' && i < sizeof(device) - 1) {
                device[i++] = *p++;
            }
            device[i] = '\0';
        }
    }

    /* -----------------------------------------------------------------------
       2️⃣ Décodage de la valeur URL-encodée (ex: "%20" → espace)
       ----------------------------------------------------------------------- */
    char decoded_device[128];
    urldecode(decoded_device, device);

    /* -----------------------------------------------------------------------
       3️⃣ Lecture des informations de la lampe dans le fichier JSON
       (adresse IP automate, entrée automate, état courant)
       ----------------------------------------------------------------------- */
    char ip_auto[64] = "";
    char input_auto[64] = "";
    char current_state[32] = "";
    get_lamp_info(decoded_device, ip_auto, input_auto, current_state);

    /* -----------------------------------------------------------------------
       4️⃣ Configuration du simulateur (hôte et port)
       Récupération du port depuis le fichier JSON via get_simulator_port()
       Valeur de secours = 49995 si non défini.
       ----------------------------------------------------------------------- */
    const char *sim_host = "192.168.56.1";    // IP du simulateur domotique
    int sim_port = get_simulator_port();
    if (sim_port <= 0) sim_port = 49995;      // Défaut si port non trouvé


    /* -----------------------------------------------------------------------
       5️⃣ Construction de la trame binaire à envoyer au simulateur
       Format (13 octets) :
       [0–3]  : adresse IP (4 octets)
       [4–11] : entrées automate (8 bits)
       [12]   : état de la lampe (0 = OFF, 1 = ON)
       ----------------------------------------------------------------------- */
    unsigned char msg[13];
    memset(msg, 0, sizeof(msg));

    ip_to_bytes(ip_auto, msg);           // Encode l’adresse IP dans les 4 premiers octets
    input_to_bytes(input_auto, msg + 4); // Encode les bits automate à partir de l’octet 4


    /* -----------------------------------------------------------------------
       6️⃣ Traitement de l’action utilisateur (ON ou OFF)
       - Met à jour le fichier JSON (fonction set_device_state_json)
       - Prépare le message binaire (msg[12])
       - Envoie la trame au simulateur (3 fois pour fiabilité)
       ----------------------------------------------------------------------- */
    if (qs && strstr(qs, "action=on")) {
        set_device_state_json(group, decoded_device, "ON");  // Mise à jour JSON
        strcpy(etat, "ON");
        msg[12] = 1; // État ON

        // 🔁 Envoi 3 fois la trame binaire au simulateur
        for (int i = 0; i < 3; i++) {
            send_to_simulator_binary(sim_host, sim_port, msg, sizeof(msg));
            Sleep(50); // Pause 50 ms entre les envois
        }
    }
    else if (qs && strstr(qs, "action=off")) {
        set_device_state_json(group, decoded_device, "OFF");
        strcpy(etat, "OFF");
        msg[12] = 0; // État OFF

        for (int i = 0; i < 3; i++) {
            send_to_simulator_binary(sim_host, sim_port, msg, sizeof(msg));
            Sleep(50);
        }
    }

    /* -----------------------------------------------------------------------
       7️⃣ Génération de la page HTML de réponse (vue utilisateur)
       Affiche les infos du device, son IP, son entrée automate, et son état actuel.
       Fournit aussi des boutons d’action “Allumer” / “Éteindre”.
       ----------------------------------------------------------------------- */
    // html_header("Lampe");
    // printf("<div class='wrap'>");
    // printf("<h2>Lampe %s</h2>", decoded_device);

    // // Affiche les informations de l’automate associées à la lampe
    // printf("<p><em>IP automate :</em> %s<br><em>Entrée automate :</em> %s</p>",
    //        ip_auto[0] ? ip_auto : "(non défini)",
    //        input_auto[0] ? input_auto : "(non défini)");

    // // Affiche l’état actuel (ON en vert, OFF en rouge)
    // printf("<p>État actuel : <span class='%s'>%s</span></p>",
    //        (strcmp(etat, "ON") == 0) ? "on" : "off", etat);

    // // Boutons pour allumer / éteindre la lampe
    // printf("<p>");
    // printf("<a href='/cgi-bin/lampe.exe?device=%s&action=on'><button>Allumer</button></a>", device);
    // printf("<a href='/cgi-bin/lampe.exe?device=%s&action=off'><button>Éteindre</button></a>", device);
    // printf("</p>");

    // // Lien de retour vers l’interface utilisateur
    // printf("<p><a href='/c/index.html'><button>Retour IHM</button></a></p>");
    // printf("</div>");
    // html_footer();

    return 0;
}
