#ifndef SOCKET_CLIENT_H
#define SOCKET_CLIENT_H

/**
 * @file socket_client.h
 * @brief Déclarations des fonctions client TCP pour la communication avec le simulateur domotique.
 *
 * Ce module fournit deux fonctions permettant d’envoyer des trames binaires au simulateur :
 *  - `send_to_simulator()` : construit une trame à partir de chaînes texte (IP, bits, état)
 *  - `send_to_simulator_binary()` : envoie directement une trame déjà préparée en binaire
 *
 * Ces fonctions sont utilisées par les scripts CGI (`lampe.c`, `clim.c`, `scenario.c`, etc.)
 * pour transmettre les ordres d’allumage, d’extinction ou de changement de mode.
 *
 * ---
 * ⚙️ Exemple d’utilisation :
 * @code
 * unsigned char trame[13] = {192, 168, 0, 100, 1, 0, 0, 1, 1, 0, 0, 0, 1};
 * send_to_simulator_binary("192.168.56.1", 49995, trame, sizeof(trame));
 * @endcode
 * ---
 */

/**
 * @brief Envoie une trame TCP vers le simulateur à partir de paramètres texte.
 *
 * Cette fonction construit une trame binaire contenant :
 * - 4 octets pour l’adresse IP automate (ex : 192.168.0.100)
 * - 8 octets représentant les bits d’entrée automate (chaîne de 0 et 1)
 * - 1 octet d’état (1 = ON, 0 = OFF)
 *
 * @param host Adresse IP du simulateur (ex : "192.168.56.1")
 * @param port Port d’écoute du simulateur (ex : 49995)
 * @param ip_auto Adresse IP de l’automate (ex : "192.168.0.100")
 * @param input_auto Chaîne binaire sur 8 caractères représentant les entrées (ex : "00010111")
 * @param state État ON/OFF à envoyer
 * @return 1 en cas de succès, 0 en cas d’échec
 */
int send_to_simulator(const char *host, int port,
                      const char *ip_auto, const char *input_auto,
                      const char *state);

/**
 * @brief Envoie une trame binaire directement (sans la construire à partir de texte).
 *
 * Utilisée lorsque la trame est déjà prête (par ex. dans les modules `clim.c` ou `scenario.c`).
 * Cette fonction gère l’envoi complet même si `send()` n’envoie qu’une partie du buffer.
 *
 * @param host Adresse IP du simulateur
 * @param port Port TCP d’écoute du simulateur
 * @param data Pointeur vers le buffer contenant les données binaires à envoyer
 * @param len Taille (en octets) du buffer à transmettre
 * @return 1 si tous les octets ont été envoyés, 0 sinon
 */
int send_to_simulator_binary(const char *host, int port,
                             const unsigned char *data, size_t len);

#endif /* SOCKET_CLIENT_H */
