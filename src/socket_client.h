#ifndef SOCKET_CLIENT_H
#define SOCKET_CLIENT_H

/*
 * Envoi d’un message binaire au simulateur domotique via TCP
 * host : adresse IP du simulateur (ex: "192.168.56.1")
 * port : port d’écoute du simulateur (ex: 60804)
 * ip_auto : IP de l’automate (ex: "192.168.0.100")
 * input_auto : chaîne binaire sur 8 caractères (ex: "00010111")
 * state : "ON" ou "OFF"
 */
int send_to_simulator(const char *host, int port, const char *ip_auto, const char *input_auto, const char *state);
int send_to_simulator_binary(const char *host, int port, const unsigned char *data, size_t len);


#endif
