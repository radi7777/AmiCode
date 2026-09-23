#ifndef AMICODE_AGENT_H
#define AMICODE_AGENT_H

#include "config.h"
#include "tools.h"

/* Eine Nachricht der Unterhaltung als fertiges JSON-Objekt (UTF-8) */
typedef struct {
    char *json;
    char *tool_id;      /* nur bei role=tool: fuer das Kuerzen */
    int compacted;
} Msg;

typedef struct {
    const Config *cfg;
    ToolCtx *tools;
    Msg *msgs;          /* msgs[0] ist immer die Systemnachricht */
    int count, cap;
    long bytes;         /* Summe der JSON-Laengen */
    char session[600];  /* Sitzungsdatei, leer = nicht speichern */
    long tokens_in, tokens_out;
    long last_in;       /* Eingabe-Tokens der letzten Anfrage = aktuelle Kontextgroesse */
    long tokens_cached; /* davon aus dem Prompt-Cache des Anbieters */
    long cost_micro;    /* Kosten in Millionstel Dollar (nur wenn der Anbieter sie meldet) */
    long ollama_ctx;    /* Kontextgroesse des Ollama-Modells (0 = noch nicht gefragt) */
    int ctx_warned;
    long compact_base;  /* Kontextgroesse direkt nach der letzten automatischen Zusammenfassung */
    int compact_pending;/* naechste Antwort liefert compact_base */
    int compact_small;  /* Warnung "Kontext zu klein" schon gezeigt */
    char model[64];     /* per /model gewaehlt, leer = aus der Konfiguration */
    const char *version;/* Programmversion fuer /status */
    StrBuf pending;     /* Ausgaben von !befehl, gehen mit dem naechsten Auftrag an das Modell */
} Agent;

int agent_init(Agent *ag, const Config *cfg, ToolCtx *tools);
void agent_free(Agent *ag);

/* Gespeicherte Sitzung laden. Liefert Anzahl geladener Nachrichten, -1 bei Fehler. */
int agent_resume(Agent *ag);
/* Verlauf verwerfen (Systemnachricht bleibt) und Sitzungsdatei neu anlegen.
   Eine Sitzung mit mindestens einem Auftrag wandert vorher nach .amicode/sessions. */
void agent_reset(Agent *ag);
/* Letzte Runden des geladenen Verlaufs anzeigen (UI_PROMPT / UI_TEXT) */
void agent_show_history(Agent *ag);
/* Archivierte Sitzungen: text=1 auch als Text, immer als UI_SESSIONS */
void agent_list_sessions(Agent *ag, int text);
/* Archivierte Sitzung loeschen (Nummer aus der Liste oder Name). 1 = ok */
int agent_delete_session(Agent *ag, const char *which);

/* Befehle wie /help, /diff, /undo, /compact, /context, /tokens, /model, /status,
   /reset, /sessions, /resume, !befehl und !!befehl. Liefert 1, wenn line ein Befehl war. */
int agent_command(Agent *ag, const char *line);

/* Fuehrt einen Auftrag (ISO-8859-1) im Agent-Loop aus. 1 = erledigt. */
int agent_ask(Agent *ag, const char *prompt, int max_steps);

#endif
