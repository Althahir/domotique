#define _WIN32_WINNT 0x0600
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <string.h>
#include "socket_client.h"

#pragma comment(lib, "ws2_32.lib")

/*
 * Envoi d’un message texte au simulateur domotique via TCP
 * Format attendu : "192.168.0.100 00010111 1\n"
 * (adresse IP, entrée automate, état numérique)
 */
int send_to_simulator(const char *host, int port, const char *ip_auto, const char *input_auto, const char *state) {
    WSADATA wsa;
    SOCKET sock;
    struct sockaddr_in server;
    int result;

    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return 0;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        WSACleanup();
        return 0;
    }

    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    result = inet_pton(AF_INET, host, &server.sin_addr);
    if (result <= 0) {
        closesocket(sock);
        WSACleanup();
        return 0;
    }

    if (connect(sock, (struct sockaddr*)&server, sizeof(server)) < 0) {
        FILE *log = fopen("C:\\xampp\\htdocs\\c\\src\\debug.log", "a");
        if (log) {
            fprintf(log, "[ERREUR] Connexion au simulateur impossible : %s:%d (%d)\n",
                    host, port, WSAGetLastError());
            fclose(log);
        }
        closesocket(sock);
        WSACleanup();
        return 0;
    }

    /* --- Format texte clair attendu par le simulateur --- */
    char buffer[128];
    snprintf(buffer, sizeof(buffer), "%s %s %d\n",
             ip_auto,
             input_auto,
             (strcmp(state, "ON") == 0) ? 1 : 0);

    int sent = send(sock, buffer, (int)strlen(buffer), 0);

    FILE *log = fopen("C:\\xampp\\htdocs\\c\\src\\debug.log", "a");
    if (log) {
        fprintf(log, "[TCP] Message texte envoyé → %s:%d : %s\n", host, port, buffer);
        fclose(log);
    }

    closesocket(sock);
    WSACleanup();
    return (sent > 0);
}
