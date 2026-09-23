#ifndef AMICODE_UI_H
#define AMICODE_UI_H

/* Schnittstelle des Agent-Kerns zur Oberflaeche. Der Kern gibt nie selbst
   etwas aus, sondern ruft diese Hooks auf - die CLI schreibt in die Shell,
   die MUI-Oberflaeche schickt Nachrichten an ihren Hauptprozess. */

enum {
    UI_INFO,        /* Statuszeilen (Kopf, Projektregeln geladen, ...) */
    UI_STEP,        /* "[n] denke nach ..." */
    UI_TEXT,        /* Text des Modells */
    UI_TOOL,        /* "> read_file foo.c" */
    UI_DETAIL,      /* Vorschau einer Aenderung, Hinweise zu Befehlen */
    UI_ERROR,
    UI_BUSY,        /* text "1" = Anfrage laeuft, "0" = fertig */
    UI_DONE,        /* Auftrag beendet; text = Zusammenfassung Schritte/Tokens */
    UI_OUTPUT,      /* Ausgabe eines Shell-Befehls (mehrzeilig) fuer das Build-Log */
    UI_STREAM,      /* Stueck einer gestreamten Antwort, ohne Zeilenumbruch anhaengen */
    UI_PROJECT,     /* neues Projekt angelegt; text = Verzeichnis (GUI wechselt dorthin) */
    UI_MODELS,      /* Modellliste: erste Zeile Ziel ("slash" oder "prefs"), dann ein Modell pro Zeile */
    UI_CHOICES,     /* Werkzeuge fuer "Neues Projekt": "id\tname" pro Zeile */
    UI_TOKENS,      /* Tokenstand: "kontext limit ein aus cache" (Zahlen, Leerzeichen) */
    UI_PROMPT,      /* frueherer Auftrag des Benutzers (Verlauf nach /resume) */
    UI_CWD,         /* Verzeichnis der 1>-Zeile nach CD (voller Pfad) */
    UI_SESSIONS     /* Sitzungsliste: "name\tdatum\tgroesse\ttitel" pro Zeile, name "*" = aktuelle */
};

/* Antworten auf ui_ask */
enum { ASK_NO, ASK_YES, ASK_ALWAYS, ASK_ABORT };

typedef struct {
    void (*print)(int kind, const char *text);  /* ISO-8859-1, ohne abschliessendes \n */
    int (*ask)(const char *question);           /* liefert ASK_* */
} UiHooks;

extern const UiHooks *ui;

void ui_printf(int kind, const char *fmt, ...);
int ui_ask(const char *question);

#endif
