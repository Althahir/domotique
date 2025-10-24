#define _WIN32_WINNT 0x0600
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <string.h>
#include "socket_client.h"

#pragma comment(lib, "ws2_32.lib")

int send_to_simulator(const char *host, int port, const char *ip_auto, const char *input_auto, const char *state) {
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

    unsigned char buffer[32];
    int idx = 0;

    // 🔹 4 octets binaires de l'IP
    unsigned int a, b, c, d;
    sscanf(ip_auto, "%u.%u.%u.%u", &a, &b, &c, &d);
    buffer[idx++] = (unsigned char)a;
    buffer[idx++] = (unsigned char)b;
    buffer[idx++] = (unsigned char)c;
    buffer[idx++] = (unsigned char)d;

    // 🔹 8 bits d’entrée (0 ou 1)
    for (int i = 0; i < 8; i++) {
        buffer[idx++] = (input_auto[i] == '1') ? 1 : 0;
    }

    // 🔹 État : 1 pour ON, 0 pour OFF
    buffer[idx++] = (strcmp(state, "ON") == 0) ? 1 : 0;

    int sent = send(sock, (const char*)buffer, idx, 0);

    FILE *log = fopen("C:\\xampp\\htdocs\\c\\src\\debug.log", "a");
    if (log) {
        fprintf(log, "[TCP] %d octets binaires envoyés vers %s:%d : ", sent, host, port);
        for (int i = 0; i < idx; i++)
            fprintf(log, "%02X ", (unsigned char)buffer[i]);
        fprintf(log, "\n");
        fclose(log);
    }

    Sleep(100);
    closesocket(sock);
    WSACleanup();
    return 1;
}
