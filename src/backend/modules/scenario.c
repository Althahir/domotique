#include "../core/domotique.h"      // Fonctions JSON et helpers HTML
#include "../core/socket_client.h"  // Fonctions réseau (client TCP pour le simulateur)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>       // API sockets Windows
#include <ws2tcpip.h>       // inet_pton, sockaddr_in
#include <windows.h>        // Sleep(), ZeroMemory(), etc.

/*
 * 🧠  DESCRIPTION GLOBALE :
 * Ce script CGI permet de lancer des scénarios domotiques complets.
 * Exemple : hiver → chauffage ON, lampes OFF, volets ouverts.
 *
 * Points clés :
 * - Lecture et écriture optimisées du JSON unique (devices.json)
 * - Envoi groupé des trames TCP (1 connexion par groupe)
 * - Gestion du simulateur domotique via socket TCP
 * - Compatible MinGW/TDM-GCC (grâce à inet_pton_compat)
 */

// ===========================================================================
// 🔸 urldecode() — Décode les paramètres d’URL (ex : "%20" → espace)
// ===========================================================================
void urldecode(char *dst, const char *src) {
    char a, b;
    while (*src) {
        if ((*src == '%') &&
            ((a = src[1]) && (b = src[2])) &&
            (isxdigit(a) && isxdigit(b))) {
            a = (a <= '9') ? a - '0' : (tolower(a) - 'a' + 10);
            b = (b <= '9') ? b - '0' : (tolower(b) - 'a' + 10);
            *dst++ = 16 * a + b;      // Décodage du caractère ASCII
            src += 3;
        } else if (*src == '+') {     // + = espace
            *dst++ = ' ';
            src++;
        } else {
            *dst++ = *src++;          // Copie directe sinon
        }
    }
    *dst = '\0';
}

// ===========================================================================
// 🔸 inet_pton_compat() — Version Windows-compatible d’inet_pton()
// ===========================================================================
int inet_pton_compat(int af, const char *src, void *dst) {
    struct sockaddr_storage ss;
    int sslen = sizeof(ss);
    char src_copy[INET6_ADDRSTRLEN + 1];
    ZeroMemory(&ss, sizeof(ss));
    strncpy(src_copy, src, INET6_ADDRSTRLEN);
    src_copy[INET6_ADDRSTRLEN] = 0;

    // Conversion texte → structure IP
    if (WSAStringToAddressA((LPSTR)src_copy, af, NULL, (struct sockaddr*)&ss, &sslen) == 0) {
        if (af == AF_INET)
            memcpy(dst, &((struct sockaddr_in*)&ss)->sin_addr, sizeof(struct in_addr));
        else
            memcpy(dst, &((struct sockaddr_in6*)&ss)->sin6_addr, sizeof(struct in6_addr));
        return 1;
    }
    return 0;
}

// ===========================================================================
// 🔸 Lecture et écriture du fichier JSON complet
// ===========================================================================
char *read_file_all(const char *path, long *out_size) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);
    char *buf = malloc(size + 1);
    fread(buf, 1, size, f);
    buf[size] = '\0';
    fclose(f);
    if (out_size) *out_size = size;
    return buf;
}

int write_file_all(const char *path, const char *buf) {
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    fwrite(buf, 1, strlen(buf), f);
    fclose(f);
    return 1;
}

// ===========================================================================
// 🔸 update_device_in_buffer()
// Met à jour les valeurs "state" et "mode" d’un appareil directement en RAM
// (pas besoin de recharger/écrire tout le JSON à chaque fois)
// ===========================================================================
void update_device_in_buffer(char *json, const char *group, const char *device, const char *state, int *mode) {
    char search_group[128];
    snprintf(search_group, sizeof(search_group), "\"%s\"", group);
    char *grp = strstr(json, search_group);
    if (!grp) return;
    grp = strchr(grp, '{');
    if (!grp) return;

    char search_device[256];
    snprintf(search_device, sizeof(search_device), "\"%s\"", device);
    char *dev = strstr(grp, search_device);
    if (!dev) return;

    // --- Mise à jour du champ "state" ---
    if (state) {
        char *k = strstr(dev, "\"state\"");
        if (k) {
            k = strchr(k, ':');
            if (k) {
                k++;
                while (*k == ' ' || *k == '\t' || *k == '\r' || *k == '\n' || *k == '\"') k++;
                char *end = strchr(k, '\"');
                if (end) {
                    size_t len_state = strlen(state);
                    if (len_state <= (size_t)(end - k)) {
                        memcpy(k, state, len_state);
                        for (size_t i = len_state; i < (size_t)(end - k); i++) k[i] = ' ';
                    }
                }
            }
        }
    }

    // --- Mise à jour du champ "mode" (uniquement pour la clim) ---
    if (mode) {
        char *k = strstr(dev, "\"mode\"");
        if (k) {
            k = strchr(k, ':');
            if (k) {
                k++;
                while (*k == ' ' || *k == '\t' || *k == '\r' || *k == '\n') k++;
                if (*k == '0' || *k == '1')
                    *k = (*mode == 1) ? '1' : '0';
            }
        }
    }
}

// ===========================================================================
// 🔸 send_group_to_simulator()
// Envoie un ensemble de trames TCP au simulateur via une seule connexion
// ===========================================================================
void send_group_to_simulator(const char *host, int port, unsigned char **frames, int frame_count, size_t frame_len) {
    WSADATA wsa;
    SOCKET sock;
    struct sockaddr_in server;

    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        return;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET)
        return;

    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    inet_pton_compat(AF_INET, host, &server.sin_addr);

    if (connect(sock, (struct sockaddr*)&server, sizeof(server)) < 0) {
        closesocket(sock);
        WSACleanup();
        return;
    }

    // Envoie successif de toutes les trames du groupe
    for (int i = 0; i < frame_count; i++) {
        send(sock, (const char*)frames[i], (int)frame_len, 0);
    }

    closesocket(sock);
    WSACleanup();
}

// ===========================================================================
// 🔸 perform_action_all()
// Applique une même action (ON/OFF, OPEN/CLOSE, etc.) à tous les appareils
// d’un groupe (lamps, store, clim).
// - Génère toutes les trames en mémoire (une par appareil)
// - Envoie le tout en une seule connexion TCP au simulateur
// - Met à jour le JSON en RAM
// ===========================================================================

void perform_action_all(char *json, const char *group, const char *action,
                        const char *mode, const char *sim_host, int sim_port)
{
    char pattern[128];
    snprintf(pattern, sizeof(pattern), "\"%s\"", group);
    char *grp = strstr(json, pattern);
    if (!grp) return;
    grp = strchr(grp, '{');
    if (!grp) return;

    unsigned char *frames[1000];   // tableau des trames à envoyer
    int frame_count = 0;
    size_t frame_len = 13;         // longueur par défaut

    char *p = grp;
    while ((p = strchr(p, '\"')) != NULL) {
        p++;
        char name[256];
        int i = 0;
        while (*p && *p != '\"' && i < 255) name[i++] = *p++;
        name[i] = '\0';
        if (strlen(name) < 3) continue;

        char ip[32] = "", input[16] = "", state[8] = "";
        if (!get_device_info(group, name, ip, input, state))
            continue;

        unsigned char *msg = calloc(1, 14);   // alloue et initialise à 0
        if (!msg) continue;

        // -- Adresse IP (4 octets)
        unsigned int a,b,c,d;
        sscanf(ip, "%u.%u.%u.%u", &a,&b,&c,&d);
        msg[0]=a; msg[1]=b; msg[2]=c; msg[3]=d;

        // -- Bits automate (8 octets)
        for (int k=0; k<8; k++)
            msg[4+k] = (input[k]=='1') ? 1 : 0;

        /* --- Groupe "lamps" --- */
        if (strcmp(group, "lamps")==0) {
            const char *new_state = (strcmp(action,"on")==0) ? "ON":"OFF";
            update_device_in_buffer(json, group, name, new_state, NULL);
            msg[12] = (strcmp(action,"on")==0) ? 1:0;
            frame_len = 13;
        }

        /* --- Groupe "store" (volets) --- */
        else if (strcmp(group,"store")==0) {
            const char *new_state = (strcmp(action,"open")==0) ? "ON":"OFF";
            update_device_in_buffer(json, group, name, new_state, NULL);
            msg[12] = (strcmp(action,"open")==0) ? 1:0;
            frame_len = 13;
        }

        /* --- Groupe "clim" --- */
        else if (strcmp(group,"clim")==0) {
            int mode_val = 0;
            if (mode && strcmp(mode,"heat")==0) mode_val = 1;
            else if (mode && strcmp(mode,"cool")==0) mode_val = 0;

            // 🔧 Correction : toujours envoyer 14 octets, même pour OFF
            msg[12] = (strcmp(action,"on")==0) ? 1 : 0;
            msg[13] = mode_val;  // cohérence trame
            frame_len = 14;

            if (strcmp(action,"on")==0)
                update_device_in_buffer(json, group, name, "ON", &mode_val);
            else if (strcmp(action,"off")==0)
                update_device_in_buffer(json, group, name, "OFF", NULL);
        }

        frames[frame_count++] = msg;
        if (frame_count >= 1000) break;
    }

    // -- Envoi global du lot de trames
    if (frame_count > 0)
        send_group_to_simulator(sim_host, sim_port, frames, frame_count, frame_len);

    // -- Libération mémoire
    for (int i=0; i<frame_count; i++)
        free(frames[i]);
}


// ===========================================================================
// 🔸 main()
// Point d’entrée principal — exécute un scénario complet (été, hiver, etc.)
// ===========================================================================
int main(void) {
    printf("Content-type: text/plain\r\n\r\n");  // Sortie HTTP texte brut

    // Lecture des paramètres URL
    char *qs = getenv("QUERY_STRING");
    if (!qs) {
        printf("Erreur : aucun paramètre.\n");
        fflush(stdout); fclose(stdout);
        return 0;
    }

    // Extraction du paramètre "type="
    char *type_param = strstr(qs, "type=");
    if (!type_param) {
        printf("Erreur : scénario non spécifié.\n");
        fflush(stdout); fclose(stdout);
        return 0;
    }

    type_param += 5;
    char decoded[64];
    urldecode(decoded, type_param);  // Décodage URL

    // Configuration du simulateur
    const char *sim_host = "192.168.56.1";
    int sim_port = get_simulator_port();
    if (sim_port <= 0) sim_port = 49995;

    // Lecture du JSON principal
    long json_size = 0;
    char *json = read_file_all(STATE_JSON_PATH, &json_size);
    if (!json) {
        printf("Erreur : impossible de lire le fichier JSON.\n");
        fflush(stdout); fclose(stdout);
        return 0;
    }

    printf("=== Exécution du scénario : %s ===\n", decoded);

    // Scénarios prédéfinis
    if (strcmp(decoded, "winter") == 0) {
        printf("Matin d'hiver → Clim chaud, lumières off, volets ouverts\n");
        perform_action_all(json, "clim", "on", "heat", sim_host, sim_port);
        perform_action_all(json, "lamps", "off", NULL, sim_host, sim_port);
        perform_action_all(json, "store", "open", NULL, sim_host, sim_port);
    }
    else if (strcmp(decoded, "summer") == 0) {
        printf("Matin d'été → Clim froid, lumières off, volets ouverts\n");
        perform_action_all(json, "clim", "on", "cool", sim_host, sim_port);
        perform_action_all(json, "lamps", "off", NULL, sim_host, sim_port);
        perform_action_all(json, "store", "open", NULL, sim_host, sim_port);
    }
    else if (strcmp(decoded, "vacation") == 0) {
        printf("Départ vacances → tout off + volets fermés\n");
        perform_action_all(json, "clim", "off", NULL, sim_host, sim_port);
        perform_action_all(json, "lamps", "off", NULL, sim_host, sim_port);
        perform_action_all(json, "store", "close", NULL, sim_host, sim_port);
    }
    else if (strcmp(decoded, "evening") == 0) {
        printf("Soirée → lumières on, volets fermés, chauffage off\n");
        perform_action_all(json, "clim", "off", NULL, sim_host, sim_port);
        perform_action_all(json, "lamps", "on", NULL, sim_host, sim_port);
        perform_action_all(json, "store", "close", NULL, sim_host, sim_port);
    }
    else {
        printf("Scénario inconnu : %s\n", decoded);
    }

    // Écriture du JSON mis à jour
    write_file_all(STATE_JSON_PATH, json);
    free(json);

    printf("\n✅ Scénario terminé.\n");
    fflush(stdout);
    fclose(stdout);
    return 0;
}
