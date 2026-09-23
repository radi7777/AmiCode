#ifndef AMICODE_GUI_PREFS_H
#define AMICODE_GUI_PREFS_H

#include <libraries/mui.h>

#include "config.h"

/* Einstellungsfenster: Anbieter, Zugangsdaten, Modellauswahl.
   Eigene ReturnIDs ab PREFS_ID_BASE; prefs_handle() wertet sie aus. */

#define PREFS_ID_BASE   1000

enum { PREFS_NONE, PREFS_SAVE, PREFS_USE, PREFS_CANCEL };

Object *prefs_create(void);                 /* Fenster bauen (vor dem Application-Objekt) */
void prefs_notify(Object *app);             /* Notifies setzen (nach dem Application-Objekt) */
void prefs_open(const Config *cfg);         /* Felder aus der Konfiguration fuellen und oeffnen */
void prefs_close(void);
int prefs_handle(ULONG id);                 /* PREFS_* */
int prefs_is_id(ULONG id);

/* Eingaben in die Konfiguration uebernehmen. Nur bei gestopptem Agent-Prozess
   aufrufen (config_set benutzt malloc). */
void prefs_apply(Config *cfg);

/* Modellliste (ein Modell pro Zeile) fuer die Seite, die sie angefordert hat */
void prefs_models(const char *list);

#endif
