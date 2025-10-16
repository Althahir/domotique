#include <stdio.h>

int main() {
    // 1. En-tête HTTP (OBLIGATOIRE !)
    printf("Content-type: text/html\n\n"); 
    
    // 2. Contenu HTML généré par le programme C
    printf("<!DOCTYPE html>\n");
    printf("<html>\n");
    printf("<head><title>Mon Programme CGI en C</title></head>\n");
    printf("<body>\n");
    printf("<h1>Bonjour depuis le programme C!</h1>\n");
    printf("<p>Ceci a été généré dynamiquement par le code C.</p>\n");
    printf("</body>\n");
    printf("</html>\n");
    
    return 0;
}