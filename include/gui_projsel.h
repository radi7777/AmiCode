#ifndef AMICODE_GUI_PROJSEL_H
#define AMICODE_GUI_PROJSEL_H

#include <libraries/mui.h>

/* Fenster "Projekt oeffnen": Liste der Projekte im Projektordner
   (<Programmverzeichnis>/Projects). Eigene ReturnIDs ab PROJSEL_ID_BASE. */

#define PROJSEL_ID_BASE 1200

enum { PS_NONE, PS_OPEN, PS_NEW, PS_OTHER, PS_CANCEL };

Object *projsel_create(void);
void projsel_notify(Object *app);
int projsel_open(const char *projects_dir, const char *current);   /* Anzahl Projekte */
void projsel_close(void);
int projsel_is_id(ULONG id);
int projsel_handle(ULONG id);               /* PS_* */
const char *projsel_selected(void);         /* voller Pfad, nach PS_OPEN */

#endif
