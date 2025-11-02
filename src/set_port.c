#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define DEVICES_JSON_PATH "C:\\xampp\\htdocs\\c\\src\\devices.json"

int main(void) {
    printf("Content-Type: text/plain\n\n");

    char *qs = getenv("QUERY_STRING");
    if (!qs || strstr(qs, "port=") == NULL) {
        printf("Erreur : paramètre 'port' manquant.\n");
        return 1;
    }

    int port = atoi(strstr(qs, "port=") + 5);
    if (port <= 0) {
        printf("Erreur : port invalide (%d)\n", port);
        return 1;
    }

    FILE *f = fopen(DEVICES_JSON_PATH, "r");
    if (!f) {
        printf("Erreur ouverture (lecture) : %s\n", strerror(errno));
        printf("Chemin utilisé : %s\n", DEVICES_JSON_PATH);
        return 1;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);
    char *content = malloc(size + 1);
    fread(content, 1, size, f);
    content[size] = '\0';
    fclose(f);

    char *pos = strstr(content, "\"port\":");
    if (!pos) {
        printf("Champ 'port' non trouvé.\n");
        free(content);
        return 1;
    }

    // 🔧 Construire le nouveau JSON
    char newJson[100000];
    char *after = strchr(pos, ',');
    if (!after) after = strchr(pos, '}');
    if (!after) {
        printf("Erreur : JSON mal formé.\n");
        free(content);
        return 1;
    }

    snprintf(newJson, sizeof(newJson), "%.*s\"port\": %d%s",
             (int)(pos - content), content, port, after);

    FILE *fw = fopen(DEVICES_JSON_PATH, "w");
    if (!fw) {
        printf("Erreur ouverture (écriture) : %s\n", strerror(errno));
        printf("Chemin utilisé : %s\n", DEVICES_JSON_PATH);
        free(content);
        return 1;
    }

    fwrite(newJson, 1, strlen(newJson), fw);
    fclose(fw);
    free(content);

    printf("✅ Port mis à jour à %d dans %s\n", port, DEVICES_JSON_PATH);
    return 0;
}
