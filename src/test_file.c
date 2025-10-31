#include <stdio.h>

int main(void) {
    const char *path = "C:\\xampp\\htdocs\\c\\src\\devices.json";
    FILE *f = fopen(path, "rb");
    printf("Content-type: text/html\r\n\r\n");
    printf("<html><body>");
    if (!f) {
        printf("<p style='color:red;'>❌ Impossible d'ouvrir %s</p>", path);
    } else {
        printf("<p style='color:green;'>✅ Lecture OK de %s</p>", path);
        fclose(f);
    }

    FILE *fw = fopen("C:\\xampp\\htdocs\\c\\src\\test_write.txt", "wb");
    if (!fw) {
        printf("<p style='color:red;'>❌ Impossible d'écrire dans le dossier src</p>");
    } else {
        fprintf(fw, "test ok");
        fclose(fw);
        printf("<p style='color:green;'>✅ Écriture test_write.txt réussie</p>");
    }
    printf("</body></html>");
    return 0;
}
