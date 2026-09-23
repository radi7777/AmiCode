#ifndef AMICODE_GUI_SESSIONS_H
#define AMICODE_GUI_SESSIONS_H

#include <libraries/mui.h>

/* Fenster "Sitzungen": archivierte Sitzungen des Projekts fortsetzen oder loeschen.
   Die Liste kommt vom Agenten (UI_SESSIONS). Eigene ReturnIDs ab SESSIONS_ID_BASE. */

#define SESSIONS_ID_BASE 1300

enum { SS_NONE, SS_RESUME, SS_DELETE, SS_CLOSE };

Object *sessions_create(void);
void sessions_notify(Object *app);
void sessions_open(void);                   /* oeffnen; Liste folgt per sessions_fill */
void sessions_close(void);
int sessions_is_open(void);
void sessions_fill(const char *text);       /* Inhalt eines UI_SESSIONS-Ereignisses */
int sessions_is_id(ULONG id);
int sessions_handle(ULONG id);              /* SS_* */
const char *sessions_selected(void);        /* Name der gewaehlten Sitzung, "" = aktuelle */

#endif
