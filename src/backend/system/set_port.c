#include <stdio.h>      // fopen, printf, fread, fwrite, etc.
#include <stdlib.h>     // malloc, atoi, getenv
#include <string.h>     // strstr, strncpy
#include <errno.h>      // strerror

/* ---------------------------------------------------------------------------
   📂 Chemins relatifs ADAPTÉS À LA NOUVELLE ARBORESCENCE
   (exécutés depuis C:\xampp\cgi-bin\ )
   --------------------------------------------------------------------------- */
#define DATA_DIR "..\\htdocs\\c\\src\\backend\\data\\"

#define DEVICES_JSON_PATH      DATA_DIR "devices.json"
#define DEVICES_SAVE_PATH      DATA_DIR "devices_save.json"

/* ---------------------------------------------------------------------------
   🔧 Fonction utilitaire : copie un fichier source vers une destination
   --------------------------------------------------------------------------- */
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

/* ---------------------------------------------------------------------------
   🚀 CGI principal
   --------------------------------------------------------------------------- */
int main(void) {
    printf("Content-Type: text/plain\n\n");

    /* -----------------------------------------------------------------------
       1️⃣ Restaurer devices.json depuis devices_save.json
       ----------------------------------------------------------------------- */
    printf("🔄 Restauration du fichier devices.json...\n");

    if (!copy_file(DEVICES_SAVE_PATH, DEVICES_JSON_PATH)) {
        printf("❌ Erreur de restauration.\n");
        return 1;
    }

    printf("✅ Fichier restauré.\n\n");

    /* -----------------------------------------------------------------------
       2️⃣ Lire le paramètre ?port=xxxx
       ----------------------------------------------------------------------- */
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

    /* -----------------------------------------------------------------------
       3️⃣ Lire devices.json
       ----------------------------------------------------------------------- */
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

    /* -----------------------------------------------------------------------
       4️⃣ Rechercher "port":
       ----------------------------------------------------------------------- */
    char *pos = strstr(content, "\"port\":");
    if (!pos) {
        printf("Champ 'port' non trouvé dans JSON.\n");
        free(content);
        return 1;
    }

    /* -----------------------------------------------------------------------
       5️⃣ Construire le nouveau JSON
       ----------------------------------------------------------------------- */
    char newJson[200000];

    char *after = strchr(pos, ',');
    if (!after) after = strchr(pos, '}');
    if (!after) {
        printf("Erreur : JSON mal formé.\n");
        free(content);
        return 1;
    }

    snprintf(newJson, sizeof(newJson),
             "%.*s\"port\": %d%s",
             (int)(pos - content),
             content,
             port,
             after);

    /* -----------------------------------------------------------------------
       6️⃣ Écrire devices.json mis à jour
       ----------------------------------------------------------------------- */
    FILE *fw = fopen(DEVICES_JSON_PATH, "w");
    if (!fw) {
        printf("Erreur ouverture (écriture) : %s\n", strerror(errno));
        free(content);
        return 1;
    }

    fwrite(newJson, 1, strlen(newJson), fw);
    fclose(fw);
    free(content);

    /* -----------------------------------------------------------------------
       🎉 Fini !
       ----------------------------------------------------------------------- */
    printf("✅ Port mis à jour à %d dans %s\n", port, DEVICES_JSON_PATH);
    return 0;
}
