#ifndef DOMOTIQUE_H     // Empêche les inclusions multiples du même fichier
#define DOMOTIQUE_H

#include <stdio.h>      // Pour les fonctions d’entrée/sortie standard (printf, FILE, fopen, etc.)
#include <stdlib.h>     // Pour malloc, free, atoi, etc.
#include <string.h>     // Pour strcmp, strncpy, strstr, etc.

/* ---------------------------------------------------------------------------
   🔧 Définition des chemins et fichiers de configuration
   Ces macros définissent l’emplacement du fichier d’état JSON principal
   --------------------------------------------------------------------------- */
#define STATE_DIR "C:\\xampp\\htdocs\\c\\src\\"   // Dossier où sont stockés les fichiers du serveur
#define STATE_JSON "devices.json"                 // Nom du fichier contenant les états et infos des appareils
#define STATE_JSON_PATH STATE_DIR STATE_JSON      // Chemin complet (concaténation des deux précédents)

/* ---------------------------------------------------------------------------
   🔢 Constantes globales
   --------------------------------------------------------------------------- */
#define MAX_LEN 256           // Taille maximale des chaînes de caractères manipulées
#define READ_BUF_SIZE 16384   // Taille du buffer de lecture lors du chargement des fichiers JSON

/* ---------------------------------------------------------------------------
   💾 Fonctions liées à la persistance JSON
   (lecture/écriture des états des appareils)
   --------------------------------------------------------------------------- */

/**
 * set_device_state_json()
 * Met à jour dans le fichier JSON l’état d’un appareil (ON/OFF)
 * @param group : nom du groupe (ex: "lamps" ou "clim")
 * @param device : nom de l’appareil (ex: "salon", "cuisine")
 * @param state : nouvelle valeur ("ON" ou "OFF")
 * @return 1 si succès, 0 sinon
 */
int set_device_state_json(const char *group, const char *device, const char *state);

/**
 * get_device_state_json()
 * Récupère l’état d’un appareil depuis le JSON
 * @param group : nom du groupe
 * @param device : nom de l’appareil
 * @param state_out : buffer où sera stocké le résultat
 * @param default_state : valeur par défaut si non trouvé
 * @return 1 si trouvé, 0 sinon
 */
int get_device_state_json(const char *group, const char *device, char *state_out, const char *default_state);

/**
 * get_lamp_info()
 * Récupère les infos spécifiques à une lampe :
 *  - adresse IP de l’automate
 *  - entrée automate
 *  - état actuel
 * @return 1 si succès, 0 sinon
 */
int get_lamp_info(const char *device, char *ip_out, char *input_out, char *state_out);

/**
 * get_simulator_port()
 * Lit dans le JSON global la clé "port" utilisée par le simulateur.
 * @return numéro de port du simulateur ou 0 si échec
 */
int get_simulator_port(void);

/* ---------------------------------------------------------------------------
   🧩 Fonction générique pour tout type d’appareil
   (Ajoutée pour mutualiser le code entre lampes, clim, etc.)
   --------------------------------------------------------------------------- */

/**
 * get_device_info()
 * Récupère les 3 informations principales d’un appareil :
 *  - son IP d’automate
 *  - son entrée automate (bits)
 *  - son état actuel (ON/OFF)
 * Fonctionne pour n’importe quel groupe (lamps, clim, etc.)
 */
int get_device_info(const char *group_name, const char *device, char *ip_out, char *input_out, char *state_out);

/* ---------------------------------------------------------------------------
   🌐 Fonctions utilitaires pour la génération HTML
   Ces fonctions servent à générer les entêtes et pieds de page
   dans les réponses CGI renvoyées au navigateur.
   --------------------------------------------------------------------------- */

/**
 * html_header()
 * Écrit les entêtes HTTP + le début du code HTML standard
 * (doctype, <html>, <head>, etc.)
 */
void html_header(const char *title);

/**
 * html_footer()
 * Termine la page HTML ouverte par html_header()
 */
void html_footer(void);

#endif  // DOMOTIQUE_H
