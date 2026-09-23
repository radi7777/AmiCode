#ifndef AMICODE_TOOLS_H
#define AMICODE_TOOLS_H

#include <dos/dos.h>

#include "config.h"
#include "json.h"

enum { MODE_SAFE, MODE_PROJECT, MODE_FULL };

enum { TOOL_READ, TOOL_LIST, TOOL_FIND, TOOL_SEARCH, TOOL_WRITE, TOOL_EDIT, TOOL_RUN, TOOL_WEB, TOOL_COUNT };

#define MAX_PROTECTED   32
#define STATE_DIR       ".amicode"

/* Datei, die in dieser Sitzung gelesen wurde (Veraltungsschutz) */
typedef struct {
    char *path;             /* voller, aufgeloester Pfad */
    long size;
    struct DateStamp date;
} ReadRec;

/* Datei, die in dieser Sitzung geaendert wurde (Sicherung, /undo, /diff) */
typedef struct {
    char *path;             /* voller Pfad des Originals */
    char *backup;           /* Sicherung oder NULL, wenn die Datei neu war */
} Change;

typedef struct {
    const Config *cfg;
    int mode;
    char root[512];             /* voller Pfad des Projektverzeichnisses */
    BPTR root_lock;
    BPTR protected[MAX_PROTECTED];  /* SYS:, C:, S:, LIBS:, DEVS:, L: (alle Verzeichnisse) */
    int nprotected;
    int always[TOOL_COUNT];     /* Benutzer hat "immer erlauben" gewaehlt */
    int abort_turn;             /* Benutzer hat "Auftrag abbrechen" gewaehlt */
    ReadRec *reads;
    int nreads, capreads;
    Change *changes;
    int nchanges, capchanges;
    char session_id[40];        /* Name des Sicherungsverzeichnisses */
} ToolCtx;

int tools_init(ToolCtx *ctx, const Config *cfg, int mode);
void tools_cleanup(ToolCtx *ctx);

/* Tool-Definitionen im OpenAI-Format (Inhalt eines JSON-Arrays inkl. Klammern) */
const char *tools_json(void);

/* Fuehrt ein Tool aus. args: geparstes Argument-Objekt (UTF-8).
   out erhaelt das Ergebnis als ISO-8859-1-Text. */
void tools_run(ToolCtx *ctx, const char *name, const JNode *args, StrBuf *out);

/* Befehl ohne Freigabepruefung ausfuehren (manuelle Eingabe im Build-Log) */
void tools_exec(ToolCtx *ctx, const char *cmd, StrBuf *out);

/* Zuletzt geaenderte Datei auf den Stand vor ihrer ersten Aenderung zuruecksetzen */
void tools_undo(ToolCtx *ctx, StrBuf *out);
/* Alle Aenderungen dieser Sitzung gegen die Sicherungen zeigen */
void tools_diff(ToolCtx *ctx, StrBuf *out);

/* Alle "gelesen"-Vermerke verwerfen (nach /compact muss neu gelesen werden) */
void tools_forget_reads(ToolCtx *ctx);

/* Kurzbeschreibung eines Aufrufs fuer die Konsole (ISO-8859-1, max. len) */
void tools_describe(const char *name, const JNode *args, char *buf, int len);

/* Protokolliert eine Zeile mit Zeitstempel in .amicode/audit.log */
void tools_audit(ToolCtx *ctx, const char *fmt, ...);

const char *mode_name(int mode);
int mode_parse(const char *s);  /* -1 bei unbekanntem Namen */

#endif
