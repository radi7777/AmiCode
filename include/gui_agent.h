#ifndef AMICODE_GUI_AGENT_H
#define AMICODE_GUI_AGENT_H

#include <exec/ports.h>
#include <dos/dos.h>

#include "config.h"

/* Der Agent laeuft in einem eigenen Prozess, damit MUI nie blockiert.
   Wichtig: libnix-malloc ist nicht threadsicher. Solange der Agent-Prozess
   laeuft, benutzt der GUI-Prozess nur AllocVec/FreeVec. */

/* Ereignis vom Agenten an die Oberflaeche (AllocVec, Text haengt dahinter) */
typedef struct {
    struct Message msg;     /* mn_ReplyPort != NULL: Rueckfrage, Antwort erwartet */
    LONG kind;              /* UI_* aus ui.h, zusaetzlich EV_IDLE */
    LONG answer;            /* bei Rueckfragen: ASK_* */
    char text[1];
} GuiEvent;

#define EV_IDLE     100     /* Auftrag beendet, Agent wartet */
#define EV_ASK      101     /* Rueckfrage (Freigabe) */

void gui_agent_version(const char *version);   /* fuer /status, vor dem Start */

/* Agent-Prozess starten. dir: Projektverzeichnis (wird dupliziert).
   events: Port der Oberflaeche fuer GuiEvents. 0 = Fehler. */
int gui_agent_start(const Config *cfg, BPTR dir, int mode, struct MsgPort *events);

void gui_agent_prompt(const char *text);    /* ISO-8859-1 */
void gui_agent_reset(void);
void gui_agent_resume(void);
void gui_agent_mode(int mode);
void gui_agent_shell(const char *cmd);
void gui_agent_undo(void);
void gui_agent_diff(void);
void gui_agent_models(const char *spec);
void gui_agent_choices(void);                  /* -> UI_CHOICES */
void gui_agent_newproject(const char *spec);   /* "id\ndir\nart\ncpu\nos\nbeschreibung" */      /* "anbieter\nbasis\nkey" -> UI_MODELS "prefs" */        /* Befehl ohne Agent, Ausgabe als UI_OUTPUT */
void gui_agent_cmdline(const char *cmd);        /* 1>-Zeile: CD bleibt erhalten -> UI_CWD */
void gui_agent_sessions(const char *del);      /* Liste -> UI_SESSIONS; del != NULL: vorher loeschen */
void gui_agent_break(void);                 /* CTRL-C an den Agenten */

/* Agenten beenden. Beantwortet offene Rueckfragen mit "Nein" und
   verwirft Ereignisse, bis der Prozess fertig ist. */
void gui_agent_stop(void);

/* Antworten der Befehls-Nachrichten abholen (Signal: gui_agent_replysig) */
void gui_agent_collect(void);
ULONG gui_agent_replysig(void);

#endif
