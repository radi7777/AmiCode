#ifndef AMICODE_CONFIG_H
#define AMICODE_CONFIG_H

/* INI-Datei: [abschnitt] + schluessel=wert. Zugriff ueber "abschnitt.schluessel". */

#define CONFIG_DEFAULT_PATH "ENV:AmiCode/amicode.conf"

typedef struct ConfigEntry {
    char *key;      /* "provider.model" */
    char *value;
    struct ConfigEntry *next;
} ConfigEntry;

typedef struct {
    ConfigEntry *first;
} Config;

int config_load(Config *cfg, const char *path);   /* 0 = Fehler */
void config_free(Config *cfg);
const char *config_get(const Config *cfg, const char *key, const char *def);

/* Wert setzen oder neu anlegen ("abschnitt.schluessel") */
int config_set(Config *cfg, const char *key, const char *value);
/* Schluessel entfernen (alle Vorkommen) */
void config_remove(Config *cfg, const char *key);
/* Als INI-Datei schreiben (Abschnitte gruppiert). header: Kommentarzeile oder NULL */
int config_save(const Config *cfg, const char *path, const char *header);

#endif
