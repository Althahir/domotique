#ifndef DOMOTIQUE_H
#define DOMOTIQUE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STATE_DIR "C:\\xampp\\htdocs\\c\\src\\"
#define STATE_JSON "devices.json"
#define STATE_JSON_PATH STATE_DIR STATE_JSON

#define MAX_LEN 256
#define READ_BUF_SIZE 16384

/* JSON-based persistence (simple) */
int set_device_state_json(const char *group, const char *device, const char *state);
int get_device_state_json(const char *group, const char *device, char *state_out, const char *default_state);

/* Gabarit HTML minimal */
void html_header(const char *title);
void html_footer(void);

#endif
