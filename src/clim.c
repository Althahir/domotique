#include "domotique.h"
#include "socket_client.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <windows.h>

/* --- Décodage URL --- */
static void urldecode(char *dst, const char *src) {
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

/* --- Convertit "00010111" → 8 octets (0x00 ou 0x01) --- */
static void input_to_bytes(const char *input, unsigned char *buffer) {
    for (int i = 0; i < 8; i++) buffer[i] = (input[i] == '1') ? 1 : 0;
}

/* --- Convertit "192.168.0.100" → 4 octets --- */
static void ip_to_bytes(const char *ip, unsigned char *buffer) {
    unsigned int a, b, c, d;
    sscanf(ip, "%u.%u.%u.%u", &a, &b, &c, &d);
    buffer[0] = (unsigned char)a;
    buffer[1] = (unsigned char)b;
    buffer[2] = (unsigned char)c;
    buffer[3] = (unsigned char)d;
}

/* --- Helpers JSON (locaux) : lecture/écriture du mode (0/1) --- */
static char *read_file_all(const char *path, long *out_size) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);
    char *buf = (char*)malloc(size + 1);
    if (!buf) { fclose(f); return NULL; }
    fread(buf, 1, size, f);
    buf[size] = '\0';
    fclose(f);
    if (out_size) *out_size = size;
    return buf;
}

static int write_file_all(const char *path, const char *buf, size_t len) {
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    size_t w = fwrite(buf, 1, len, f);
    fclose(f);
    return (w == len);
}

static int json_get_mode(const char *device, int *mode_out) {
    long sz = 0;
    char *json = read_file_all(STATE_JSON_PATH, &sz);
    if (!json) return 0;

    char search[256];
    snprintf(search, sizeof(search), "\"%s\"", device);
    char *dev = strstr(json, search);
    if (!dev) { free(json); return 0; }

    char *block = strchr(dev, '{');
    if (!block) { free(json); return 0; }

    char *k = strstr(block, "\"mode\"");
    if (!k) { free(json); return 0; }
    k = strchr(k, ':');
    if (!k) { free(json); return 0; }
    k++;
    while (*k == ' ' || *k == '\t' || *k == '\r' || *k == '\n') k++;

    // attend 0 ou 1
    if (*k == '0' || *k == '1') {
        *mode_out = (*k == '1') ? 1 : 0;
        free(json);
        return 1;
    }
    free(json);
    return 0;
}

static int json_set_mode(const char *device, int mode_value) {
    long sz = 0;
    char *json = read_file_all(STATE_JSON_PATH, &sz);
    if (!json) return 0;

    char search[256];
    snprintf(search, sizeof(search), "\"%s\"", device);
    char *dev = strstr(json, search);
    if (!dev) { free(json); return 0; }

    char *block = strchr(dev, '{');
    if (!block) { free(json); return 0; }

    char *k = strstr(block, "\"mode\"");
    if (!k) { free(json); return 0; }
    k = strchr(k, ':');
    if (!k) { free(json); return 0; }
    k++;
    while (*k == ' ' || *k == '\t' || *k == '\r' || *k == '\n') k++;

    // remplace le caractère 0/1 existant
    if (*k == '0' || *k == '1') {
        *k = (mode_value == 1) ? '1' : '0';
        int ok = write_file_all(STATE_JSON_PATH, json, strlen(json));
        free(json);
        return ok;
    }

    free(json);
    return 0;
}

/* ------------------------------------------------------------------- */
/* PROGRAMME PRINCIPAL                                                 */
/* ------------------------------------------------------------------- */
int main(void) {
    const char *group = "clim";
    const char *sim_host = "192.168.56.1";
    const int   sim_port = 50550;

    char *qs = getenv("QUERY_STRING");
    char decoded_device[MAX_LEN] = "";
    char ip_auto[16] = "";
    char input_auto[9] = "";
    char current_state[8] = "";

    /* 1) Récupère device depuis la querystring */
    if (qs) {
        char *p = strstr(qs, "device=");
        if (p) {
            p += 7;
            char enc[MAX_LEN];
            char *end = strchr(p, '&');
            if (end) { strncpy(enc, p, end - p); enc[end - p] = '\0'; }
            else { strcpy(enc, p); }
            urldecode(decoded_device, enc);
        }
    }

    /* 2) Lit IP / input / state dans le JSON via communication.c */
    if (!get_device_info(group, decoded_device, ip_auto, input_auto, current_state)) {
        html_header("Erreur Clim");
        printf("<p style='color:red;'>❌ Climatisation \"%s\" introuvable (groupe %s).</p>", decoded_device, group);
        html_footer();
        return 0;
    }

    /* 3) Récupère le mode courant (0/1) */
    int mode_val = 0; // défaut = clim/froid
    json_get_mode(decoded_device, &mode_val);

    /* 4) Construit la trame : 14 octets */
    unsigned char msg[14] = {0};
    ip_to_bytes(ip_auto, msg);          // [0..3]  IP
    input_to_bytes(input_auto, msg + 4);// [4..11] 8 octets input

    // [12] = state (0/1) ; [13] = mode (0/1)
    // Mettez d’abord la situation JSON actuelle :
    msg[12] = (strcmp(current_state, "ON") == 0) ? 1 : 0;
    msg[13] = (unsigned char)mode_val;

    /* 5) Applique l'action demandée */
    if (qs && strstr(qs, "action=on")) {
        // state = 1
        set_device_state_json(group, decoded_device, "ON");
        msg[12] = 1;
    } else if (qs && strstr(qs, "action=off")) {
        // state = 0
        set_device_state_json(group, decoded_device, "OFF");
        msg[12] = 0;
    } else if (qs && strstr(qs, "action=cool")) {
        // mode = 0 (clim)
        json_set_mode(decoded_device, 0);
        msg[13] = 0;
    } else if (qs && strstr(qs, "action=heat")) {
        // mode = 1 (chauffage)
        json_set_mode(decoded_device, 1);
        msg[13] = 1;
    }

    /* 6) Envoi (3 fois) */
    for (int i = 0; i < 3; i++) {
        send_to_simulator_binary(sim_host, sim_port, msg, sizeof(msg)); // 14 octets
        Sleep(50);
    }

    /* 7) Réponse HTML simple */
    html_header("Climatisation");
    printf("<div class='wrap'>");
    printf("<h2>Climatisation %s</h2>", decoded_device);
    printf("<p><em>IP automate :</em> %s<br><em>Entrée automate :</em> %s</p>", ip_auto, input_auto);
    printf("<p>Trame envoyée avec state=%d, mode=%d.</p>", msg[12], msg[13]);
    printf("<p><a href='/c/cuisine.html'>&larr; Retour</a></p>");
    printf("</div>");
    html_footer();
    return 0;
}
