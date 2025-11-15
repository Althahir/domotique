#define _WIN32_WINNT 0x0600       // Minimum Windows Vista pour inet_pton()
#include <winsock2.h>             // API réseau bas niveau Windows (socket, connect, etc.)
#include <windows.h>              // Sleep(), etc.
#include <ws2tcpip.h>             // inet_pton(), structures sockaddr_in, etc.
#include <stdio.h>                // printf, FILE, fprintf
#include <string.h>               // strcmp, memset
#include "socket_client.h"        // Déclaration des prototypes

#pragma comment(lib, "ws2_32.lib")  // Lien automatique avec la librairie Winsock2

/* ============================================================================
   ⚙️ MODULE : socket_client.c
   Ce module gère la communication réseau entre les scripts CGI et le simulateur
   domotique via TCP/IP.

   Il contient deux fonctions :
   1️⃣ send_to_simulator()        → construit une trame à partir de paramètres texte
   2️⃣ send_to_simulator_binary() → envoie directement une trame binaire préparée

   Chaque fonction :
   - établit une connexion TCP vers le simulateur
   - envoie les octets (binaire pur)
   - consigne l’opération dans un fichier debug.log
   ============================================================================ */


/* ---------------------------------------------------------------------------
   🔸 send_to_simulator()
   Construit et envoie une trame binaire à partir de :
   - l’adresse IP automate (ex: "192.168.0.100")
   - les 8 bits d’entrée automate (ex: "01010101")
   - l’état ON/OFF

   Structure de trame envoyée :
   [0–3]   → Adresse IP (4 octets)
   [4–11]  → Bits d’entrée automate (8 octets)
   [12]    → État de la lampe (1 octet, 0=OFF, 1=ON)
   --------------------------------------------------------------------------- */
int send_to_simulator(const char *host, int port, const char *ip_auto, const char *input_auto, const char *state) {
    WSADATA wsa;
    SOCKET sock;
    struct sockaddr_in server;

    // Initialisation de Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        return 0;

    // Création du socket TCP
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET)
        return 0;

    // Paramétrage du serveur distant
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    inet_pton(AF_INET, host, &server.sin_addr);

    // Connexion au simulateur
    if (connect(sock, (struct sockaddr*)&server, sizeof(server)) < 0) {
        closesocket(sock);
        WSACleanup();
        return 0;
    }

    // Construction de la trame binaire
    unsigned char buffer[32];
    int idx = 0;

    // 🔹 4 octets d’adresse IP (convertis en binaire)
    unsigned int a, b, c, d;
    sscanf(ip_auto, "%u.%u.%u.%u", &a, &b, &c, &d);
    buffer[idx++] = (unsigned char)a;
    buffer[idx++] = (unsigned char)b;
    buffer[idx++] = (unsigned char)c;
    buffer[idx++] = (unsigned char)d;

    // 🔹 8 bits d’entrée automate (chaîne "01010101" → octets 0/1)
    for (int i = 0; i < 8; i++) {
        buffer[idx++] = (input_auto[i] == '1') ? 1 : 0;
    }

    // 🔹 1 octet d’état (1 = ON / 0 = OFF)
    buffer[idx++] = (strcmp(state, "ON") == 0) ? 1 : 0;

    // Envoi TCP
    int sent = send(sock, (const char*)buffer, idx, 0);

    // 🔍 Journalisation dans un fichier de debug
    FILE *log = fopen("..\\data\\debug.log", "a");
    if (log) {
        fprintf(log, "[TCP] %d octets binaires envoyés vers %s:%d : ", sent, host, port);
        for (int i = 0; i < idx; i++)
            fprintf(log, "%02X ", (unsigned char)buffer[i]);
        fprintf(log, "\n");
        fclose(log);
    }

    // Fermeture
    Sleep(100);
    closesocket(sock);
    WSACleanup();
    return 1;
}


/* ---------------------------------------------------------------------------
   🔸 send_to_simulator_binary()
   Variante plus directe : reçoit déjà une trame binaire prête à être envoyée.
   Utilisée notamment dans `lampe.c` ou `scenario.c`.

   Avantages :
   - évite de reconstruire la trame côté CGI
   - gère l’envoi complet même si send() n’envoie qu’une partie des octets
   --------------------------------------------------------------------------- */
int send_to_simulator_binary(const char *host, int port, const unsigned char *data, size_t len) {
    WSADATA wsa;
    SOCKET sock;
    struct sockaddr_in server;

    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        return 0;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET)
        return 0;

    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    inet_pton(AF_INET, host, &server.sin_addr);

    if (connect(sock, (struct sockaddr*)&server, sizeof(server)) < 0) {
        closesocket(sock);
        WSACleanup();
        return 0;
    }

    // 🔹 Envoi fiable : boucle jusqu’à l’envoi complet de la trame
    size_t total_sent = 0;
    while (total_sent < len) {
        int sent = send(sock, (const char*)(data + total_sent), (int)(len - total_sent), 0);
        if (sent <= 0) break;
        total_sent += sent;
    }

    // 🔍 Log détaillé dans debug.log
    FILE *log = fopen("..\\data\\debug.log", "a");
    if (log) {
        fprintf(log, "[TCP] %zu/%zu octets envoyés (binaire direct) vers %s:%d : ",
                total_sent, len, host, port);
        for (size_t i = 0; i < len; i++)
            fprintf(log, "%02X ", data[i]);
        fprintf(log, "\n");
        fclose(log);
    }

    Sleep(100);
    closesocket(sock);
    WSACleanup();
    return (total_sent == len);
}
