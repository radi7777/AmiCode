#ifndef AMICODE_GUI_SKILLS_H
#define AMICODE_GUI_SKILLS_H

#include <libraries/mui.h>

#include "config.h"

/* Fenster "Skills": Modus je Skill (immer / automatisch / aus) waehlen.
   Eigene ReturnIDs ab SKILLS_ID_BASE. */

#define SKILLS_ID_BASE 1400

enum { SK_NONE, SK_SAVE, SK_CANCEL };

Object *skillswin_create(void);
void skillswin_notify(Object *app);
void skillswin_open(const Config *cfg);
void skillswin_close(void);
int skillswin_is_id(ULONG id);
int skillswin_handle(ULONG id);             /* SK_* */
/* Gewaehlte Modi in die Konfiguration schreiben. Nur bei gestopptem Agenten (malloc). */
void skillswin_apply(Config *cfg);

#endif
