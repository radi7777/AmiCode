#ifndef AMICODE_TOOLCHAIN_H
#define AMICODE_TOOLCHAIN_H

#include "config.h"
#include "json.h"

/* Entwicklungswerkzeuge (Compiler, Interpreter), die AmiCode kennt.
   Welche davon installiert sind und wo, steht in toolchains.conf. */

#define TOOLCHAINS_CONF     "ENV:AmiCode/toolchains.conf"
#define TOOLCHAINS_CONF_ARC "ENVARC:AmiCode/toolchains.conf"

/* Installierte Werkzeuge suchen und toolchains.conf schreiben.
   report erhaelt eine lesbare Liste. Liefert die Anzahl gefundener Werkzeuge. */
int toolchains_scan(StrBuf *report);

/* Gefundene Werkzeuge auflisten (liest toolchains.conf; scannt, wenn sie fehlt) */
void toolchains_list(StrBuf *out);

/* Kurzfassung fuer den Systemprompt des Agenten (leer, wenn nichts bekannt) */
void toolchains_prompt(StrBuf *out);

/* Angaben aus dem Assistenten "Neues Projekt" (alle optional) */
typedef struct {
    const char *kind;           /* Programmart, z. B. "Shell-Programm" */
    const char *cpu;            /* z. B. "68020" */
    const char *os;             /* z. B. "AmigaOS 3.2" */
    const char *description;    /* was das Programm tun soll */
} ProjectOptions;

/* Auswahl fuer den Assistenten: "id\tname\n" je gefundenem Werkzeug, zuletzt "custom" */
void toolchains_choices(StrBuf *out);

/* Wie toolchains_new_project, mit Angaben aus dem Assistenten. id "custom" =
   Projekt ohne Vorgaben (Agent waehlt Sprache und Werkzeug). */
int toolchains_new_project2(const char *id, const char *dir, const ProjectOptions *opt, StrBuf *out);

/* Neues Projekt anlegen: Verzeichnis, Quelltext, build-Skript, AMICODE.md,
   .amicode/settings. id z. B. "vbcc". Liefert 1 bei Erfolg; out beschreibt es. */
int toolchains_new_project(const char *id, const char *dir, StrBuf *out);

#endif
