#ifndef AMICODE_SKILLS_H
#define AMICODE_SKILLS_H

#include "config.h"
#include "json.h"

/* Skills: Wissensdateien fuer das Modell (Markdown). Erste Zeile "# Titel",
   dann "Description: ..." und der Inhalt. Gesucht wird in .amicode/skills
   (Projekt, hat Vorrang) und PROGDIR:Skills. Pro Skill ein Modus in der
   Konfiguration: [skills] name=auto|always|off (Standard auto).
   skills_scan kommt ohne malloc aus und darf auch im GUI-Prozess laufen. */

#define SKILLS_MAX 32

enum { SKILL_OFF, SKILL_AUTO, SKILL_ALWAYS };

typedef struct {
    char name[32];          /* Dateiname ohne .md */
    char title[64];
    char desc[200];
    char path[160];
} SkillInfo;

int skills_scan(SkillInfo *out, int max);                 /* Anzahl */
int skills_mode(const Config *cfg, const char *name);     /* SKILL_* */
const char *skills_mode_name(int mode);                   /* "auto", "always", "off" */

/* Systemprompt: Inhalte der "always"-Skills und Liste der "auto"-Skills */
void skills_prompt(const Config *cfg, StrBuf *sys);
/* Werkzeug read_skill: Inhalt eines Skills (ohne Kopfzeilen) */
void skills_read(const Config *cfg, const char *name, StrBuf *out);
/* Uebersicht fuer /skills */
void skills_list(const Config *cfg, StrBuf *out);

#endif
