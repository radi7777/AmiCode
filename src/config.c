#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"

static char *trim(char *s)
{
    char *e;
    while (*s == ' ' || *s == '\t')
        s++;
    e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\n' || e[-1] == '\r'))
        *--e = 0;
    return s;
}

int config_load(Config *cfg, const char *path)
{
    FILE *f;
    char line[512];
    char section[64] = "";

    ConfigEntry **tail = &cfg->first;

    cfg->first = NULL;
    if (!(f = fopen(path, "r")))
        return 0;

    while (fgets(line, sizeof(line), f)) {
        char *s = trim(line), *eq;
        ConfigEntry *e;

        if (!*s || *s == '#' || *s == ';')
            continue;
        if (*s == '[') {
            char *end = strchr(s, ']');
            if (end) {
                *end = 0;
                strncpy(section, trim(s + 1), sizeof(section) - 1);
                section[sizeof(section) - 1] = 0;
            }
            continue;
        }
        if (!(eq = strchr(s, '=')))
            continue;
        *eq = 0;

        e = calloc(1, sizeof(ConfigEntry));
        if (!e)
            break;
        e->key = malloc(strlen(section) + strlen(trim(s)) + 2);
        e->value = strdup(trim(eq + 1));
        if (!e->key || !e->value) {
            free(e->key);
            free(e->value);
            free(e);
            break;
        }
        sprintf(e->key, "%s%s%s", section, *section ? "." : "", trim(s));
        /* in Dateireihenfolge anhaengen (wichtig fuer config_save) */
        *tail = e;
        tail = &e->next;
    }
    fclose(f);
    return 1;
}

void config_free(Config *cfg)
{
    ConfigEntry *e = cfg->first;
    while (e) {
        ConfigEntry *n = e->next;
        free(e->key);
        free(e->value);
        free(e);
        e = n;
    }
    cfg->first = NULL;
}

const char *config_get(const Config *cfg, const char *key, const char *def)
{
    ConfigEntry *e;
    for (e = cfg->first; e; e = e->next)
        if (strcmp(e->key, key) == 0)
            return e->value;
    return def;
}

int config_set(Config *cfg, const char *key, const char *value)
{
    ConfigEntry *e;
    char *v = strdup(value ? value : "");

    if (!v)
        return 0;
    for (e = cfg->first; e; e = e->next)
        if (strcmp(e->key, key) == 0) {
            free(e->value);
            e->value = v;
            return 1;
        }
    if (!(e = calloc(1, sizeof(ConfigEntry))) || !(e->key = strdup(key))) {
        free(e);
        free(v);
        return 0;
    }
    e->value = v;
    /* hinten anhaengen, damit die Reihenfolge der Datei erhalten bleibt */
    if (!cfg->first) {
        cfg->first = e;
    } else {
        ConfigEntry *last = cfg->first;
        while (last->next)
            last = last->next;
        last->next = e;
    }
    return 1;
}

/* Abschnittsname eines Schluessels ("openai.model" -> "openai", len 6) */
static int section_len(const char *key)
{
    const char *dot = strchr(key, '.');
    return dot ? (int)(dot - key) : 0;
}

int config_save(const Config *cfg, const char *path, const char *header)
{
    FILE *f;
    ConfigEntry **list, *e;
    int n = 0, i, j;
    char *done;

    for (e = cfg->first; e; e = e->next)
        n++;
    if (!(list = malloc((n + 1) * sizeof(*list))) || !(done = calloc(n + 1, 1))) {
        free(list);
        return 0;
    }
    /* Eintraege stehen in Dateireihenfolge, per config_set neue dahinter */
    i = 0;
    for (e = cfg->first; e; e = e->next)
        list[i++] = e;
    if (!(f = fopen(path, "w"))) {
        free(list);
        free(done);
        return 0;
    }
    if (header)
        fprintf(f, "%s\n", header);
    for (i = 0; i < n; i++) {
        int sl;
        if (done[i])
            continue;
        sl = section_len(list[i]->key);
        if (sl)
            fprintf(f, "\n[%.*s]\n", sl, list[i]->key);
        for (j = i; j < n; j++) {
            if (done[j] || section_len(list[j]->key) != sl ||
                strncmp(list[j]->key, list[i]->key, sl) != 0)
                continue;
            fprintf(f, "%s=%s\n", list[j]->key + (sl ? sl + 1 : 0), list[j]->value);
            done[j] = 1;
        }
    }
    fclose(f);
    free(list);
    free(done);
    return 1;
}

void config_remove(Config *cfg, const char *key)
{
    ConfigEntry **pp = &cfg->first, *e;

    while ((e = *pp)) {
        if (strcmp(e->key, key) == 0) {
            *pp = e->next;
            free(e->key);
            free(e->value);
            free(e);
        } else {
            pp = &e->next;
        }
    }
}
