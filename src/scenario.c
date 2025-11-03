#include "domotique.h"
#include "socket_client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

/*
 * Version optimisée avec envoi groupé des trames TCP.
 * Compatible MinGW : inet_pton remplacé par inet_pton_compat().
 * Une seule connexion TCP par type d’appareil (lamps, store, clim).
 * JSON lu/écrit une seule fois.
 * Sortie libérée proprement (fflush/fclose).
 */
// ---------------------------------------------------------------------------
// Décode une chaîne URL (ex: %20 -> espace)
// ---------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------
// Fonction de compatibilité pour inet_pton (pour MinGW / TDM-GCC)
// ---------------------------------------------------------------------------
int inet_pton_compat(int af, const char *src, void *dst) {
    struct sockaddr_storage ss;
    int sslen = sizeof(ss);
    char src_copy[INET6_ADDRSTRLEN + 1];
    ZeroMemory(&ss, sizeof(ss));
    strncpy(src_copy, src, INET6_ADDRSTRLEN);
    src_copy[INET6_ADDRSTRLEN] = 0;

    if (WSAStringToAddressA((LPSTR)src_copy, af, NULL, (struct sockaddr*)&ss, &sslen) == 0) {
        if (af == AF_INET)
            memcpy(dst, &((struct sockaddr_in*)&ss)->sin_addr, sizeof(struct in_addr));
        else
            memcpy(dst, &((struct sockaddr_in6*)&ss)->sin6_addr, sizeof(struct in6_addr));
        return 1;
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Lecture / écriture du fichier JSON
// ---------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------
// Met à jour "state" et "mode" d’un appareil dans le buffer JSON
// ---------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------
// Envoi groupé des trames TCP (1 connexion pour tout un groupe)
// ---------------------------------------------------------------------------
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

    for (int i = 0; i < frame_count; i++) {
        send(sock, (const char*)frames[i], (int)frame_len, 0);
    }

    closesocket(sock);
    WSACleanup();
}

// ---------------------------------------------------------------------------
// Applique une action à tous les appareils d’un groupe
// ---------------------------------------------------------------------------
void perform_action_all(char *json, const char *group, const char *action, const char *mode, const char *sim_host, int sim_port) {
    char pattern[128];
    snprintf(pattern, sizeof(pattern), "\"%s\"", group);
    char *grp = strstr(json, pattern);
    if (!grp) return;
    grp = strchr(grp, '{');
    if (!grp) return;

    unsigned char *frames[1000];
    int frame_count = 0;
    size_t frame_len = 13; // par défaut

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

        unsigned char *msg = malloc(14);
        memset(msg, 0, 14);

        unsigned int a, b, c, d;
        sscanf(ip, "%u.%u.%u.%u", &a, &b, &c, &d);
        msg[0] = a; msg[1] = b; msg[2] = c; msg[3] = d;

        for (int k = 0; k < 8; k++)
            msg[4 + k] = (input[k] == '1') ? 1 : 0;

        /* Lamps */
        if (strcmp(group, "lamps") == 0) {
            const char *new_state = (strcmp(action, "on") == 0) ? "ON" : "OFF";
            update_device_in_buffer(json, group, name, new_state, NULL);
            msg[12] = (strcmp(action, "on") == 0) ? 1 : 0;
            frame_len = 13;
        }
        /* Stores */
        else if (strcmp(group, "store") == 0) {
            const char *new_state = (strcmp(action, "open") == 0) ? "ON" : "OFF";
            update_device_in_buffer(json, group, name, new_state, NULL);
            msg[12] = (strcmp(action, "open") == 0) ? 1 : 0;
            frame_len = 13;
        }
        /* Clims */
        else if (strcmp(group, "clim") == 0) {
            int mode_val = 0;
            if (mode && strcmp(mode, "heat") == 0) mode_val = 1;
            else if (mode && strcmp(mode, "cool") == 0) mode_val = 0;

            if (strcmp(action, "on") == 0) {
                update_device_in_buffer(json, group, name, "ON", &mode_val);
                msg[12] = 1;
                msg[13] = mode_val;
                frame_len = 14;
            } else if (strcmp(action, "off") == 0) {
                update_device_in_buffer(json, group, name, "OFF", NULL);
                msg[12] = 0;
                frame_len = 13;
            }
        }

        frames[frame_count++] = msg;
        if (frame_count >= 1000) break;
    }

    if (frame_count > 0)
        send_group_to_simulator(sim_host, sim_port, frames, frame_count, frame_len);

    for (int i = 0; i < frame_count; i++)
        free(frames[i]);
}

// ---------------------------------------------------------------------------
// Programme principal
// ---------------------------------------------------------------------------
int main(void) {
    printf("Content-type: text/plain\r\n\r\n");

    char *qs = getenv("QUERY_STRING");
    if (!qs) {
        printf("Erreur : aucun paramètre.\n");
        fflush(stdout); fclose(stdout);
        return 0;
    }

    char *type_param = strstr(qs, "type=");
    if (!type_param) {
        printf("Erreur : scénario non spécifié.\n");
        fflush(stdout); fclose(stdout);
        return 0;
    }

    type_param += 5;
    char decoded[64];
    urldecode(decoded, type_param);

    const char *sim_host = "192.168.56.1";
    int sim_port = get_simulator_port();
    if (sim_port <= 0) sim_port = 49995;

    long json_size = 0;
    char *json = read_file_all(STATE_JSON_PATH, &json_size);
    if (!json) {
        printf("Erreur : impossible de lire le fichier JSON.\n");
        fflush(stdout); fclose(stdout);
        return 0;
    }

    printf("=== Exécution du scénario : %s ===\n", decoded);

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
        printf("Soirée → lumières off, volets fermés, chauffage off\n");
        perform_action_all(json, "clim", "off", NULL, sim_host, sim_port);
        perform_action_all(json, "lamps", "on", NULL, sim_host, sim_port);
        perform_action_all(json, "store", "close", NULL, sim_host, sim_port);
    }

    else {
        printf("Scénario inconnu : %s\n", decoded);
    }

    write_file_all(STATE_JSON_PATH, json);
    free(json);

    printf("\n✅ Scénario terminé.\n");
    fflush(stdout);
    fclose(stdout);
    return 0;
}
