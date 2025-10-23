#include "domotique.h"

/* Helper : lire tout le fichier JSON dans un buffer (malloc) */
static char *read_json_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);
    if (size < 0) { fclose(f); return NULL; }
    char *buf = malloc((size_t)size + 1);
    if (!buf) { fclose(f); return NULL; }
    if (fread(buf, 1, (size_t)size, f) == 0 && size > 0) {
        free(buf); fclose(f); return NULL;
    }
    buf[size] = '\0';
    fclose(f);
    return buf;
}

/* Helper : write buffer to file (overwrite) */
static int write_json_file(const char *path, const char *buf) {
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    size_t len = strlen(buf);
    size_t wrote = fwrite(buf, 1, len, f);
    fclose(f);
    return wrote == len;
}

/* Fonction pour supprimer espaces et retours à la ligne (tolérance format JSON) */
static void compact_json(const char *src, char *dst, size_t max) {
    size_t j = 0;
    for (size_t i = 0; src[i] && j < max - 1; i++) {
        if (src[i] != '\n' && src[i] != '\r' && src[i] != '\t' && src[i] != ' ')
            dst[j++] = src[i];
    }
    dst[j] = '\0';
}

/* Remplacement textuel d’un device dans un JSON simple */
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

    /* Compactage pour éviter les échecs dus aux espaces / retours à la ligne */
    char compact[65536];
    compact_json(json, compact, sizeof(compact));

    char search_group[128];
    snprintf(search_group, sizeof(search_group), "\"%s\":{", group);
    char *grp = strstr(compact, search_group);
    if (!grp) {
        FILE *logg = fopen("C:\\xampp\\htdocs\\c\\src\\debug.log", "a");
        if (logg) {
            fprintf(logg, "[ERREUR] Groupe %s non trouvé dans JSON compacté\n", group);
            fclose(logg);
        }
        free(json);
        return 0;
    }

    /* Cherche device dans le groupe compacté */
    char search_key[128];
    snprintf(search_key, sizeof(search_key), "\"%s\":\"", device);
    char *found = strstr(compact, search_key);
    if (!found) {
        FILE *logf = fopen("C:\\xampp\\htdocs\\c\\src\\debug.log", "a");
        if (logf) {
            fprintf(logf, "[ERREUR] Device %s non trouvé dans JSON compacté\n", device);
            fclose(logf);
        }
        free(json);
        return 0;
    }

    /* Trouver la position dans le JSON original pour remplacer la valeur */
    char *orig = strstr(json, search_key);
    if (!orig) { free(json); return 0; }
    char *valpos = strchr(orig, ':');
    if (!valpos) { free(json); return 0; }
    valpos++;
    while (*valpos == ' ' || *valpos == '\t') valpos++;
    if (*valpos != '\"') { free(json); return 0; }
    char *valstart = valpos + 1;
    char *valend = valstart;
    while (*valend && *valend != '\"') {
        if (*valend == '\\' && *(valend + 1)) valend += 2;
        else valend++;
    }
    if (*valend != '\"') { free(json); return 0; }

    /* Création du nouveau JSON avec valeur remplacée */
    size_t before_len = (size_t)(valstart - json);
    size_t after_len = strlen(valend);
    size_t new_len = before_len + strlen(state) + after_len + 1;
    char *new_json = malloc(new_len);
    if (!new_json) { free(json); return 0; }

    memcpy(new_json, json, before_len);
    memcpy(new_json + before_len, state, strlen(state));
    strcpy(new_json + before_len + strlen(state), valend);

    int ok = write_json_file(path, new_json);

    FILE *log5 = fopen("C:\\xampp\\htdocs\\c\\src\\debug.log", "a");
    if (log5) {
        fprintf(log5, "[DEBUG] Écriture JSON terminée (ok=%d)\n", ok);
        fclose(log5);
    }

    free(json);
    free(new_json);
    return ok;
}

/* Lecture d’un device dans le JSON (tolérante aux retours ligne) */
int get_device_state_json(const char *group, const char *device, char *state_out, const char *default_state) {
    char path[260];
    snprintf(path, sizeof(path), "%s", STATE_JSON_PATH);

    char *json = read_json_file(path);
    if (!json) {
        strncpy(state_out, default_state, MAX_LEN - 1);
        state_out[MAX_LEN - 1] = '\0';
        return 0;
    }

    char compact[65536];
    compact_json(json, compact, sizeof(compact));

    char search_key[128];
    snprintf(search_key, sizeof(search_key), "\"%s\":\"", device);
    char *found = strstr(compact, search_key);
    if (!found) {
        strncpy(state_out, default_state, MAX_LEN - 1);
        state_out[MAX_LEN - 1] = '\0';
        free(json);
        return 0;
    }

    char *valstart = found + strlen(search_key);
    char *valend = strchr(valstart, '\"');
    if (!valend) {
        strncpy(state_out, default_state, MAX_LEN - 1);
        state_out[MAX_LEN - 1] = '\0';
        free(json);
        return 0;
    }

    size_t len = (size_t)(valend - valstart);
    if (len >= MAX_LEN) len = MAX_LEN - 1;
    memcpy(state_out, valstart, len);
    state_out[len] = '\0';
    free(json);
    return 1;
}

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
