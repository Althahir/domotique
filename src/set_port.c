#include <stdio.h>      // Entrées/sorties standard : fopen, printf, fread, fwrite, etc.
#include <stdlib.h>     // Fonctions utilitaires : malloc, atoi, getenv, etc.
#include <string.h>     // Manipulation de chaînes : strstr, strncpy, etc.
#include <errno.h>      // Gestion des erreurs système (strerror)

/* ---------------------------------------------------------------------------
   📂 Définition des chemins de fichiers utilisés
   --------------------------------------------------------------------------- */
#define DEVICES_JSON_PATH "C:\\xampp\\htdocs\\c\\src\\devices.json"        // Fichier JSON actif
#define DEVICES_SAVE_PATH "C:\\xampp\\htdocs\\c\\src\\devices_save.json"   // Copie de sauvegarde

/* ---------------------------------------------------------------------------
   🔧 Fonction utilitaire : copy_file()
   Copie le contenu d’un fichier source (binaire) vers un fichier destination.
   Utilisée ici pour restaurer "devices.json" à partir de "devices_save.json".
   --------------------------------------------------------------------------- */
int copy_file(const char *src, const char *dst) {
    FILE *in = fopen(src, "rb");       // Ouvre le fichier source en lecture binaire
    if (!in) {
        printf("❌ Erreur : impossible d’ouvrir %s\n", src);
        return 0;
    }

    FILE *out = fopen(dst, "wb");      // Ouvre la destination en écriture binaire
    if (!out) {
        printf("❌ Erreur : impossible d’écrire dans %s\n", dst);
        fclose(in);
        return 0;
    }

    char buf[4096];                    // Buffer de transfert (4 Ko)
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {  // Lecture du fichier source
        fwrite(buf, 1, n, out);                         // Écriture vers le fichier cible
    }

    fclose(in);
    fclose(out);
    return 1;  // Succès
}

/* ---------------------------------------------------------------------------
   🚀 Programme principal CGI
   Étapes :
   1️⃣ Restaure le fichier `devices.json` depuis la sauvegarde
   2️⃣ Lit le paramètre `port` dans l’URL
   3️⃣ Met à jour la valeur `"port": ...` dans le JSON
   --------------------------------------------------------------------------- */
int main(void) {
    // En-tête HTTP obligatoire (réponse en texte brut)
    printf("Content-Type: text/plain\n\n");

    /* -----------------------------------------------------------------------
       1️⃣ Restauration du fichier principal à partir de la sauvegarde
       ----------------------------------------------------------------------- */
    printf("🔄 Restauration du fichier devices.json depuis devices_save.json...\n");
    if (copy_file(DEVICES_SAVE_PATH, DEVICES_JSON_PATH)) {
        printf("✅ Fichier restauré avec succès.\n\n");
    } else {
        printf("❌ Erreur : échec de la restauration du fichier.\n\n");
    }

    /* -----------------------------------------------------------------------
       2️⃣ Lecture du paramètre 'port' depuis la query string
       Exemple d’URL : /cgi-bin/set_port.exe?port=49995
       ----------------------------------------------------------------------- */
    char *qs = getenv("QUERY_STRING");  // Récupération des paramètres d’URL
    if (!qs || strstr(qs, "port=") == NULL) {
        printf("Erreur : paramètre 'port' manquant.\n");
        return 1;
    }

    // Conversion texte → entier
    int port = atoi(strstr(qs, "port=") + 5);
    if (port <= 0) {
        printf("Erreur : port invalide (%d)\n", port);
        return 1;
    }

    /* -----------------------------------------------------------------------
       3️⃣ Lecture du contenu actuel du fichier JSON
       ----------------------------------------------------------------------- */
    FILE *f = fopen(DEVICES_JSON_PATH, "r");
    if (!f) {
        printf("Erreur ouverture (lecture) : %s\n", strerror(errno));
        printf("Chemin utilisé : %s\n", DEVICES_JSON_PATH);
        return 1;
    }

    // Lecture complète du fichier dans une chaîne
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    rewind(f);
    char *content = malloc(size + 1);
    fread(content, 1, size, f);
    content[size] = '\0';
    fclose(f);

    /* -----------------------------------------------------------------------
       4️⃣ Recherche de la clé "port" dans le JSON
       ----------------------------------------------------------------------- */
    char *pos = strstr(content, "\"port\":");
    if (!pos) {
        printf("Champ 'port' non trouvé.\n");
        free(content);
        return 1;
    }

    /* -----------------------------------------------------------------------
       5️⃣ Remplacement de la valeur du port
       On reconstruit le JSON en gardant tout avant "port",
       puis on insère la nouvelle valeur numérique, puis le reste.
       ----------------------------------------------------------------------- */
    char newJson[100000];
    char *after = strchr(pos, ',');  // cherche la virgule après le port
    if (!after) after = strchr(pos, '}');  // sinon la fin du bloc JSON
    if (!after) {
        printf("Erreur : JSON mal formé.\n");
        free(content);
        return 1;
    }

    // Reconstitution du JSON avec la nouvelle valeur
    snprintf(newJson, sizeof(newJson), "%.*s\"port\": %d%s",
             (int)(pos - content), content, port, after);

    /* -----------------------------------------------------------------------
       6️⃣ Écriture du JSON modifié dans le fichier principal
       ----------------------------------------------------------------------- */
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

    /* -----------------------------------------------------------------------
       ✅ Fin du traitement
       ----------------------------------------------------------------------- */
    printf("✅ Port mis à jour à %d dans %s\n", port, DEVICES_JSON_PATH);
    return 0;
}
