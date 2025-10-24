#include "domotique.h"

/* Lire tout le fichier JSON dans un buffer (malloc) */
static char *read_json_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);
    if (size < 0) { fclose(f); return NULL; }
    char *buf = malloc((size_t)size + 1);
    if (!buf) { fclose(f); return NULL; }
    fread(buf, 1, (size_t)size, f);
    buf[size] = '\0';
    fclose(f);
    return buf;
}

/* Écrire du texte dans un fichier */
static int write_json_file(const char *path, const char *buf) {
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    size_t len = strlen(buf);
    size_t wrote = fwrite(buf, 1, len, f);
    fclose(f);
    return wrote == len;
}

/* --- Fonction robuste : recherche d’un bloc device (indépendante des retours à la ligne) --- */
static char *find_device_block(char *json, const char *device) {
    char *d = strstr(json, device);
    if (!d) return NULL;
    while (*d && *d != '{') d++;  // avance jusqu’à la première accolade
    if (*d != '{') return NULL;
    return d;
}

/* Modifier l’état d’un device */
int set_device_state_json(const char *group, const char *device, const char *state) {
    char path[260];
    snprintf(path, sizeof(path), "%s", STATE_JSON_PATH);

    FILE *log = fopen("C:\\xampp\\htdocs\\c\\src\\debug.log", "a");
    if (log) {
        fprintf(log, "[DEBUG] Tentative d'ouverture du JSON: %s\n", path);
        fclose(log);
    }

    char *json = read_json_file(path);
    if (!json) {
        FILE *log2 = fopen("C:\\xampp\\htdocs\\c\\src\\debug.log", "a");
        if (log2) {
            fprintf(log2, "[ERREUR] Impossible de lire le fichier JSON.\n");
            fclose(log2);
        }
        return 0;
    }

    /* Localiser le bloc group */
    char groupPattern[128];
    snprintf(groupPattern, sizeof(groupPattern), "\"%s\"", group);
    char *grp = strstr(json, groupPattern);
    if (!grp) { free(json); return 0; }
    grp = strchr(grp, '{');
    if (!grp) { free(json); return 0; }

    /* Limiter la recherche au bloc du groupe */
    char *grp_end = grp;
    int brace = 1;
    while (*++grp_end && brace > 0) {
        if (*grp_end == '{') brace++;
        else if (*grp_end == '}') brace--;
    }
    if (brace != 0) { free(json); return 0; }

    /* Cherche le bloc device */
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

    /* Cherche la clé "state" */
    char *state_key = strstr(device_block, "\"state\"");
    if (!state_key || state_key > grp_end) { free(json); return 0; }

    char *val_start = strchr(state_key, ':');
    if (!val_start) { free(json); return 0; }
    val_start++;
    while (*val_start && (*val_start == ' ' || *val_start == '\t' || *val_start == '\r' || *val_start == '\n')) val_start++;
    if (*val_start != '\"') { free(json); return 0; }
    val_start++;
    char *val_end = strchr(val_start, '\"');
    if (!val_end) { free(json); return 0; }

    /* Remplacement */
    size_t before_len = (size_t)(val_start - json);
    size_t after_len = strlen(val_end);
    char *new_json = malloc(before_len + strlen(state) + after_len + 2);
    if (!new_json) { free(json); return 0; }

    memcpy(new_json, json, before_len);
    memcpy(new_json + before_len, state, strlen(state));
    strcpy(new_json + before_len + strlen(state), val_end);

    int ok = write_json_file(path, new_json);

    FILE *log5 = fopen("C:\\xampp\\htdocs\\c\\src\\debug.log", "a");
    if (log5) {
        fprintf(log5, "[DEBUG] Mise à jour de %s -> %s (ok=%d)\n", device, state, ok);
        fclose(log5);
    }

    free(new_json);
    free(json);
    return ok;
}

/* Lire l’état d’un device */
int get_device_state_json(const char *group, const char *device, char *state_out, const char *default_state) {
    char path[260];
    snprintf(path, sizeof(path), "%s", STATE_JSON_PATH);

    char *json = read_json_file(path);
    if (!json) {
        strncpy(state_out, default_state, MAX_LEN - 1);
        state_out[MAX_LEN - 1] = '\0';
        return 0;
    }

    char groupPattern[128];
    snprintf(groupPattern, sizeof(groupPattern), "\"%s\"", group);
    char *grp = strstr(json, groupPattern);
    if (!grp) {
        strncpy(state_out, default_state, MAX_LEN - 1);
        state_out[MAX_LEN - 1] = '\0';
        free(json);
        return 0;
    }
    grp = strchr(grp, '{');
    if (!grp) { strncpy(state_out, default_state, MAX_LEN - 1); free(json); return 0; }

    /* Chercher bloc device */
    char *device_block = find_device_block(grp, device);
    if (!device_block) {
        strncpy(state_out, default_state, MAX_LEN - 1);
        state_out[MAX_LEN - 1] = '\0';
        FILE *log3 = fopen("C:\\xampp\\htdocs\\c\\src\\debug.log", "a");
        if (log3) {
            fprintf(log3, "[ERREUR] Device %s non trouvé dans JSON (lecture)\n", device);
            fclose(log3);
        }
        free(json);
        return 0;
    }

    /* Lire la clé "state" */
    char *state_key = strstr(device_block, "\"state\"");
    if (!state_key) {
        strncpy(state_out, default_state, MAX_LEN - 1);
        state_out[MAX_LEN - 1] = '\0';
        free(json);
        return 0;
    }
    char *val_start = strchr(state_key, ':');
    if (!val_start) { free(json); return 0; }
    val_start++;
    while (*val_start && (*val_start == ' ' || *val_start == '\t' || *val_start == '\r' || *val_start == '\n')) val_start++;
    if (*val_start != '\"') { free(json); return 0; }
    val_start++;
    char *val_end = strchr(val_start, '\"');
    if (!val_end) { free(json); return 0; }

    size_t len = val_end - val_start;
    if (len >= MAX_LEN) len = MAX_LEN - 1;
    memcpy(state_out, val_start, len);
    state_out[len] = '\0';
    free(json);
    return 1;
}

/* ------------------------------------------------------------ */
/* 🔍 Nouvelle fonction : récupération IP + input + state */
/* Récupère les infos d’une lampe : ip, input, state */
int get_lamp_info(const char *device, char *ip_out, char *input_out, char *state_out) {
    FILE *debug = fopen("C:\\xampp\\htdocs\\c\\src\\debug.log", "a");

    char path[260];
    snprintf(path, sizeof(path), "%s", STATE_JSON_PATH);
    char *json = read_json_file(path);
    if (debug) {
    fprintf(debug, "\n=== DEBUG get_lamp_info ===\n");
    fprintf(debug, "Recherche du device : %s\n", device);
    // fprintf(debug, "Contenu début du JSON (200 premiers caractères) :\n%.200s\n", json);
    fclose(debug);
}
    if (!json) return 0;

    /* Cherche la section "lamps" */
    char *group = strstr(json, "\"lamps\"");
    if (!group) { free(json); return 0; }
    group = strchr(group, '{');
    if (!group) { free(json); return 0; }

    /* Cherche le bloc du device */
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

    /* Utilitaire pour extraire une valeur clé/valeur */
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


/* ------------------------------------------------------------ */
/* HTML helpers */
void html_header(const char *title) {
    printf("Content-type: text/html\r\n\r\n");
    printf("<!DOCTYPE html><html lang='fr'><head><meta charset='utf-8'>");
    printf("<title>%s</title>", title ? title : "Domotique");
    printf("<style>"
           "body{font-family:Arial; text-align:center; padding:20px}"
           "button{margin:8px; padding:10px 18px; font-size:15px}"
           ".on{color:green; font-weight:bold}"
           ".off{color:#c00; font-weight:bold}"
           ".wrap{display:inline-block; padding:16px; border:1px solid #ddd; border-radius:10px}"
           "</style></head><body>");
}

void html_footer(void) {
    printf("</body></html>");
}
