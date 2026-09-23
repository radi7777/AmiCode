/* AmiCodeIDE - MUI-Oberflaeche fuer den AmiCode-Agenten.
   Laeuft auf RTG und auf nativen PAL/NTSC-Modes: bei schmalen Bildschirmen
   werden Dateien, Editor und Agent als Register statt nebeneinander gezeigt. */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <libraries/gadtools.h>
#include <libraries/asl.h>
#include <libraries/iffparse.h>
#include <libraries/mui.h>
/* MUI 3.8-Header kennen IPTR nicht, die MCC-Header (NListtree) benutzen es */
#ifndef IPTR
#define IPTR ULONG
#endif
#include <mui/TextEditor_mcc.h>
#include <mui/NList_mcc.h>
#include <mui/NListview_mcc.h>
#include <mui/NListtree_mcc.h>
#include <mui/BetterString_mcc.h>
#include <mui/Busy_mcc.h>
#include <mui/TheBar_mcc.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>
#include <devices/inputevent.h>

#include "config.h"
#include "tools.h"
#include "ui.h"
#include "gui_agent.h"
#include "gui_prefs.h"
#include "gui_newproj.h"
#include "gui_projsel.h"
#include "gui_sessions.h"
#include "gui_skills.h"
#include "provider.h"

#define VERSION_STRING  "0.28"
#define NARROW_WIDTH    800     /* darunter Register statt drei Spalten */
#define MAX_EDIT_FILE   (512 * 1024)
#define MAX_TREE_DEPTH  6
#define MAX_TREE_NODES  2000
#define MAX_LOG_LINES   2000

static const char vers[] = "$VER: AmiCodeIDE " VERSION_STRING " (23.09.2026)";

unsigned long __stack = 65536;

struct Library *MUIMasterBase = NULL;

enum {
    ID_SEND = 1, ID_STOP, ID_OPEN_FILE, ID_SAVE_FILE, ID_PROJECT, ID_ABOUT, ID_QUIT,
    ID_RESET, ID_RESUME, ID_MODE, ID_RELOAD, ID_LOG_JUMP, ID_RUN_CMD, ID_CLEAR_LOG,
    ID_UNDO, ID_DIFF, ID_BUILD, ID_RUN, ID_TAB_SELECT, ID_CLOSE_FILE, ID_EDIT_CHANGED,
    ID_INPUT_CHANGED, ID_SLASH_PICK, ID_PREFS, ID_NEWPROJ, ID_GUIDE, ID_SESSIONS, ID_SKILLS
};

#define GUIDE_FILE  "PROGDIR:AmiCodeIDE.guide"

/* Befehle fuer die Auswahlliste beim Tippen von "/" (wie in opencode).
   arg: Befehl erwartet ein Argument, Return fuegt ihn nur ein. */
static const struct {
    const char *cmd;
    const char *desc;
    int arg;
} slash_cmds[] = {
    { "/help",    "alle Befehle", 0 },
    { "/status",  "Version, Modell, Projekt", 0 },
    { "/context", "Groesse der Unterhaltung", 0 },
    { "/tokens",  "verbrauchte Tokens", 0 },
    { "/model",   "Modell waehlen (Liste)", 1 },
    { "/compact", "Sitzung zusammenfassen", 1 },
    { "/prune",   "alte Tool-Ausgaben entfernen", 0 },
    { "/diff",    "Aenderungen anzeigen", 0 },
    { "/undo",    "letzte Aenderung zuruecknehmen", 0 },
    { "/reset",   "neue Sitzung", 0 },
    { "/sessions", "fruehere Sitzungen", 0 },
    { "/resume",  "Sitzung fortsetzen [nr]", 1 },
    { "/skills",  "Amiga-Wissen (Skills)", 0 },
    { "/new",     "neues Projekt (Assistent)", 0 },
    { "/toolchains", "installierte Compiler zeigen", 0 },
    { NULL, NULL, 0 }
};

/* Werkzeugleiste (TheBar.mcc im Textmodus; ohne TheBar normale Knoepfe) */
static struct MUIS_TheBar_Button tb_buttons[] = {
    { 0, ID_NEWPROJ,   "Ne_u",          "Neues Projekt anlegen (Assistent)", 0, 0, NULL, NULL },
    { 0, ID_PROJECT,   "Oe_ffnen",      "Anderes Projekt oeffnen", 0, 0, NULL, NULL },
    { MUIV_TheBar_BarSpacer, 0, NULL, NULL, 0, 0, NULL, NULL },
    { 0, ID_BUILD,     "_Bauen",        "Projekt bauen (Execute build, make oder [ui] build=)", 0, 0, NULL, NULL },
    { 0, ID_RUN,       "Sta_rten",      "Programm starten ([ui] run=)", 0, 0, NULL, NULL },
    { MUIV_TheBar_BarSpacer, 0, NULL, NULL, 0, 0, NULL, NULL },
    { 0, ID_SAVE_FILE, "Spei_chern",    "Datei im Editor speichern", 0, 0, NULL, NULL },
    { 0, ID_DIFF,      "_Diff",         "Aenderungen des Agenten in dieser Sitzung", 0, 0, NULL, NULL },
    { 0, ID_UNDO,      "_Undo",         "Zuletzt vom Agenten geaenderte Datei zuruecksetzen", 0, 0, NULL, NULL },
    { MUIV_TheBar_BarSpacer, 0, NULL, NULL, 0, 0, NULL, NULL },
    { 0, ID_STOP,      "St_op",         "Laufenden Auftrag oder Befehl abbrechen", 0, 0, NULL, NULL },
    { 0, ID_RESET,     "_Neue Sitzung", "Verlauf verwerfen und neu beginnen", 0, 0, NULL, NULL },
    { MUIV_TheBar_End, 0, NULL, NULL, 0, 0, NULL, NULL }
};

static struct NewMenu menu[] = {
    { NM_TITLE, "Projekt",                 NULL, 0, 0, NULL },
    { NM_ITEM,  "Neues Projekt...",        "N",  0, 0, (APTR)ID_NEWPROJ },
    { NM_ITEM,  "Projekt oeffnen...",      "O",  0, 0, (APTR)ID_PROJECT },
    { NM_ITEM,  "Dateiliste neu laden",    "R",  0, 0, (APTR)ID_RELOAD },
    { NM_ITEM,  "Datei speichern",         "S",  0, 0, (APTR)ID_SAVE_FILE },
    { NM_ITEM,  "Datei schliessen",        "W",  0, 0, (APTR)ID_CLOSE_FILE },
    { NM_ITEM,  "Build-Log leeren",        "L",  0, 0, (APTR)ID_CLEAR_LOG },
    { NM_ITEM,  NM_BARLABEL,               NULL, 0, 0, NULL },
    { NM_ITEM,  "Einstellungen...",        "E",  0, 0, (APTR)ID_PREFS },
    { NM_ITEM,  "Skills...",               "K",  0, 0, (APTR)ID_SKILLS },
    { NM_ITEM,  NM_BARLABEL,               NULL, 0, 0, NULL },
    { NM_ITEM,  "Anleitung...",            "H",  0, 0, (APTR)ID_GUIDE },
    { NM_ITEM,  "Ueber AmiCodeIDE...",        "?",  0, 0, (APTR)ID_ABOUT },
    { NM_ITEM,  NM_BARLABEL,               NULL, 0, 0, NULL },
    { NM_ITEM,  "Beenden",                 "Q",  0, 0, (APTR)ID_QUIT },
    { NM_TITLE, "Agent",                   NULL, 0, 0, NULL },
    { NM_ITEM,  "Auftrag senden",          NULL, 0, 0, (APTR)ID_SEND },
    { NM_ITEM,  "Stop",                    ".",  0, 0, (APTR)ID_STOP },
    { NM_ITEM,  NM_BARLABEL,               NULL, 0, 0, NULL },
    { NM_ITEM,  "Aenderungen anzeigen",    "D",  0, 0, (APTR)ID_DIFF },
    { NM_ITEM,  "Letzte Aenderung zuruecknehmen", "Z", 0, 0, (APTR)ID_UNDO },
    { NM_ITEM,  NM_BARLABEL,               NULL, 0, 0, NULL },
    { NM_ITEM,  "Neue Sitzung",            NULL, 0, 0, (APTR)ID_RESET },
    { NM_ITEM,  "Letzte Sitzung fortsetzen", NULL, 0, 0, (APTR)ID_RESUME },
    { NM_ITEM,  "Sitzungen...",            "J",  0, 0, (APTR)ID_SESSIONS },
    { NM_END,   NULL,                      NULL, 0, 0, NULL }
};

static const char *mode_labels[] = { "Safe", "Project", "Full", NULL };

static Object *app, *win, *files, *editor, *output, *input, *busy, *status, *tokentext;
static Object *projtext, *modecycle, *bt_send, *bt_stop, *modeltext, *prefswin;
static char *model_list;            /* zuletzt geholte Modelle (AllocVec), fuer /model */
static int slash_models;            /* Auswahlliste zeigt gerade Modelle */
static Object *buildlog, *cmdinput, *cmdlabel, *bt_run, *pages, *toolbar;
static int toolbar_is_thebar;
static int tree_nodes;
static int busy_is_mcc, agent_busy;
static char project[512];
static char current_file[512];

/* Offene Dateien. Ungespeicherter Text einer nicht angezeigten Datei liegt in
   text (AllocVec aus MUIM_TextEditor_ExportText). */
#define MAX_TABS 16
typedef struct {
    char path[512];
    char *text;
    LONG cursor;
    int changed;
} Tab;
static Tab tabs[MAX_TABS];
static int ntabs, cur_tab = -1;
static Object *openlist;
static char pending_project[512];   /* nach /new: dorthin wechseln, sobald der Agent frei ist */
static char *pending_first;         /* erster Auftrag im neuen Projekt (AllocVec) */
static Object *newprojwin, *projselwin, *sessionswin, *skillswin;
static Object *slashlist;           /* Auswahlliste fuer /befehle */
static Object *slashview;           /* ihr Listview (5 Zeilen hoch, ein-/ausgeblendet) */
static int slash_map[24];           /* Listenzeile -> Index in slash_cmds */
static int slash_count, slash_shown;
static BPTR project_lock, orig_dir;
static int orig_saved;
static Config cfg;
static Config projcfg;      /* .amicode/settings des Projekts (nur [ui]) */

#define PROJECT_SETTINGS ".amicode/settings"

/* Einstellung erst im Projekt, dann in der globalen Konfiguration suchen */
static void open_new_project(void);
static void remember_project(void);
static const char *projects_dir(void);

static const char *ui_setting(const char *key)
{
    const char *v = config_get(&projcfg, key, NULL);
    return v ? v : config_get(&cfg, key, NULL);
}

/* ---------- kleine Helfer (kein malloc: siehe gui_agent.h) ---------- */

static void set_status(const char *fmt, ...)
{
    static char buf[300];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    set(status, MUIA_Text_Contents, buf);
}

static void set_busy(int on)
{
    agent_busy = on;
    if (busy_is_mcc)
        set(busy, MUIA_Busy_Speed, on ? MUIV_Busy_Speed_User : MUIV_Busy_Speed_Off);
    else
        set(busy, MUIA_Text_Contents, on ? "\33c\33bdenke nach ..." : "");
}

/* Text unten an die Agent-Ausgabe haengen und hinscrollen */
static void out_append(const char *text)
{
    DoMethod(output, MUIM_TextEditor_InsertText, (ULONG)text, MUIV_TextEditor_InsertText_Bottom);
    /* Cursor ans Ende: TextEditor scrollt die Ansicht mit */
    DoMethod(output, MUIM_TextEditor_ARexxCmd, (ULONG)"GOTOBOTTOM");
}

/* Ereignis des Agenten darstellen. Escapes: \33b fett, \33i kursiv, \33n normal */
static void show_event(LONG kind, const char *text)
{
    static char line[1200];

    switch (kind) {
    case UI_STEP:   snprintf(line, sizeof(line), "\33i%s\33n\n", text); break;
    case UI_PROMPT: snprintf(line, sizeof(line), "\33b> %s\33n\n", text); break;
    case UI_TEXT:   snprintf(line, sizeof(line), "%s\n", text); break;
    case UI_TOOL:   snprintf(line, sizeof(line), "  \33b>\33n %s\n", text); break;
    case UI_DETAIL: snprintf(line, sizeof(line), "      \33i%s\33n\n", text); break;
    case UI_ERROR:  snprintf(line, sizeof(line), "\33b! %s\33n\n", text); break;
    case UI_DONE:   snprintf(line, sizeof(line), "\33i%s\33n\n\n", text); break;
    case UI_STREAM: out_append(text); return;   /* Stueck ohne eigenen Zeilenumbruch */
    default:        snprintf(line, sizeof(line), "%s\n", text); break;
    }
    if (kind == UI_TEXT && strlen(text) >= sizeof(line) - 2) {
        /* lange Antworten ungekuerzt einfuegen */
        out_append(text);
        out_append("\n");
    } else {
        out_append(line);
    }
}

/* Tokenzahl kurz: 540, 2.5k, 128k, 1.2M */
static void fmt_tokens(char *buf, int len, long n)
{
    if (n < 1000)
        snprintf(buf, len, "%ld", n);
    else if (n < 100000)
        snprintf(buf, len, "%ld.%ldk", n / 1000, (n % 1000) / 100);
    else if (n < 1000000)
        snprintf(buf, len, "%ldk", n / 1000);
    else
        snprintf(buf, len, "%ld.%ldM", n / 1000000, (n % 1000000) / 100000);
}

/* UI_TOKENS: "kontext limit ein aus cache kosten" -> Anzeige rechts in der Statuszeile.
   Ab 70% (dort fasst der Agent zusammen) fett. Kosten in Millionstel Dollar, -1 = unbekannt. */
static void show_tokens(const char *text)
{
    static char line[160];
    char ctx[16], lim[16], in[16], out[16], cache[16];
    long v[6] = { 0, 0, 0, 0, 0, -1 };
    const char *p = text;
    int i;

    for (i = 0; i < 6 && *p; i++) {
        v[i] = strtol(p, (char **)&p, 10);
        while (*p == ' ')
            p++;
    }
    fmt_tokens(ctx, sizeof(ctx), v[0]);
    fmt_tokens(lim, sizeof(lim), v[1]);
    fmt_tokens(in, sizeof(in), v[2]);
    fmt_tokens(out, sizeof(out), v[3]);
    fmt_tokens(cache, sizeof(cache), v[4]);
    if (v[1] > 0) {
        long pct = v[0] * 100 / v[1];
        snprintf(line, sizeof(line), "%sKontext %s/%s (%ld%%)%s | %s ein / %s aus",
                 pct >= 70 ? "\33b" : "", ctx, lim, pct, pct >= 70 ? "\33n" : "", in, out);
    } else {
        snprintf(line, sizeof(line), "Kontext %s | %s ein / %s aus", ctx, in, out);
    }
    if (v[4] > 0) {
        size_t l = strlen(line);
        snprintf(line + l, sizeof(line) - l, " (%s Cache)", cache);
    }
    if (v[5] >= 0) {
        /* Kosten nach vorn: lange Zeilen schneidet MUI rechts ab */
        static char with_cost[200];
        if (v[5] < 1000000)
            snprintf(with_cost, sizeof(with_cost), "\33b$0.%03ld\33n | %s", (v[5] + 500) / 1000, line);
        else
            snprintf(with_cost, sizeof(with_cost), "\33b$%ld.%02ld\33n | %s", v[5] / 1000000,
                     (v[5] % 1000000 + 5000) / 10000 % 100, line);
        set(tokentext, MUIA_Text_Contents, with_cost);
        return;
    }
    set(tokentext, MUIA_Text_Contents, line);
}

/* ---------- Dateien und Editor ---------- */

/* Verzeichnis rekursiv in den Baum einlesen. rel: Pfad relativ zum Projekt */
static void add_dir(const char *rel, struct MUI_NListtree_TreeNode *parent, int depth)
{
    struct FileInfoBlock *fib;
    BPTR lock;
    char name[120], path[512];

    if (depth > MAX_TREE_DEPTH || !(lock = Lock(rel, SHARED_LOCK)))
        return;
    if ((fib = AllocDosObject(DOS_FIB, NULL))) {
        if (Examine(lock, fib) && fib->fib_DirEntryType > 0) {
            while (tree_nodes < MAX_TREE_NODES && ExNext(lock, fib)) {
                struct MUI_NListtree_TreeNode *tn;
                int len = strlen(fib->fib_FileName);
                int is_dir = fib->fib_DirEntryType > 0;

                if (len > 5 && stricmp(fib->fib_FileName + len - 5, ".info") == 0)
                    continue;
                if (stricmp(fib->fib_FileName, "amicode.session") == 0 ||
                    stricmp(fib->fib_FileName, ".amicode") == 0)
                    continue;
                if (*rel)
                    snprintf(path, sizeof(path), "%s/%s", rel, fib->fib_FileName);
                else
                    snprintf(path, sizeof(path), "%s", fib->fib_FileName);
                snprintf(name, sizeof(name), is_dir ? "\33b%s" : "%s", fib->fib_FileName);
                tn = (struct MUI_NListtree_TreeNode *)DoMethod(files, MUIM_NListtree_Insert,
                        (ULONG)name, (ULONG)path, (ULONG)parent,
                        MUIV_NListtree_Insert_PrevNode_Sorted, is_dir ? TNF_LIST : 0);
                tree_nodes++;
                if (tn && is_dir)
                    add_dir(path, tn, depth + 1);
            }
        }
        FreeDosObject(DOS_FIB, fib);
    }
    UnLock(lock);
}

static void load_file_list(void)
{
    set(files, MUIA_NListtree_Quiet, TRUE);
    DoMethod(files, MUIM_NListtree_Clear, NULL, 0);
    tree_nodes = 0;
    {
        /* oberster Knoten: das Projekt selbst, aufgeklappt */
        static char rootname[120];
        struct MUI_NListtree_TreeNode *root;
        snprintf(rootname, sizeof(rootname), "\33b%s", FilePart(project));
        root = (struct MUI_NListtree_TreeNode *)DoMethod(files, MUIM_NListtree_Insert,
                    (ULONG)rootname, (ULONG)"", MUIV_NListtree_Insert_ListNode_Root,
                    MUIV_NListtree_Insert_PrevNode_Tail, TNF_LIST | TNF_OPEN);
        add_dir("", root ? root : (struct MUI_NListtree_TreeNode *)MUIV_NListtree_Insert_ListNode_Root, 0);
    }
    set(files, MUIA_NListtree_Quiet, FALSE);
}

/* ---------- Syntax-Hervorhebung ----------
   Nur Stile (fett/kursiv), keine Farben: sieht auch auf nativen Modes mit
   wenigen Farben gut aus. Wird beim Laden eingefuegt; gespeichert wird mit
   ExportHook_NoStyle, damit keine Stil-Codes in die Datei geraten. */

enum { LANG_NONE, LANG_C, LANG_ASM };

static const char *c_keywords[] = {
    "auto", "break", "case", "char", "const", "continue", "default", "do", "double",
    "else", "enum", "extern", "float", "for", "goto", "if", "inline", "int", "long",
    "register", "return", "short", "signed", "sizeof", "static", "struct", "switch",
    "typedef", "union", "unsigned", "void", "volatile", "while",
    "BYTE", "UBYTE", "WORD", "UWORD", "LONG", "ULONG", "BOOL", "APTR", "STRPTR",
    "BPTR", "TRUE", "FALSE", "NULL", NULL
};

static int lang_of(const char *name)
{
    const char *dot = strrchr(name, '.');

    if (!dot)
        return LANG_NONE;
    if (!stricmp(dot, ".c") || !stricmp(dot, ".h") || !stricmp(dot, ".cpp") ||
        !stricmp(dot, ".cc") || !stricmp(dot, ".hpp") || !stricmp(dot, ".cxx"))
        return LANG_C;
    if (!stricmp(dot, ".s") || !stricmp(dot, ".asm") || !stricmp(dot, ".i"))
        return LANG_ASM;
    return LANG_NONE;
}

static int is_ident(char c, int first)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' ||
           (!first && c >= '0' && c <= '9');
}

static int is_keyword(const char *s, int len)
{
    int i;
    for (i = 0; c_keywords[i]; i++)
        if ((int)strlen(c_keywords[i]) == len && strncmp(c_keywords[i], s, len) == 0)
            return 1;
    return 0;
}

#define EMIT(str)   do { const char *e_ = (str); while (*e_) *o++ = *e_++; } while (0)

/* Liefert AllocVec-Puffer mit Stil-Codes oder NULL (dann ungefaerbt laden) */
static char *highlight(const char *src, int lang)
{
    long n = strlen(src);
    char *out, *o;
    const char *p = src;
    int in_block = 0;

    if (lang == LANG_NONE || strchr(src, '\33'))
        return NULL;
    /* schlimmster Fall: leere Zeilen in Blockkommentaren */
    if (!(out = AllocVec(n * 5 + 64, MEMF_ANY)))
        return NULL;
    o = out;

    while (*p) {
        const char *eol = strchr(p, '\n');
        const char *end = eol ? eol : p + strlen(p);

        if (in_block && p < end)
            EMIT("\33i");
        if (lang == LANG_ASM) {
            const char *c = p;
            while (c < end && *c != ';' && !(c == p && *c == '*'))
                c++;
            memcpy(o, p, c - p);
            o += c - p;
            if (c < end) {
                EMIT("\33i");
                memcpy(o, c, end - c);
                o += end - c;
                EMIT("\33n");
            }
            p = end;
        } else {
            /* Praeprozessor: die Direktive fett */
            const char *s = p;
            while (s < end && (*s == ' ' || *s == '\t'))
                s++;
            if (!in_block && s < end && *s == '#') {
                const char *w = s + 1;
                while (w < end && (*w == ' ' || *w == '\t'))
                    w++;
                while (w < end && is_ident(*w, 0))
                    w++;
                memcpy(o, p, s - p);
                o += s - p;
                EMIT("\33b");
                memcpy(o, s, w - s);
                o += w - s;
                EMIT("\33n");
                p = w;
            }
            while (p < end) {
                if (in_block) {
                    if (p[0] == '*' && p + 1 < end && p[1] == '/') {
                        EMIT("*/\33n");
                        p += 2;
                        in_block = 0;
                    } else {
                        *o++ = *p++;
                    }
                } else if (p[0] == '/' && p + 1 < end && p[1] == '*') {
                    EMIT("\33i/*");
                    p += 2;
                    in_block = 1;
                } else if (p[0] == '/' && p + 1 < end && p[1] == '/') {
                    EMIT("\33i");
                    memcpy(o, p, end - p);
                    o += end - p;
                    EMIT("\33n");
                    p = end;
                } else if (*p == '"' || *p == '\'') {
                    char q = *p;
                    *o++ = *p++;
                    while (p < end && *p != q) {
                        if (*p == '\\' && p + 1 < end)
                            *o++ = *p++;
                        *o++ = *p++;
                    }
                    if (p < end)
                        *o++ = *p++;
                } else if (is_ident(*p, 1)) {
                    const char *w = p;
                    while (w < end && is_ident(*w, 0))
                        w++;
                    if (is_keyword(p, w - p)) {
                        EMIT("\33b");
                        memcpy(o, p, w - p);
                        o += w - p;
                        EMIT("\33n");
                    } else {
                        memcpy(o, p, w - p);
                        o += w - p;
                    }
                    p = w;
                } else {
                    *o++ = *p++;
                }
            }
            if (in_block && end > s)
                EMIT("\33n");      /* Stil gilt nur bis Zeilenende; naechste Zeile setzt neu */
        }
        if (eol) {
            *o++ = '\n';
            p = eol + 1;
        } else {
            p = end;
        }
    }
    *o = 0;
    return out;
}

/* Datei (relativ zum Projekt) von der Platte in den Editor laden. 1 = ok */
static int load_into_editor(const char *name)
{
    int ok = 0;
    BPTR fh;
    char *buf;
    LONG size;
    struct FileInfoBlock *fib;
    BPTR lock;

    if (!(lock = Lock(name, SHARED_LOCK))) {
        set_status("Kann '%s' nicht oeffnen", name);
        return 0;
    }
    size = -1;
    if ((fib = AllocDosObject(DOS_FIB, NULL))) {
        if (Examine(lock, fib) && fib->fib_DirEntryType < 0)
            size = fib->fib_Size;
        FreeDosObject(DOS_FIB, fib);
    }
    UnLock(lock);
    if (size < 0 || size > MAX_EDIT_FILE) {
        set_status("'%s' ist kein Textfile oder zu gross", name);
        return 0;
    }
    if (!(buf = AllocVec(size + 1, MEMF_ANY)))
        return 0;
    if ((fh = Open(name, MODE_OLDFILE))) {
        LONG n = Read(fh, buf, size);
        Close(fh);
        buf[n > 0 ? n : 0] = 0;
        if (memchr(buf, 0, n > 0 ? n : 0)) {
            set_status("'%s' ist eine Binaerdatei", name);
        } else {
            char *hl = highlight(buf, lang_of(name));
            set(editor, MUIA_TextEditor_Contents, hl ? hl : buf);
            if (hl)
                FreeVec(hl);
            set(editor, MUIA_TextEditor_HasChanged, FALSE);
            set_status("%s (%ld Bytes)", name, (long)n);
            ok = 1;
        }
    }
    FreeVec(buf);
    return ok;
}

/* ---------- Offene Dateien ---------- */

static int editor_changed(void)
{
    LONG c = FALSE;
    get(editor, MUIA_TextEditor_HasChanged, &c);
    return c != 0;
}

static int tab_is_changed(int i)
{
    return tabs[i].changed || (i == cur_tab && editor_changed());
}

static void tabs_refresh_list(void)
{
    static char line[520];
    int i;

    set(openlist, MUIA_NList_Quiet, TRUE);
    DoMethod(openlist, MUIM_NList_Clear);
    for (i = 0; i < ntabs; i++) {
        snprintf(line, sizeof(line), "%s%.500s", tab_is_changed(i) ? "* " : "  ", tabs[i].path);
        DoMethod(openlist, MUIM_NList_InsertSingle, (ULONG)line, MUIV_NList_Insert_Bottom);
    }
    nnset(openlist, MUIA_NList_Active, cur_tab >= 0 ? cur_tab : MUIV_NList_Active_Off);
    set(openlist, MUIA_NList_Quiet, FALSE);
}

/* Zustand der angezeigten Datei sichern, bevor eine andere gezeigt wird */
static void tab_store(void)
{
    Tab *t;

    if (cur_tab < 0)
        return;
    t = &tabs[cur_tab];
    get(editor, MUIA_TextEditor_CursorY, &t->cursor);
    if (editor_changed()) {
        char *txt = (char *)DoMethod(editor, MUIM_TextEditor_ExportText);
        if (txt) {
            if (t->text)
                FreeVec(t->text);
            t->text = txt;
            t->changed = 1;
        }
    }
}

static void tab_show(int i)
{
    Tab *t = &tabs[i];

    cur_tab = i;
    strcpy(current_file, t->path);
    if (t->text) {
        /* ungespeicherter Stand: wieder in den Editor, dort lebt er weiter */
        char *hl = highlight(t->text, lang_of(t->path));
        set(editor, MUIA_TextEditor_Contents, hl ? hl : t->text);
        if (hl)
            FreeVec(hl);
        set(editor, MUIA_TextEditor_HasChanged, TRUE);
        FreeVec(t->text);
        t->text = NULL;
        set_status("%s (ungespeichert)", t->path);
    } else {
        load_into_editor(t->path);
    }
    set(editor, MUIA_TextEditor_CursorY, t->cursor);
    tabs_refresh_list();
}

/* Datei oeffnen: vorhandenen Eintrag zeigen oder neuen anlegen */
static void open_file(const char *name)
{
    int i, prev = cur_tab;

    for (i = 0; i < ntabs; i++)
        if (stricmp(tabs[i].path, name) == 0)
            break;
    if (i < ntabs) {
        if (i != cur_tab) {
            tab_store();
            tab_show(i);
        }
        return;
    }
    if (ntabs == MAX_TABS) {
        set_status("Zu viele offene Dateien - erst eine schliessen (Amiga-W)");
        return;
    }
    tab_store();
    if (!load_into_editor(name)) {
        if (prev >= 0 && tabs[prev].text)
            tab_show(prev);     /* Editor zeigt weiter die vorige Datei */
        return;
    }
    memset(&tabs[ntabs], 0, sizeof(Tab));
    strncpy(tabs[ntabs].path, name, sizeof(tabs[ntabs].path) - 1);
    cur_tab = ntabs++;
    strcpy(current_file, name);
    if (prev >= 0 && tabs[prev].text)
        tabs[prev].changed = 1;
    tabs_refresh_list();
}

static void close_all_tabs(void)
{
    int i;
    for (i = 0; i < ntabs; i++)
        if (tabs[i].text)
            FreeVec(tabs[i].text);
    ntabs = 0;
    cur_tab = -1;
    current_file[0] = 0;
    set(editor, MUIA_TextEditor_Contents, "");
    set(editor, MUIA_TextEditor_HasChanged, FALSE);
    tabs_refresh_list();
}

static int count_unsaved(void)
{
    int i, n = 0;
    for (i = 0; i < ntabs; i++)
        if (tab_is_changed(i))
            n++;
    return n;
}

/* Rueckfrage bei ungespeicherten Aenderungen. 1 = darf verworfen werden */
static int confirm_discard(const char *what)
{
    int n = count_unsaved();

    if (!n)
        return 1;
    return MUI_Request(app, win, 0, "AmiCodeIDE", "_Verwerfen|_Abbrechen",
                       "\33c%ld Datei(en) mit ungespeicherten Aenderungen.\n%s",
                       (ULONG)n, (ULONG)what) == 1;
}

static void close_tab(void)
{
    int i;

    if (cur_tab < 0)
        return;
    if (tab_is_changed(cur_tab) &&
        MUI_Request(app, win, 0, "AmiCodeIDE", "_Verwerfen|_Abbrechen",
                    "\33c%s hat ungespeicherte Aenderungen.\nTrotzdem schliessen?",
                    (ULONG)tabs[cur_tab].path) != 1)
        return;
    if (tabs[cur_tab].text)
        FreeVec(tabs[cur_tab].text);
    for (i = cur_tab; i < ntabs - 1; i++)
        tabs[i] = tabs[i + 1];
    ntabs--;
    i = cur_tab < ntabs ? cur_tab : ntabs - 1;
    cur_tab = -1;
    set(editor, MUIA_TextEditor_HasChanged, FALSE);
    if (i >= 0) {
        tab_show(i);
    } else {
        current_file[0] = 0;
        set(editor, MUIA_TextEditor_Contents, "");
        set(editor, MUIA_TextEditor_HasChanged, FALSE);
        tabs_refresh_list();
    }
}

static void tab_select(void)
{
    LONG i = -1;

    get(openlist, MUIA_NList_Active, &i);
    if (i < 0 || i >= ntabs || i == cur_tab)
        return;
    tab_store();
    tab_show(i);
}

static void save_file(void)
{
    char *text;
    BPTR fh;

    if (!current_file[0]) {
        set_status("Keine Datei geoeffnet");
        return;
    }
    if (!(text = (char *)DoMethod(editor, MUIM_TextEditor_ExportText)))
        return;
    if ((fh = Open(current_file, MODE_NEWFILE))) {
        LONG y = 0;
        static char name[512];

        Write(fh, text, strlen(text));
        Close(fh);
        /* neu laden, damit die Hervorhebung wieder stimmt; Cursorzeile behalten */
        get(editor, MUIA_TextEditor_CursorY, &y);
        strcpy(name, current_file);
        load_into_editor(name);
        set(editor, MUIA_TextEditor_CursorY, y);
        if (cur_tab >= 0)
            tabs[cur_tab].changed = 0;
        tabs_refresh_list();
        set_status("%s gespeichert", current_file);
    } else {
        set_status("Kann %s nicht schreiben", current_file);
    }
    FreeVec(text);
}

/* Nach einem Auftrag: Liste und unveraenderte Datei neu laden */
static void refresh_after_agent(void)
{
    LONG changed = FALSE;

    load_file_list();
    /* der Agent kann .amicode/settings geaendert haben (build=/run=); er ist jetzt
       untaetig, daher darf hier config_load (malloc) laufen */
    config_free(&projcfg);
    config_load(&projcfg, PROJECT_SETTINGS);
    get(editor, MUIA_TextEditor_HasChanged, &changed);
    if (current_file[0] && !changed) {
        LONG y = 0;
        get(editor, MUIA_TextEditor_CursorY, &y);
        load_into_editor(current_file);
        set(editor, MUIA_TextEditor_CursorY, y);
    }
}

static void file_doubleclick(void)
{
    struct MUI_NListtree_TreeNode *tn = NULL;

    get(files, MUIA_NListtree_Active, &tn);
    if (!tn || (tn->tn_Flags & TNF_LIST) || !tn->tn_User)
        return;     /* Verzeichnisse klappt NListtree selbst auf */
    open_file((const char *)tn->tn_User);
    if (pages)
        set(pages, MUIA_Group_ActivePage, 1);   /* Register: zum Editor */
}

/* ---------- Build-Log ---------- */

/* Enthaelt die Zeile eine Fehler- oder Warnungsmeldung? */
static int is_diag(const char *s)
{
    return strstr(s, "error") || strstr(s, "Error") || strstr(s, "warning") ||
           strstr(s, "Warning") || strstr(s, "failed");
}

/* Mehrzeilige Ausgabe an das Build-Log haengen */
static void log_append(const char *text)
{
    static char line[260];
    LONG entries = 0;

    set(buildlog, MUIA_NList_Quiet, TRUE);
    while (*text) {
        const char *e = strchr(text, '\n');
        int len = e ? (int)(e - text) : (int)strlen(text);
        int max = sizeof(line) - 4;
        const char *raw = text;

        text = e ? e + 1 : text + len;
        if (len > max)
            len = max;
        memcpy(line + 2, raw, len);
        line[2 + len] = 0;
        if (is_diag(line + 2)) {
            line[0] = '\33';
            line[1] = 'b';
            DoMethod(buildlog, MUIM_NList_InsertSingle, (ULONG)line, MUIV_NList_Insert_Bottom);
        } else {
            DoMethod(buildlog, MUIM_NList_InsertSingle, (ULONG)(line + 2), MUIV_NList_Insert_Bottom);
        }
    }
    get(buildlog, MUIA_NList_Entries, &entries);
    while (entries-- > MAX_LOG_LINES)
        DoMethod(buildlog, MUIM_NList_Remove, MUIV_NList_Remove_First);
    set(buildlog, MUIA_NList_Quiet, FALSE);
    DoMethod(buildlog, MUIM_NList_Jump, MUIV_NList_Jump_Bottom);
}

/* Datei und Zeile aus einer Compilermeldung lesen:
   VBCC:  error 82 in line 18 of "hello.c": ...
   GCC:   hello.c:18:5: error: ...
   SAS/C: hello.c 18 Error 55: ... */
static int parse_location(const char *s, char *file, int flen, LONG *line)
{
    const char *p, *q;

    if ((p = strstr(s, " in line ")) && (q = strstr(p, " of \""))) {
        const char *end;
        *line = atol(p + 9);
        q += 5;
        if ((end = strchr(q, '"')) && end - q < flen) {
            memcpy(file, q, end - q);
            file[end - q] = 0;
            return *line > 0;
        }
    }
    for (p = s; *p == ' ' || *p == '\t'; p++)
        ;
    for (q = p; *q && *q != ':' && *q != ' '; q++)
        ;
    if (q > p && q - p < flen && ((*q == ':' && q[1] >= '0' && q[1] <= '9') ||
                                  (*q == ' ' && q[1] >= '0' && q[1] <= '9'))) {
        memcpy(file, p, q - p);
        file[q - p] = 0;
        *line = atol(q + 1);
        return *line > 0 && strchr(file, '.') != NULL;
    }
    return 0;
}

/* Meldungen mit Zeile, aber ohne Datei in derselben Zeile:
   AmiBlitz3: "Compiler Error #1 in <datei>:" / naechste Zeile "Line 4: ..."
   Amiga E:   "LINE 3: ..."           PureBasic: "Error: Line 1 - ..."
   Die Datei steht dann einige Zeilen darueber (AmiBlitz) oder es ist die
   Hauptdatei des Projekts ([ui] main= in .amicode/settings). */
static int parse_line_only(const char *s, LONG index, char *file, int flen, LONG *line)
{
    const char *p = s, *main;
    int k;

    while (*p == ' ')
        p++;
    if (strnicmp(p, "Error: Line ", 12) == 0)
        p += 12;
    else if (strnicmp(p, "Line ", 5) == 0)
        p += 5;
    else
        return 0;
    if ((*line = atol(p)) <= 0)
        return 0;

    /* AmiBlitz: Datei in einer der vorigen Zeilen "... in <datei>:" */
    for (k = 1; k <= 5 && index - k >= 0; k++) {
        char *prev = NULL, *a, *b;
        DoMethod(buildlog, MUIM_NList_GetEntry, index - k, (ULONG)&prev);
        if (prev && (a = strstr(prev, " in <")) && (b = strchr(a + 5, '>')) && b - (a + 5) < flen) {
            memcpy(file, a + 5, b - (a + 5));
            file[b - (a + 5)] = 0;
            return 1;
        }
    }
    if ((main = ui_setting("ui.main")) && *main) {
        strncpy(file, main, flen - 1);
        file[flen - 1] = 0;
        return 1;
    }
    return 0;
}

static void log_jump(void)
{
    char *entry = NULL, file[256];
    LONG line;

    LONG active = -1;

    get(buildlog, MUIA_NList_Active, &active);
    DoMethod(buildlog, MUIM_NList_GetEntry, MUIV_NList_GetEntry_Active, (ULONG)&entry);
    if (!entry)
        return;
    if (entry[0] == '\33')
        entry += 2;
    if (!parse_location(entry, file, sizeof(file), &line) &&
        !parse_line_only(entry, active, file, sizeof(file), &line)) {
        set_status("Keine Datei/Zeile in dieser Meldung erkannt");
        return;
    }
    open_file(file);
    if (strcmp(current_file, file) == 0) {
        set(editor, MUIA_TextEditor_CursorY, line - 1);
        set(win, MUIA_Window_ActiveObject, editor);
        if (pages)
            set(pages, MUIA_Group_ActivePage, 1);
        set_status("%s, Zeile %ld", file, (long)line);
    }
}

/* ---------- 1>-Zeile: History mit Cursor hoch/runter ---------- */

#define HIST_MAX 30
static char hist[HIST_MAX][256];
static int hist_n, hist_pos;

static void hist_add(const char *cmd)
{
    if (hist_n && strcmp(hist[hist_n - 1], cmd) == 0) {
        hist_pos = hist_n;
        return;
    }
    if (hist_n == HIST_MAX) {
        memmove(hist[0], hist[1], sizeof(hist[0]) * (HIST_MAX - 1));
        hist_n--;
    }
    strncpy(hist[hist_n], cmd, sizeof(hist[0]) - 1);
    hist[hist_n][sizeof(hist[0]) - 1] = 0;
    hist_pos = ++hist_n;
}

static void hist_step(int dir)
{
    const char *text;

    if (dir < 0 && hist_pos > 0)
        hist_pos--;
    else if (dir > 0 && hist_pos < hist_n)
        hist_pos++;
    else
        return;
    text = hist_pos < hist_n ? hist[hist_pos] : "";
    set(cmdinput, MUIA_String_Contents, text);
    set(cmdinput, MUIA_String_BufferPos, strlen(text));
}

/* Unsichtbares Objekt, das Cursor hoch/runter abfaengt, solange die 1>-Zeile aktiv ist.
   Eigener Event-Handler mit hoher Prioritaet, damit BetterString die Tasten nicht bekommt. */
struct KeyCatchData {
    struct MUI_EventHandlerNode ehn;
};
static struct MUI_CustomClass *keycatch_class;

/* MUI ruft den Dispatcher mit Argumenten in a0/a2/a1 auf */
static ULONG keycatch_dispatch(struct IClass *cl __asm("a0"), Object *obj __asm("a2"), Msg msg __asm("a1"))
{
    struct KeyCatchData *data;

    switch (msg->MethodID) {
    case MUIM_Setup:
        if (!DoSuperMethodA(cl, obj, msg))
            return FALSE;
        data = INST_DATA(cl, obj);
        data->ehn.ehn_Priority = 50;
        data->ehn.ehn_Flags = 0;
        data->ehn.ehn_Object = obj;
        data->ehn.ehn_Class = cl;
        data->ehn.ehn_Events = IDCMP_RAWKEY;
        DoMethod(_win(obj), MUIM_Window_AddEventHandler, (ULONG)&data->ehn);
        return TRUE;
    case MUIM_Cleanup:
        data = INST_DATA(cl, obj);
        DoMethod(_win(obj), MUIM_Window_RemEventHandler, (ULONG)&data->ehn);
        break;
    case MUIM_HandleEvent: {
        struct IntuiMessage *im = ((struct MUIP_HandleEvent *)msg)->imsg;
        Object *active = NULL;

        if (!im || im->Class != IDCMP_RAWKEY || (im->Qualifier & (IEQUALIFIER_LSHIFT |
                IEQUALIFIER_RSHIFT | IEQUALIFIER_CONTROL | IEQUALIFIER_LALT | IEQUALIFIER_RALT)))
            return 0;
        if (im->Code != 0x4C && im->Code != 0x4D)
            return 0;
        get(_win(obj), MUIA_Window_ActiveObject, &active);
        if (active != cmdinput)
            return 0;
        hist_step(im->Code == 0x4C ? -1 : 1);
        return MUI_EventHandlerRC_Eat;
    }
    }
    return DoSuperMethodA(cl, obj, msg);
}

static Object *keycatch_new(void)
{
    static struct TagItem tags[] = {
        { MUIA_HorizWeight, 0 }, { MUIA_FixWidth, 0 }, { TAG_DONE, 0 }
    };

    if (!keycatch_class)
        keycatch_class = MUI_CreateCustomClass(NULL, MUIC_Rectangle, NULL,
                                               sizeof(struct KeyCatchData), (APTR)keycatch_dispatch);
    if (keycatch_class)
        return NewObjectA(keycatch_class->mcc_Class, NULL, tags);
    return HSpace(1);
}

/* Anzeige vor der 1>-Zeile: letzter Teil des Shell-Verzeichnisses */
static void show_cwd(const char *dir)
{
    static char label[48];
    const char *part = FilePart((STRPTR)dir);

    if (!*part)
        part = dir;
    /* feste Breite (12 Zeichen): lange Namen von hinten zeigen */
    if (strlen(part) > 11)
        part += strlen(part) - 11;
    snprintf(label, sizeof(label), "%s>", part);
    set(cmdlabel, MUIA_Text_Contents, label);
}

static void run_manual_command(void)
{
    char *cmd = NULL;

    get(cmdinput, MUIA_String_Contents, &cmd);
    if (!cmd || !*cmd)
        return;
    if (agent_busy) {
        set_status("Der Agent arbeitet noch - Stop oder warten");
        return;
    }
    hist_add(cmd);
    gui_agent_cmdline(cmd);
    set(cmdinput, MUIA_String_Contents, "");
    set(win, MUIA_Window_ActiveObject, cmdinput);   /* Return deaktiviert die Zeile */
    set_busy(1);
    set_status("Befehl laeuft ...");
}

/* ---------- Agent ---------- */

/* ---------- Auswahlliste fuer /befehle ---------- */

static void slash_hide(void)
{
    if (slash_shown) {
        set(slashview, MUIA_ShowMe, FALSE);
        slash_shown = 0;
    }
}

/* Nach jeder Aenderung der Eingabe: passende Befehle zeigen */
static void slash_update(void)
{
    static char line[120];
    char *text = NULL;
    int i, len;

    get(input, MUIA_String_Contents, &text);
    /* "/model <filter>": geholte Modelle anbieten */
    if (text && strnicmp(text, "/model ", 7) == 0 && model_list) {
        const char *flt = text + 7, *p = model_list;
        int fl = strlen(flt);
        set(slashlist, MUIA_NList_Quiet, TRUE);
        DoMethod(slashlist, MUIM_NList_Clear);
        slash_count = 0;
        while (*p) {
            const char *e = strchr(p, '\n');
            int len = e ? (int)(e - p) : (int)strlen(p), k, hit = fl == 0;
            for (k = 0; !hit && k + fl <= len; k++)
                hit = strnicmp(p + k, flt, fl) == 0;
            if (hit && len < 110) {
                snprintf(line, sizeof(line), "%.*s", len, p);
                DoMethod(slashlist, MUIM_NList_InsertSingle, (ULONG)line, MUIV_NList_Insert_Bottom);
                slash_count++;
            }
            if (!e)
                break;
            p = e + 1;
        }
        slash_models = 1;
        nnset(slashlist, MUIA_NList_Active, slash_count ? 0 : MUIV_NList_Active_Off);
        set(slashlist, MUIA_NList_Quiet, FALSE);
        if (slash_count && !slash_shown) {
            set(slashview, MUIA_ShowMe, TRUE);
            slash_shown = 1;
        } else if (!slash_count) {
            slash_hide();
        }
        return;
    }
    slash_models = 0;
    if (!text || text[0] != '/' || strchr(text, ' ')) {
        slash_hide();
        return;
    }
    len = strlen(text);
    set(slashlist, MUIA_NList_Quiet, TRUE);
    DoMethod(slashlist, MUIM_NList_Clear);
    slash_count = 0;
    for (i = 0; slash_cmds[i].cmd; i++) {
        if (strnicmp(slash_cmds[i].cmd, text, len) != 0)
            continue;
        snprintf(line, sizeof(line), "\33b%-9s\33n %s", slash_cmds[i].cmd, slash_cmds[i].desc);
        DoMethod(slashlist, MUIM_NList_InsertSingle, (ULONG)line, MUIV_NList_Insert_Bottom);
        slash_map[slash_count++] = i;
    }
    nnset(slashlist, MUIA_NList_Active, slash_count ? 0 : MUIV_NList_Active_Off);
    set(slashlist, MUIA_NList_Quiet, FALSE);
    if (slash_count && !slash_shown) {
        set(slashview, MUIA_ShowMe, TRUE);
        slash_shown = 1;
    } else if (!slash_count) {
        slash_hide();
    }
}

/* Gewaehlten Befehl uebernehmen. Liefert 1, wenn er sofort gesendet werden soll. */
static int slash_pick(void)
{
    static char buf[160];
    LONG active = -1;
    int c;

    if (!slash_shown)
        return 0;
    get(slashlist, MUIA_NList_Active, &active);
    if (active < 0 || active >= slash_count)
        return 0;
    if (slash_models) {
        char *entry = NULL;
        DoMethod(slashlist, MUIM_NList_GetEntry, active, (ULONG)&entry);
        slash_hide();
        if (!entry)
            return 0;
        snprintf(buf, sizeof(buf), "/model %s", entry);
        nnset(input, MUIA_String_Contents, buf);
        return 1;
    }
    c = slash_map[active];
    slash_hide();
    if (stricmp(slash_cmds[c].cmd, "/model") == 0 && !agent_busy) {
        /* verfuegbare Modelle holen; die Liste erscheint, sobald sie da ist */
        gui_agent_prompt("/model");
        set_busy(1);
        set_status("Hole Modellliste ...");
    }
    if (slash_cmds[c].arg) {
        snprintf(buf, sizeof(buf), "%s ", slash_cmds[c].cmd);
        nnset(input, MUIA_String_Contents, buf);
        set(input, MUIA_String_BufferPos, strlen(buf));
        set(win, MUIA_Window_ActiveObject, input);
        return 0;
    }
    nnset(input, MUIA_String_Contents, slash_cmds[c].cmd);
    return 1;
}

static void send_prompt(void)
{
    char *text = NULL;
    static char echo[1100];

    get(input, MUIA_String_Contents, &text);
    if (!text || !*text)
        return;
    if (agent_busy) {
        set_status("Der Agent arbeitet noch - Stop oder warten");
        return;
    }
    slash_hide();
    if (stricmp(text, "/new") == 0) {
        set(input, MUIA_String_Contents, "");
        open_new_project();
        return;
    }
    snprintf(echo, sizeof(echo), "\33b> %s\33n\n", text);
    out_append(echo);
    gui_agent_prompt(text);
    set(input, MUIA_String_Contents, "");
    set_busy(1);
    set_status("Agent arbeitet ...");
}

static int switch_pending;
static int startup_choose;

/* Fenster "Sitzungen": gewaehlte Sitzung fortsetzen oder loeschen */
static void session_action(int what)
{
    static char cmd[80];
    const char *name = sessions_selected();

    if (agent_busy) {
        set_status("Erst den laufenden Auftrag beenden");
        return;
    }
    if (what == SS_DELETE) {
        if (!MUI_Request(app, sessionswin, 0, "AmiCodeIDE - Sitzung loeschen", "_Loeschen|_Abbrechen",
                         "Sitzung %s endgueltig loeschen?", (ULONG)name))
            return;
        gui_agent_sessions(name);
    } else {
        DoMethod(output, MUIM_TextEditor_ClearText);
        snprintf(cmd, sizeof(cmd), "/resume %s", name);
        gui_agent_prompt(cmd);
    }
    set_busy(1);
}

static void handle_events(struct MsgPort *port)
{
    GuiEvent *ev;

    while ((ev = (GuiEvent *)GetMsg(port))) {
        if (ev->kind == EV_ASK) {
            /* Ablehnen steht rechts: Esc und Schliessen ergeben 0 = ablehnen */
            LONG r = MUI_Request(app, win, 0, "AmiCodeIDE - Freigabe",
                                 "_Erlauben|_Immer|Auftrag _stoppen|_Ablehnen", "\33l%s", (ULONG)ev->text);
            ev->answer = r == 1 ? ASK_YES : r == 2 ? ASK_ALWAYS : r == 3 ? ASK_ABORT : ASK_NO;
            ReplyMsg(&ev->msg);
            continue;
        }
        switch (ev->kind) {
        case EV_IDLE:
            set_busy(0);
            set_status("Bereit");
            if (pending_project[0])
                switch_pending = 1;     /* erst nach der Schleife: der Agent-Prozess wird neu gestartet */
            else
                refresh_after_agent();
            break;
        case UI_PROJECT:
            strncpy(pending_project, ev->text, sizeof(pending_project) - 1);
            break;
        case UI_CHOICES:
            newproj_choices(ev->text);
            break;
        case UI_MODELS: {
            char *nl = strchr(ev->text, '\n');
            if (!nl)
                break;
            if (strncmp(ev->text, "prefs", 5) == 0) {
                prefs_models(nl + 1);
            } else {
                if (model_list)
                    FreeVec(model_list);
                if ((model_list = AllocVec(strlen(nl + 1) + 1, MEMF_ANY)))
                    strcpy(model_list, nl + 1);
                slash_update();
            }
            break;
        }
        case UI_BUSY:
            set_busy(ev->text[0] == '1');
            break;
        case UI_TOKENS:
            show_tokens(ev->text);
            break;
        case UI_SESSIONS:
            sessions_fill(ev->text);
            break;
        case UI_CWD:
            show_cwd(ev->text);
            break;
        case UI_OUTPUT:
            log_append(ev->text);
            break;
        default:
            show_event(ev->kind, ev->text);
            break;
        }
        FreeVec(ev);
    }
}

/* ---------- Projekt ---------- */

static int set_project(const char *dir)
{
    BPTR lock = Lock(dir, SHARED_LOCK);

    if (!lock)
        return 0;
    if (project_lock) {
        CurrentDir(lock);
        UnLock(project_lock);
    } else {
        orig_dir = CurrentDir(lock);
        orig_saved = 1;
    }
    project_lock = lock;
    NameFromLock(lock, project, sizeof(project));
    /* nur bei gestopptem Agent-Prozess aufrufen: config_load benutzt malloc */
    config_free(&projcfg);
    config_load(&projcfg, PROJECT_SETTINGS);
    set(projtext, MUIA_Text_Contents, project);
    remember_project();
    close_all_tabs();
    load_file_list();
    return 1;
}

static int start_agent(struct MsgPort *events)
{
    LONG m = 1;

    get(modecycle, MUIA_Cycle_Active, &m);
    if (!gui_agent_start(&cfg, project_lock, (int)m, events)) {
        out_append("\33b! Agent-Prozess konnte nicht gestartet werden\33n\n");
        return 0;
    }
    set(tokentext, MUIA_Text_Contents, "Tokens: -");    /* neuer Agent zaehlt von vorn */
    set_busy(1);    /* bis der Agent sein Netzwerk geoeffnet hat */
    return 1;
}

/* Projektordner: [ui] projects= oder <Programmverzeichnis>/Projects (wird angelegt) */
static const char *projects_dir(void)
{
    static char dir[512];
    const char *cfgdir = ui_setting("ui.projects");
    BPTR lock;

    if (cfgdir && *cfgdir) {
        strncpy(dir, cfgdir, sizeof(dir) - 1);
        dir[sizeof(dir) - 1] = 0;
    } else {
        dir[0] = 0;
        NameFromLock(GetProgramDir(), dir, sizeof(dir));
        AddPart(dir, "Projects", sizeof(dir));
    }
    if ((lock = Lock(dir, SHARED_LOCK)))
        UnLock(lock);
    else if ((lock = CreateDir(dir)))
        UnLock(lock);
    return dir;
}

/* Zuletzt benutztes Projekt merken (ENV: und ENVARC:) */
static void remember_project(void)
{
    static const char *files[] = { "ENV:AmiCode/lastproject", "ENVARC:AmiCode/lastproject", NULL };
    BPTR fh;
    int i;

    if (stricmp(project, projects_dir()) == 0)
        return;     /* nur der Platzhalter beim Start ohne Projekt */
    for (i = 0; files[i]; i++)
        if ((fh = Open(files[i], MODE_NEWFILE))) {
            Write(fh, project, strlen(project));
            Close(fh);
        }
}

static int last_project(char *dir, int len)
{
    BPTR fh = Open("ENV:AmiCode/lastproject", MODE_OLDFILE), lock;
    LONG n;

    if (!fh)
        fh = Open("ENVARC:AmiCode/lastproject", MODE_OLDFILE);
    if (!fh)
        return 0;
    n = Read(fh, dir, len - 1);
    Close(fh);
    if (n <= 0)
        return 0;
    dir[n] = 0;
    while (n > 0 && (dir[n - 1] == '\n' || dir[n - 1] == ' '))
        dir[--n] = 0;
    if (!(lock = Lock(dir, SHARED_LOCK)))
        return 0;
    UnLock(lock);
    return 1;
}

/* In ein anderes Projekt wechseln: Agent stoppen, Projekt setzen, Agent starten */
static void switch_to_project(const char *newdir, struct MsgPort *events)
{
    static char dir[600];

    if (agent_busy) {
        set_status("Erst den laufenden Auftrag beenden");
        return;
    }
    if (!confirm_discard("Beim Projektwechsel gehen sie verloren."))
        return;
    strncpy(dir, newdir, sizeof(dir) - 1);
    dir[sizeof(dir) - 1] = 0;
    gui_agent_stop();
    if (set_project(dir)) {
        out_append("\n\33bProjekt:\33n ");
        out_append(project);
        out_append("\n");
    } else {
        set_status("Projekt %s nicht gefunden", dir);
    }
    start_agent(events);
}

/* Ausnahme: beliebige Schublade als Projekt oeffnen */
static void choose_project(struct MsgPort *events)
{
    struct FileRequester *req;

    req = MUI_AllocAslRequestTags(ASL_FileRequest,
                                  ASLFR_TitleText, (ULONG)"Schublade als Projekt oeffnen",
                                  ASLFR_DrawersOnly, TRUE,
                                  ASLFR_InitialDrawer, (ULONG)project,
                                  TAG_DONE);
    if (!req)
        return;
    if (MUI_AslRequestTags(req, TAG_DONE) && req->fr_Drawer && *req->fr_Drawer)
        switch_to_project(req->fr_Drawer, events);
    MUI_FreeAslRequest(req);
}

/* ---------- Bauen und Starten ---------- */

static int file_exists(const char *name)
{
    BPTR lock = Lock(name, SHARED_LOCK);
    if (lock)
        UnLock(lock);
    return lock != 0;
}

/* Build-Befehl: [ui] build=, sonst nach vorhandenen Dateien raten */
static const char *build_command(void)
{
    const char *cmd = ui_setting("ui.build");

    if (cmd && *cmd)
        return cmd;
    if (file_exists("build"))
        return "Execute build";
    if (file_exists("Makefile") || file_exists("makefile") || file_exists("GNUmakefile"))
        return "make";
    if (file_exists("smakefile"))
        return "smake";
    return NULL;
}

static void run_tool_command(int id)
{
    const char *cmd = id == ID_BUILD ? build_command() : ui_setting("ui.run");

    if (agent_busy) {
        set_status("Der Agent arbeitet noch - Stop oder warten");
        return;
    }
    if (!cmd || !*cmd) {
        set_status(id == ID_BUILD ? "Kein Build gefunden: [ui] build=... in der Konfiguration setzen"
                                  : "Kein Startbefehl: [ui] run=... in " PROJECT_SETTINGS " setzen");
        return;
    }
    if (pages)
        set(pages, MUIA_Group_ActivePage, 3);   /* Register: zum Build-Log */
    gui_agent_shell(cmd);
    set_busy(1);
    set_status("%s ...", cmd);
}

/* ---------- Aufbau ---------- */

static Object *build_toolbar(void)
{
    Object *grp;
    int i;

    toolbar = MUI_NewObject(MUIC_TheBar,
                            MUIA_Group_Horiz, TRUE,
                            MUIA_TheBar_Buttons, (ULONG)tb_buttons,
                            MUIA_TheBar_ViewMode, MUIV_TheBar_ViewMode_Text,
                            MUIA_TheBar_EnableKeys, TRUE,
                            MUIA_TheBar_Frame, FALSE,
                            TAG_DONE);
    if (toolbar) {
        toolbar_is_thebar = 1;
        return toolbar;
    }
    /* Ersatz ohne TheBar.mcc: eine Reihe einfacher Knoepfe */
    grp = MUI_NewObject(MUIC_Group, MUIA_Group_Horiz, TRUE, TAG_DONE);
    if (!grp)
        return NULL;
    for (i = 0; tb_buttons[i].img != MUIV_TheBar_End; i++) {
        Object *o;
        if (tb_buttons[i].img == MUIV_TheBar_BarSpacer)
            o = MUI_NewObject(MUIC_Rectangle, MUIA_Rectangle_VBar, TRUE, MUIA_FixWidth, 8, TAG_DONE);
        else
            o = tb_buttons[i].obj = SimpleButton(tb_buttons[i].text);
        if (o)
            DoMethod(grp, OM_ADDMEMBER, (ULONG)o);
    }
    DoMethod(grp, OM_ADDMEMBER, (ULONG)MUI_NewObject(MUIC_Rectangle, TAG_DONE));  /* Rest fuellen */
    toolbar = grp;
    return grp;
}

static void notify_toolbar(void)
{
    int i;

    for (i = 0; tb_buttons[i].img != MUIV_TheBar_End; i++) {
        ULONG id = tb_buttons[i].ID;
        if (tb_buttons[i].img == MUIV_TheBar_BarSpacer)
            continue;
        if (toolbar_is_thebar)
            DoMethod(toolbar, MUIM_TheBar_Notify, id, MUIA_Pressed, FALSE,
                     (ULONG)app, 2, MUIM_Application_ReturnID, id);
        else if (tb_buttons[i].obj)
            DoMethod(tb_buttons[i].obj, MUIM_Notify, MUIA_Pressed, FALSE,
                     (ULONG)app, 2, MUIM_Application_ReturnID, id);
    }
}

/* TextEditor mit eigener Scrollleiste */
static Object *editor_with_slider(Object **ed, int readonly)
{
    Object *slider = ScrollbarObject, End;

    if (!slider)
        return NULL;
    *ed = MUI_NewObject(MUIC_TextEditor,
                        MUIA_TextEditor_Slider, (ULONG)slider,
                        MUIA_TextEditor_ReadOnly, readonly,
                        MUIA_TextEditor_FixedFont, !readonly,
                        /* Quelltext: Tabs erhalten (Makefiles!), keine harten Umbrueche,
                           beim Export alle Stil-Codes der Hervorhebung entfernen */
                        MUIA_TextEditor_ConvertTabs, FALSE,
                        MUIA_TextEditor_WrapMode, MUIV_TextEditor_WrapMode_SoftWrap,
                        MUIA_TextEditor_ExportHook, MUIV_TextEditor_ExportHook_NoStyle,
                        MUIA_CycleChain, 1,
                        TAG_DONE);
    if (!*ed) {
        MUI_DisposeObject(slider);
        return NULL;
    }
    return HGroup, MUIA_Group_Spacing, 0,
        Child, *ed,
        Child, slider,
    End;
}

static Object *build_window(int narrow)
{
    Object *files_lv, *edit_grp, *out_grp, *agent_grp, *main_grp, *log_lv, *log_grp;

    files = MUI_NewObject(MUIC_NListtree,
                          MUIA_Frame, MUIV_Frame_InputList,
                          MUIA_NListtree_ConstructHook, MUIV_NListtree_ConstructHook_String,
                          MUIA_NListtree_DestructHook, MUIV_NListtree_DestructHook_String,
                          MUIA_NListtree_CompareHook, MUIV_NListtree_CompareHook_LeavesBottom,
                          MUIA_NListtree_DoubleClick, MUIV_NListtree_DoubleClick_All,
                          MUIA_NListtree_EmptyNodes, TRUE,
                          TAG_DONE);
    if (!files)
        return NULL;
    files_lv = MUI_NewObject(MUIC_NListview, MUIA_NListview_NList, (ULONG)files,
                             MUIA_CycleChain, 1, TAG_DONE);
    openlist = MUI_NewObject(MUIC_NList,
                             MUIA_Frame, MUIV_Frame_InputList,
                             MUIA_NList_ConstructHook, MUIV_NList_ConstructHook_String,
                             MUIA_NList_DestructHook, MUIV_NList_DestructHook_String,
                             TAG_DONE);
    if (!openlist || !files_lv)
        return NULL;
    files_lv = VGroup,
        Child, VGroup, MUIA_VertWeight, 70, Child, files_lv, End,
        Child, BalanceObject, End,
        Child, VGroup, MUIA_VertWeight, 30,
            Child, TextObject, MUIA_Text_Contents, "Offene Dateien", MUIA_Font, MUIV_Font_Tiny, End,
            Child, MUI_NewObject(MUIC_NListview, MUIA_NListview_NList, (ULONG)openlist,
                                 MUIA_CycleChain, 1, TAG_DONE),
        End,
    End;

    /* Build-Log: jede Zeile ein Eintrag, Doppelklick springt zum Fehler */
    buildlog = MUI_NewObject(MUIC_NList,
                             MUIA_Frame, MUIV_Frame_InputList,
                             MUIA_NList_ConstructHook, MUIV_NList_ConstructHook_String,
                             MUIA_NList_DestructHook, MUIV_NList_DestructHook_String,
                             MUIA_Font, MUIV_Font_Fixed,
                             TAG_DONE);
    if (!buildlog)
        return NULL;
    log_lv = MUI_NewObject(MUIC_NListview, MUIA_NListview_NList, (ULONG)buildlog,
                           MUIA_CycleChain, 1, TAG_DONE);
    cmdinput = MUI_NewObject(MUIC_BetterString, MUIA_Frame, MUIV_Frame_String,
                             MUIA_CycleChain, 1, MUIA_String_MaxLen, 500, TAG_DONE);
    if (!cmdinput)
        cmdinput = StringObject, StringFrame, MUIA_CycleChain, 1, MUIA_String_MaxLen, 500, End;
    log_grp = VGroup,
        Child, log_lv,
        Child, HGroup,
            Child, cmdlabel = TextObject, MUIA_Text_PreParse, (ULONG)"\33r",
                MUIA_Text_Contents, (ULONG)"1>", MUIA_FixWidthTxt, (ULONG)"Verzeichnis>",
                MUIA_FramePhantomHoriz, TRUE, MUIA_Frame, MUIV_Frame_String, End,
            Child, HGroup, MUIA_HorizWeight, 400, Child, cmdinput, End,
            Child, bt_run = SimpleButton("_Ausfuehren"),
            Child, keycatch_new(),
        End,
    End;
    if (!log_lv || !log_grp)
        return NULL;
    edit_grp = editor_with_slider(&editor, FALSE);
    out_grp = editor_with_slider(&output, TRUE);
    if (!files_lv || !edit_grp || !out_grp)
        return NULL;

    /* Eingabezeile: BetterString.mcc, sonst eingebauter String */
    input = MUI_NewObject(MUIC_BetterString, MUIA_Frame, MUIV_Frame_String,
                          MUIA_CycleChain, 1, MUIA_String_MaxLen, 1000, TAG_DONE);
    if (!input)
        input = StringObject, StringFrame, MUIA_CycleChain, 1, MUIA_String_MaxLen, 1000, End;
    busy = MUI_NewObject(MUIC_Busy, MUIA_Busy_Speed, MUIV_Busy_Speed_Off,
                         MUIA_FixHeightTxt, (ULONG)"X", TAG_DONE);
    busy_is_mcc = busy != NULL;
    if (!busy)
        busy = TextObject, MUIA_Text_Contents, "", End;

    slashlist = MUI_NewObject(MUIC_NList,
                              MUIA_Frame, MUIV_Frame_InputList,
                              MUIA_NList_ConstructHook, MUIV_NList_ConstructHook_String,
                              MUIA_NList_DestructHook, MUIV_NList_DestructHook_String,
                              MUIA_NList_AutoVisible, TRUE,
                              TAG_DONE);
    if (!slashlist)
        return NULL;
    /* fest 5 Zeilen hoch; weitere Befehle per Scrollen oder Weitertippen */
    slashview = MUI_NewObject(MUIC_NListview, MUIA_NListview_NList, (ULONG)slashlist,
                              MUIA_FixHeightTxt, (ULONG)"\n\n\n\n",
                              MUIA_ShowMe, FALSE,
                              TAG_DONE);
    if (!slashview)
        return NULL;
    /* Pfeiltasten im Eingabefeld bewegen die Auswahl in der Liste */
    set(input, MUIA_String_AttachedList, slashlist);

    agent_grp = VGroup,
        Child, out_grp,
        Child, busy,
        Child, slashview,
        Child, HGroup,
            Child, HGroup, MUIA_HorizWeight, 400, Child, input, End,
            Child, bt_send = SimpleButton("_Senden"),
            Child, bt_stop = SimpleButton("S_top"),
        End,
    End;
    if (!agent_grp)
        return NULL;

    if (narrow) {
        static const char *titles[] = { "Agent", "Editor", "Dateien", "Build", NULL };
        main_grp = pages = RegisterGroup(titles),
            Child, agent_grp,
            Child, edit_grp,
            Child, files_lv,
            Child, log_grp,
        End;
    } else {
        main_grp = VGroup,
            Child, HGroup, MUIA_VertWeight, 75,
                Child, VGroup, MUIA_HorizWeight, 20, Child, files_lv, End,
                Child, BalanceObject, End,
                Child, VGroup, MUIA_HorizWeight, 45, Child, edit_grp, End,
                Child, BalanceObject, End,
                Child, VGroup, MUIA_HorizWeight, 35, Child, agent_grp, End,
            End,
            Child, BalanceObject, End,
            Child, VGroup, MUIA_VertWeight, 25, Child, log_grp, End,
        End;
    }
    if (!main_grp)
        return NULL;

    return WindowObject,
        MUIA_Window_Title, (ULONG)"AmiCodeIDE " VERSION_STRING,
        MUIA_Window_ID, MAKE_ID('A','M','C','D'),
        MUIA_HelpNode, (ULONG)"MAINWINDOW",
        MUIA_Window_Width, MUIV_Window_Width_Screen(80),
        MUIA_Window_Height, MUIV_Window_Height_Screen(75),
        MUIA_Window_Menustrip, MUI_MakeObject(MUIO_MenustripNM, (ULONG)menu, 0),
        WindowContents, VGroup,
            Child, build_toolbar(),
            Child, HGroup,
                Child, Label("Projekt:"),
                Child, projtext = TextObject, TextFrame, MUIA_Background, MUII_TextBack, End,
                Child, Label("Modell:"),
                Child, modeltext = TextObject, TextFrame, MUIA_Background, MUII_TextBack, End,
                Child, Label("Modus:"),
                Child, modecycle = CycleObject, MUIA_Cycle_Entries, (ULONG)mode_labels,
                                   MUIA_Cycle_Active, 1, MUIA_CycleChain, 1, End,
            End,
            Child, main_grp,
            Child, HGroup,
                Child, status = TextObject, TextFrame, MUIA_Background, MUII_TextBack,
                                MUIA_Text_Contents, "Bereit", End,
                Child, tokentext = TextObject, TextFrame, MUIA_Background, MUII_TextBack,
                                   MUIA_HorizWeight, 60, MUIA_Text_PreParse, (ULONG)"\33r",
                                   MUIA_Text_Contents, (ULONG)"Tokens: -", End,
            End,
        End,
    End;
}

/* ---------- Neues Projekt ---------- */

static void open_new_project(void)
{
    newproj_open(projects_dir());
}

static void create_project(void)
{
    const char *fp = newproj_first_prompt();

    if (agent_busy) {
        set_status("Der Agent arbeitet noch - Stop oder warten");
        return;
    }
    if (pending_first) {
        FreeVec(pending_first);
        pending_first = NULL;
    }
    if (fp && (pending_first = AllocVec(strlen(fp) + 1, MEMF_ANY)))
        strcpy(pending_first, fp);
    out_append("\n\33bNeues Projekt wird angelegt ...\33n\n");
    gui_agent_newproject(newproj_spec());
    set_busy(1);
}

/* Anbieter und Modell oben im Fenster anzeigen */
static void update_model_label(void)
{
    static char buf[200];
    ProviderSettings ps;

    provider_settings(&cfg, NULL, &ps);
    snprintf(buf, sizeof(buf), "%s: %s", ps.def->name, *ps.model ? ps.model : "(kein Modell)");
    set(modeltext, MUIA_Text_Contents, buf);
}

/* Einstellungen uebernehmen: Agent-Prozess stoppen (Config wird geaendert,
   malloc!), speichern, Agent neu starten und die Sitzung fortsetzen */
/* Konfiguration aus einem Fenster uebernehmen: Agent anhalten (config_set braucht
   malloc), Werte setzen, speichern, Agent neu starten und die Sitzung fortsetzen. */
static void apply_prefs(int save, struct MsgPort *events, void (*fill)(Config *))
{
    static const char *header = "# AmiCode Konfiguration (Einstellungsfenster). Enthaelt API-Keys - nicht weitergeben.";
    BPTR lock;

    if (agent_busy) {
        set_status("Erst den laufenden Auftrag beenden (Stop), dann speichern");
        return;
    }
    gui_agent_stop();
    fill(&cfg);
    if ((lock = CreateDir("ENV:AmiCode")))
        UnLock(lock);
    config_save(&cfg, "ENV:AmiCode/amicode.conf", header);
    if (save) {
        if ((lock = CreateDir("ENVARC:AmiCode")))
            UnLock(lock);
        config_save(&cfg, "ENVARC:AmiCode/amicode.conf", header);
    }
    prefs_close();
    update_model_label();
    if (model_list) {
        FreeVec(model_list);
        model_list = NULL;
    }
    out_append(save ? "\n\33bEinstellungen gespeichert.\33n\n" : "\n\33bEinstellungen verwendet (bis zum Neustart).\33n\n");
    start_agent(events);
    if (file_exists("amicode.session"))
        gui_agent_resume();     /* Unterhaltung mit dem neuen Modell weiterfuehren */
}

static int screen_width(void)
{
    struct Screen *scr = LockPubScreen(NULL);
    int w = 640;

    if (scr) {
        w = scr->Width;
        UnlockPubScreen(NULL, scr);
    }
    return w;
}

int main(int argc, char **argv)
{
    struct MsgPort *events;
    const char *start_dir = argc > 1 ? argv[1] : "";
    ULONG sigs = 0;
    int running = 1, narrow;
    LONG mode;

    (void)vers;
    if (!(MUIMasterBase = OpenLibrary(MUIMASTER_NAME, MUIMASTER_VMIN))) {
        printf("AmiCodeIDE braucht muimaster.library (MUI 3.8 oder neuer)\n");
        return RETURN_FAIL;
    }
    if (!config_load(&cfg, CONFIG_DEFAULT_PATH))
        config_load(&cfg, "ENVARC:AmiCode/amicode.conf");
    if (!(events = CreateMsgPort()))
        goto out_lib;

    /* [ui] layout = auto | narrow | wide */
    {
        const char *layout = config_get(&cfg, "ui.layout", "auto");
        if (stricmp(layout, "narrow") == 0)
            narrow = 1;
        else if (stricmp(layout, "wide") == 0)
            narrow = 0;
        else
            narrow = screen_width() < NARROW_WIDTH;
    }
    app = ApplicationObject,
        MUIA_Application_Title, (ULONG)"AmiCodeIDE",
        MUIA_Application_Version, (ULONG)vers,
        MUIA_Application_Copyright, (ULONG)"2026",
        MUIA_Application_Description, (ULONG)"Coding-Agent fuer AmigaOS",
        MUIA_Application_Base, (ULONG)"AMICODEIDE",
        MUIA_Application_HelpFile, (ULONG)GUIDE_FILE,
        SubWindow, win = build_window(narrow),
        SubWindow, prefswin = prefs_create(),
        SubWindow, newprojwin = newproj_create(),
        SubWindow, projselwin = projsel_create(),
        SubWindow, sessionswin = sessions_create(),
        SubWindow, skillswin = skillswin_create(),
    End;
    if (!app || !win || !prefswin || !newprojwin || !projselwin || !sessionswin || !skillswin) {
        MUI_Request(NULL, NULL, 0, "AmiCodeIDE", "OK",
                    "Oberflaeche konnte nicht aufgebaut werden.\n"
                    "Benoetigt: TextEditor.mcc, NList.mcc, NListview.mcc, NListtree.mcc\n"
                    "(Aminet: dev/mui/MCC_TextEditor, dev/mui/MCC_NList)");
        if (app)
            MUI_DisposeObject(app);
        if (keycatch_class)
            MUI_DeleteCustomClass(keycatch_class);
        goto out_port;
    }

    mode = mode_parse(config_get(&cfg, "agent.mode", "project"));
    set(modecycle, MUIA_Cycle_Active, mode < 0 ? MODE_PROJECT : mode);

    DoMethod(win, MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
             (ULONG)app, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
    DoMethod(bt_send, MUIM_Notify, MUIA_Pressed, FALSE,
             (ULONG)app, 2, MUIM_Application_ReturnID, ID_SEND);
    DoMethod(input, MUIM_Notify, MUIA_String_Acknowledge, MUIV_EveryTime,
             (ULONG)app, 2, MUIM_Application_ReturnID, ID_SEND);
    DoMethod(input, MUIM_Notify, MUIA_String_Contents, MUIV_EveryTime,
             (ULONG)app, 2, MUIM_Application_ReturnID, ID_INPUT_CHANGED);
    DoMethod(slashlist, MUIM_Notify, MUIA_NList_DoubleClick, MUIV_EveryTime,
             (ULONG)app, 2, MUIM_Application_ReturnID, ID_SLASH_PICK);
    DoMethod(bt_stop, MUIM_Notify, MUIA_Pressed, FALSE,
             (ULONG)app, 2, MUIM_Application_ReturnID, ID_STOP);
    DoMethod(files, MUIM_Notify, MUIA_NListtree_DoubleClick, MUIV_EveryTime,
             (ULONG)app, 2, MUIM_Application_ReturnID, ID_OPEN_FILE);
    DoMethod(openlist, MUIM_Notify, MUIA_NList_Active, MUIV_EveryTime,
             (ULONG)app, 2, MUIM_Application_ReturnID, ID_TAB_SELECT);
    DoMethod(editor, MUIM_Notify, MUIA_TextEditor_HasChanged, MUIV_EveryTime,
             (ULONG)app, 2, MUIM_Application_ReturnID, ID_EDIT_CHANGED);
    DoMethod(buildlog, MUIM_Notify, MUIA_NList_DoubleClick, MUIV_EveryTime,
             (ULONG)app, 2, MUIM_Application_ReturnID, ID_LOG_JUMP);
    notify_toolbar();
    prefs_notify(app);
    newproj_notify(app);
    projsel_notify(app);
    sessions_notify(app);
    skillswin_notify(app);
    update_model_label();
    DoMethod(bt_run, MUIM_Notify, MUIA_Pressed, FALSE,
             (ULONG)app, 2, MUIM_Application_ReturnID, ID_RUN_CMD);
    DoMethod(cmdinput, MUIM_Notify, MUIA_String_Acknowledge, MUIV_EveryTime,
             (ULONG)app, 2, MUIM_Application_ReturnID, ID_RUN_CMD);
    DoMethod(modecycle, MUIM_Notify, MUIA_Cycle_Active, MUIV_EveryTime,
             (ULONG)app, 2, MUIM_Application_ReturnID, ID_MODE);

    {
        static char last[512];
        int shown = 0;
        if (argc > 1 && set_project(start_dir)) {
            shown = 1;
        } else if (last_project(last, sizeof(last)) && set_project(last)) {
            shown = 1;
        } else {
            set_project(projects_dir());        /* vorlaeufig, bis ein Projekt gewaehlt ist */
        }
        startup_choose = !shown;
    }
    set(win, MUIA_Window_Open, TRUE);
    set(win, MUIA_Window_ActiveObject, input);

    out_append("\33bAmiCodeIDE " VERSION_STRING "\33n - Auftrag unten eingeben und mit Return senden.\n");
    gui_agent_version(VERSION_STRING);
    start_agent(events);
    if (startup_choose) {
        /* noch kein Projekt: Auswahl zeigen, oder gleich den Assistenten */
        if (!projsel_open(projects_dir(), NULL)) {
            projsel_close();
            open_new_project();
        }
    }

    while (running) {
        ULONG id = DoMethod(app, MUIM_Application_NewInput, (ULONG)&sigs);

        if (projsel_is_id(id)) {
            switch (projsel_handle(id)) {
            case PS_OPEN:
                switch_to_project(projsel_selected(), events);
                break;
            case PS_NEW:
                open_new_project();
                break;
            case PS_OTHER:
                choose_project(events);
                break;
            }
            continue;
        }
        if (sessions_is_id(id)) {
            int r = sessions_handle(id);
            if (r == SS_RESUME || r == SS_DELETE)
                session_action(r);
            continue;
        }
        if (newproj_is_id(id)) {
            if (newproj_handle(id) == NP_CREATE)
                create_project();
            continue;
        }
        if (skillswin_is_id(id)) {
            if (skillswin_handle(id) == SK_SAVE)
                apply_prefs(1, events, skillswin_apply);
            continue;
        }
        if (prefs_is_id(id)) {
            int r = prefs_handle(id);
            if (r == PREFS_SAVE || r == PREFS_USE)
                apply_prefs(r == PREFS_SAVE, events, prefs_apply);
            continue;
        }
        switch (id) {
        case ID_PREFS:
            prefs_open(&cfg);
            break;
        case ID_SKILLS:
            skillswin_open(&cfg);
            break;
        case ID_NEWPROJ:
            open_new_project();
            break;
        case ID_GUIDE:
            DoMethod(app, MUIM_Application_ShowHelp, (ULONG)win, (ULONG)GUIDE_FILE, (ULONG)"MAIN", 0);
            break;
        case MUIV_Application_ReturnID_Quit:
        case ID_QUIT:
            if (confirm_discard("Beim Beenden gehen sie verloren."))
                running = 0;
            break;
        case ID_TAB_SELECT:
            tab_select();
            break;
        case ID_CLOSE_FILE:
            close_tab();
            break;
        case ID_EDIT_CHANGED:
            tabs_refresh_list();
            break;
        case ID_SEND:
            /* Liste offen: erst den gewaehlten Befehl uebernehmen */
            if (!slash_shown || slash_pick())
                send_prompt();
            break;
        case ID_SLASH_PICK:
            if (slash_pick())
                send_prompt();
            break;
        case ID_INPUT_CHANGED:
            slash_update();
            break;
        case ID_STOP:
            gui_agent_break();
            set_status("Stop gesendet");
            break;
        case ID_OPEN_FILE:
            file_doubleclick();
            break;
        case ID_SAVE_FILE:
            save_file();
            break;
        case ID_RELOAD:
            load_file_list();
            break;
        case ID_LOG_JUMP:
            log_jump();
            break;
        case ID_RUN_CMD:
            run_manual_command();
            break;
        case ID_UNDO:
        case ID_DIFF:
            if (agent_busy) {
                set_status("Der Agent arbeitet noch - Stop oder warten");
                break;
            }
            if (id == ID_UNDO)
                gui_agent_undo();
            else
                gui_agent_diff();
            set_busy(1);
            break;
        case ID_BUILD:
        case ID_RUN:
            run_tool_command(id);
            break;
        case ID_CLEAR_LOG:
            DoMethod(buildlog, MUIM_NList_Clear);
            break;
        case ID_PROJECT:
            if (agent_busy)
                set_status("Erst den laufenden Auftrag beenden");
            else
                projsel_open(projects_dir(), project);
            break;
        case ID_RESET:
            if (!agent_busy) {
                DoMethod(output, MUIM_TextEditor_ClearText);
                gui_agent_reset();
            }
            break;
        case ID_RESUME:
            if (agent_busy) {
                set_status("Erst den laufenden Auftrag beenden");
            } else {
                DoMethod(output, MUIM_TextEditor_ClearText);
                gui_agent_prompt("/resume");
                set_busy(1);
            }
            break;
        case ID_SESSIONS:
            if (agent_busy) {
                set_status("Erst den laufenden Auftrag beenden");
            } else {
                sessions_open();
                gui_agent_sessions(NULL);
                set_busy(1);
            }
            break;
        case ID_MODE:
            get(modecycle, MUIA_Cycle_Active, &mode);
            gui_agent_mode((int)mode);
            break;
        case ID_ABOUT:
            MUI_Request(app, win, 0, "Ueber AmiCodeIDE", "OK",
                        "\33c\33bAmiCodeIDE " VERSION_STRING "\33n\n"
                        "Coding-Agent fuer AmigaOS 3.2\n\nModell: %s",
                        (ULONG)config_get(&cfg, "provider.model", "gpt-5.4-mini"));
            break;
        }
        if (running && sigs) {
            ULONG evsig = 1UL << events->mp_SigBit;
            ULONG got = Wait(sigs | evsig | gui_agent_replysig() | SIGBREAKF_CTRL_C);
            if (got & SIGBREAKF_CTRL_C)
                running = 0;
            if (got & evsig)
                handle_events(events);
            if (got & gui_agent_replysig())
                gui_agent_collect();
        }
        /* nach /new: ins neue Projekt wechseln (Agent-Prozess neu starten) */
        if (running && switch_pending) {
            static char dir[512];
            switch_pending = 0;
            strcpy(dir, pending_project);
            pending_project[0] = 0;
            if (confirm_discard("Beim Wechsel ins neue Projekt gehen sie verloren.")) {
                gui_agent_stop();
                if (set_project(dir)) {
                    out_append("\n\33bProjekt gewechselt:\33n ");
                    out_append(project);
                    out_append("\n");
                }
                start_agent(events);
                if (pending_first) {
                    /* Assistent: direkt mit der Aufgabe loslegen */
                    out_append("\33b> ");
                    out_append(pending_first);
                    out_append("\33n\n");
                    gui_agent_prompt(pending_first);
                    set_busy(1);
                    set_status("Agent arbeitet am neuen Projekt ...");
                }
            }
            if (pending_first) {
                FreeVec(pending_first);
                pending_first = NULL;
            }
        }
    }

    set(win, MUIA_Window_Open, FALSE);
    gui_agent_stop();
    MUI_DisposeObject(app);
    if (keycatch_class)
        MUI_DeleteCustomClass(keycatch_class);
out_port:
    DeleteMsgPort(events);
out_lib:
    if (orig_saved)
        CurrentDir(orig_dir);
    if (project_lock)
        UnLock(project_lock);
    config_free(&projcfg);
    config_free(&cfg);
    CloseLibrary(MUIMasterBase);
    return RETURN_OK;
}
