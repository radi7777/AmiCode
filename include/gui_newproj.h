#ifndef AMICODE_GUI_NEWPROJ_H
#define AMICODE_GUI_NEWPROJ_H

#include <libraries/mui.h>

/* Assistent "Neues Projekt": fragt Name, Ort, Werkzeug, Programmart,
   Zielsystem und die Aufgabe ab. Eigene ReturnIDs ab NEWPROJ_ID_BASE. */

#define NEWPROJ_ID_BASE 1100

enum { NP_NONE, NP_CREATE, NP_CANCEL };

Object *newproj_create(void);
void newproj_notify(Object *app);
void newproj_open(const char *default_dir);     /* fordert die Werkzeugliste beim Agenten an */
void newproj_close(void);
int newproj_is_id(ULONG id);
int newproj_handle(ULONG id);                   /* NP_* */

void newproj_choices(const char *list);         /* UI_CHOICES: "id\tname" pro Zeile */

/* Nach NP_CREATE gueltig (statische Puffer): */
const char *newproj_spec(void);                 /* fuer gui_agent_newproject() */
const char *newproj_first_prompt(void);         /* erster Auftrag oder NULL */

#endif
