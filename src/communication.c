#include "domotique.h"  // Fichier d’en-tête principal contenant définitions globales et macros (ex: STATE_JSON_PATH)
#include <ctype.h>

/* ---------------------------------------------------------------------------
   🔹 Fonction utilitaire : lecture complète d’un fichier JSON en mémoire
   --------------------------------------------------------------------------- */
static char *read_json_file(const char *path) {
    FILE *f = fopen(path, "rb");           // Ouvre le fichier en mode binaire
    if (!f) return NULL;                   // Si erreur → NULL
    fseek(f, 0, SEEK_END);                 // Place le curseur à la fin
    long size = ftell(f);                  // Récupère la taille du fichier
    rewind(f);                             // Retour au début
    if (size < 0) { fclose(f); return NULL; }

    char *buf = malloc((size_t)size + 1);  // Alloue un buffer pour le contenu
    if (!buf) { fclose(f); return NULL; }

    fread(buf, 1, (size_t)size, f);        // Lit tout le fichier
    buf[size] = '\0';                      // Termine par un caractère nul
    fclose(f);
    return buf;                            // Retourne le texte JSON complet
}

/* ---------------------------------------------------------------------------
   🔹 Fonction utilitaire : écriture d’un texte dans un fichier
   --------------------------------------------------------------------------- */
static int write_json_file(const char *path, const char *buf) {
    FILE *f = fopen(path, "wb");           // Ouvre en écriture binaire
    if (!f) return 0;
    size_t len = strlen(buf);
    size_t wrote = fwrite(buf, 1, len, f); // Écrit le buffer entier
    fclose(f);
    return wrote == len;                   // Retourne 1 si tout a été écrit
}

/* ---------------------------------------------------------------------------
   🔹 Recherche d’un bloc "device" dans le JSON
   Fonction tolérante aux retours à la ligne et espaces
   Exemple : trouve la position du “{” du bloc d’un appareil donné.
   --------------------------------------------------------------------------- */
static char *find_device_block(char *json, const char *device) {
    char *d = strstr(json, device);        // Recherche le nom du device
    if (!d) return NULL;
    while (*d && *d != '{') d++;           // Avance jusqu’à la première accolade ouvrante
    if (*d != '{') return NULL;
    return d;                              // Retourne le pointeur vers le début du bloc
}

/* ---------------------------------------------------------------------------
   ⚙️  set_device_state_json() : Modifie la valeur "state" d’un appareil
   Exemple : "OFF" → "ON" dans le fichier JSON de l’état global
   --------------------------------------------------------------------------- */
int set_device_state_json(const char *group, const char *device, const char *state) {
    char path[260];
    snprintf(path, sizeof(path), "%s", STATE_JSON_PATH); // Chemin du JSON global

    // Ajout d’un message de debug (écrit dans un fichier log local)
    FILE *log = fopen("C:\\xampp\\htdocs\\c\\src\\debug.log", "a");
    if (log) {
        fprintf(log, "[DEBUG] Tentative d'ouverture du JSON: %s\n", path);
        fclose(log);
    }

    // Lecture du JSON
    char *json = read_json_file(path);
    if (!json) {
        FILE *log2 = fopen("C:\\xampp\\htdocs\\c\\src\\debug.log", "a");
        if (log2) {
            fprintf(log2, "[ERREUR] Impossible de lire le fichier JSON.\n");
            fclose(log2);
        }
        return 0;
    }

    /* Recherche du bloc de groupe (ex: "lamps", "clim", etc.) */
    char groupPattern[128];
    snprintf(groupPattern, sizeof(groupPattern), "\"%s\"", group);
    char *grp = strstr(json, groupPattern);
    if (!grp) { free(json); return 0; }
    grp = strchr(grp, '{');
    if (!grp) { free(json); return 0; }

    /* Délimite la fin du bloc du groupe (en comptant les accolades imbriquées) */
    char *grp_end = grp;
    int brace = 1;
    while (*++grp_end && brace > 0) {
        if (*grp_end == '{') brace++;
        else if (*grp_end == '}') brace--;
    }
    if (brace != 0) { free(json); return 0; }

    /* Cherche le bloc du device dans ce groupe */
    char *device_block = find_device_block(grp, device);
    if (!device_block) {
        FILE *log3 = fopen("C:\\xampp\\htdocs\\c\\src\\debug.log", "a");
        if (log3) {
            fprintf(log3, "[ERREUR] Device %s non trouvé dans JSON.\n", device);
            fclose(log3);
        }
        free(json);
        return 0;
    }

    /* Recherche la clé "state" dans le bloc du device */
    char *state_key = strstr(device_block, "\"state\"");
    if (!state_key || state_key > grp_end) { free(json); return 0; }

    /* Trouve la valeur après les deux points */
    char *val_start = strchr(state_key, ':');
    if (!val_start) { free(json); return 0; }
    val_start++;
    while (*val_start && isspace(*val_start)) val_start++;
    if (*val_start != '\"') { free(json); return 0; }
    val_start++;
    char *val_end = strchr(val_start, '\"');
    if (!val_end) { free(json); return 0; }

    /* Construit le nouveau JSON avec la valeur modifiée */
    size_t before_len = (size_t)(val_start - json);
    size_t after_len = strlen(val_end);
    char *new_json = malloc(before_len + strlen(state) + after_len + 2);
    if (!new_json) { free(json); return 0; }

    memcpy(new_json, json, before_len);
    memcpy(new_json + before_len, state, strlen(state));
    strcpy(new_json + before_len + strlen(state), val_end);

    int ok = write_json_file(path, new_json); // Écrit la nouvelle version du fichier

    // Log de confirmation
    FILE *log5 = fopen("C:\\xampp\\htdocs\\c\\src\\debug.log", "a");
    if (log5) {
        fprintf(log5, "[DEBUG] Mise à jour de %s -> %s (ok=%d)\n", device, state, ok);
        fclose(log5);
    }

    free(new_json);
    free(json);
    return ok;  // Retourne 1 si succès
}

/* ---------------------------------------------------------------------------
   ⚙️  get_device_state_json() : Lit l’état d’un appareil (ON/OFF)
   Retourne 1 si trouvé, sinon 0 (et remplit une valeur par défaut)
   --------------------------------------------------------------------------- */
int get_device_state_json(const char *group, const char *device, char *state_out, const char *default_state) {
    char path[260];
    snprintf(path, sizeof(path), "%s", STATE_JSON_PATH);

    char *json = read_json_file(path);
    if (!json) {
        strncpy(state_out, default_state, MAX_LEN - 1);
        state_out[MAX_LEN - 1] = '\0';
        return 0;
    }

    /* Recherche du bloc de groupe */
    char groupPattern[128];
    snprintf(groupPattern, sizeof(groupPattern), "\"%s\"", group);
    char *grp = strstr(json, groupPattern);
    if (!grp) {
        strncpy(state_out, default_state, MAX_LEN - 1);
        free(json);
        return 0;
    }
    grp = strchr(grp, '{');
    if (!grp) { strncpy(state_out, default_state, MAX_LEN - 1); free(json); return 0; }

    /* Recherche du device */
    char *device_block = find_device_block(grp, device);
    if (!device_block) {
        strncpy(state_out, default_state, MAX_LEN - 1);
        FILE *log3 = fopen("C:\\xampp\\htdocs\\c\\src\\debug.log", "a");
        if (log3) {
            fprintf(log3, "[ERREUR] Device %s non trouvé dans JSON (lecture)\n", device);
            fclose(log3);
        }
        free(json);
        return 0;
    }

    /* Lecture de la clé "state" */
    char *state_key = strstr(device_block, "\"state\"");
    if (!state_key) { strncpy(state_out, default_state, MAX_LEN - 1); free(json); return 0; }

    char *val_start = strchr(state_key, ':');
    if (!val_start) { free(json); return 0; }
    val_start++;
    while (*val_start && isspace(*val_start)) val_start++;
    if (*val_start != '\"') { free(json); return 0; }
    val_start++;
    char *val_end = strchr(val_start, '\"');
    if (!val_end) { free(json); return 0; }

    /* Copie la valeur dans le buffer de sortie */
    size_t len = val_end - val_start;
    if (len >= MAX_LEN) len = MAX_LEN - 1;
    memcpy(state_out, val_start, len);
    state_out[len] = '\0';
    free(json);
    return 1;
}

/* ---------------------------------------------------------------------------
   💡 get_lamp_info() : récupère ip / input / state pour un appareil "lampe"
   Cherche dans la section "lamps" du JSON
   --------------------------------------------------------------------------- */
int get_lamp_info(const char *device, char *ip_out, char *input_out, char *state_out) {
    FILE *debug = fopen("C:\\xampp\\htdocs\\c\\src\\debug.log", "a");

    char path[260];
    snprintf(path, sizeof(path), "%s", STATE_JSON_PATH);
    char *json = read_json_file(path);

    if (debug) {
        fprintf(debug, "\n=== DEBUG get_lamp_info ===\n");
        fprintf(debug, "Recherche du device : %s\n", device);
        fclose(debug);
    }
    if (!json) return 0;

    /* Recherche du groupe "lamps" */
    char *group = strstr(json, "\"lamps\"");
    if (!group) { free(json); return 0; }
    group = strchr(group, '{');
    if (!group) { free(json); return 0; }

    /* Recherche du bloc du device */
    char search[128];
    snprintf(search, sizeof(search), "\"%s\"", device);
    char *dev = strstr(group, search);
    if (!dev) {
        FILE *log = fopen("C:\\xampp\\htdocs\\c\\src\\debug.log", "a");
        if (log) { fprintf(log, "[ERREUR] get_lamp_info: device %s non trouvé.\n", device); fclose(log); }
        free(json);
        return 0;
    }

    char *block = strchr(dev, '{');
    if (!block) { free(json); return 0; }

    /* Macro pour extraire proprement une clé/valeur d’un bloc JSON */
    #define EXTRACT_VALUE(KEY, DEST) { \
        char *k = strstr(block, "\"" KEY "\""); \
        if (k) { \
            k = strchr(k, ':'); \
            if (k) { \
                k++; \
                while (*k && (*k == ' ' || *k == '\t' || *k == '\n' || *k == '\"')) k++; \
                char *end = k; \
                while (*end && *end != '\"' && *end != ',' && *end != '}') end++; \
                size_t len = end - k; \
                if (len > 0 && len < 128) { strncpy(DEST, k, len); DEST[len] = '\0'; } \
            } \
        } \
    }

    EXTRACT_VALUE("ip", ip_out);
    EXTRACT_VALUE("input", input_out);
    EXTRACT_VALUE("state", state_out);

    #undef EXTRACT_VALUE

    FILE *log = fopen("C:\\xampp\\htdocs\\c\\src\\debug.log", "a");
    if (log) {
        fprintf(log, "[INFO] Lamp %s -> ip=%s, input=%s, state=%s\n", device, ip_out, input_out, state_out);
        fclose(log);
    }

    free(json);
    return 1;
}

/* ---------------------------------------------------------------------------
   🧩 get_device_info() : fonction générique (pour clim, lampe, etc.)
   Retourne ip, input, state à partir du nom du groupe et du device
   --------------------------------------------------------------------------- */
int get_device_info(const char *group_name, const char *device, char *ip_out, char *input_out, char *state_out) {
    char path[260];
    snprintf(path, sizeof(path), "%s", STATE_JSON_PATH);
    char *json = read_json_file(path);
    if (!json) return 0;

    /* Recherche du bloc de groupe */
    char pattern[128];
    snprintf(pattern, sizeof(pattern), "\"%s\"", group_name);
    char *group = strstr(json, pattern);
    if (!group) { free(json); return 0; }
    group = strchr(group, '{');
    if (!group) { free(json); return 0; }

    /* Recherche du bloc du device */
    char search[128];
    snprintf(search, sizeof(search), "\"%s\"", device);
    char *dev = strstr(group, search);
    if (!dev) { free(json); return 0; }

    char *block = strchr(dev, '{');
    if (!block) { free(json); return 0; }

    #define EXTRACT_VALUE(KEY, DEST) { \
        char *k = strstr(block, "\"" KEY "\""); \
        if (k) { \
            k = strchr(k, ':'); \
            if (k) { \
                k++; \
                while (*k && (*k == ' ' || *k == '\t' || *k == '\n' || *k == '\"')) k++; \
                char *end = k; \
                while (*end && *end != '\"' && *end != ',' && *end != '}') end++; \
                size_t len = end - k; \
                if (len > 0 && len < 128) { strncpy(DEST, k, len); DEST[len] = '\0'; } \
            } \
        } \
    }

    EXTRACT_VALUE("ip", ip_out);
    EXTRACT_VALUE("input", input_out);
    EXTRACT_VALUE("state", state_out);
    #undef EXTRACT_VALUE

    FILE *log = fopen("C:\\xampp\\htdocs\\c\\src\\debug.log", "a");
    if (log) {
        fprintf(log, "[INFO] Device (%s) %s -> ip=%s, input=%s, state=%s\n", group_name, device, ip_out, input_out, state_out);
        fclose(log);
    }

    free(json);
    return 1;
}

/* ---------------------------------------------------------------------------
   ⚙️ get_simulator_port() : lit le port du simulateur depuis le JSON racine
   Extrait la clé "port" (nombre entier)
   --------------------------------------------------------------------------- */
int get_simulator_port(void) {
    char path[260];
    snprintf(path, sizeof(path), "%s", STATE_JSON_PATH);

    char *json = read_json_file(path);
    if (!json) return 0;

    char *k = strstr(json, "\"port\"");
    if (!k) { free(json); return 0; }

    k = strchr(k, ':');
    if (!k) { free(json); return 0; }
    k++;

    while (*k && isspace(*k)) k++;

    int port = atoi(k); // Convertit la valeur texte en entier

    FILE *log = fopen("C:\\xampp\\htdocs\\c\\src\\debug.log", "a");
    if (log) {
        fprintf(log, "[INFO] Port simulateur lu depuis JSON : %d\n", port);
        fclose(log);
    }

    free(json);
    return port;
}


