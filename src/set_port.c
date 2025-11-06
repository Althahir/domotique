#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define DEVICES_JSON_PATH "C:\\xampp\\htdocs\\c\\src\\devices.json"
#define DEVICES_SAVE_PATH "C:\\xampp\\htdocs\\c\\src\\devices_save.json"

// -------------------------------------------------------------
// Fonction utilitaire : copie un fichier binaire (sauvegarde → actif)
// -------------------------------------------------------------
int copy_file(const char *src, const char *dst) {
    FILE *in = fopen(src, "rb");
    if (!in) {
        printf("❌ Erreur : impossible d’ouvrir %s\n", src);
        return 0;
    }

    FILE *out = fopen(dst, "wb");
    if (!out) {
        printf("❌ Erreur : impossible d’écrire dans %s\n", dst);
        fclose(in);
        return 0;
    }

    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        fwrite(buf, 1, n, out);
    }

    fclose(in);
    fclose(out);
    return 1;
}

// -------------------------------------------------------------
// Programme principal CGI
// -------------------------------------------------------------
int main(void) {
    printf("Content-Type: text/plain\n\n");

    // 1️⃣ Restauration du fichier devices.json depuis la sauvegarde
    printf("🔄 Restauration du fichier devices.json depuis devices_save.json...\n");
    if (copy_file(DEVICES_SAVE_PATH, DEVICES_JSON_PATH)) {
        printf("✅ Fichier restauré avec succès.\n\n");
    } else {
        printf("❌ Erreur : échec de la restauration du fichier.\n\n");
    }

    // 2️⃣ Lecture du paramètre port
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

    // 3️⃣ Lecture du JSON existant (copié à l'étape 1)
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

    // 4️⃣ Recherche du champ "port"
    char *pos = strstr(content, "\"port\":");
    if (!pos) {
        printf("Champ 'port' non trouvé.\n");
        free(content);
        return 1;
    }

    // 5️⃣ Mise à jour du port
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

    // 6️⃣ Sauvegarde du JSON mis à jour
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
