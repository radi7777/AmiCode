# AmiCodeIDE

## Projektziel

Entwicklung einer nativen, MUI-basierten IDE für AmigaOS 3.x auf einem beschleunigten Amiga mit Fast RAM und RTG. Die Anwendung soll sich konzeptionell an Visual Studio Code orientieren und eine integrierte, OpenCode-ähnliche Coding-Agent-Umgebung enthalten.

Der Agent soll direkt auf dem Amiga laufen und dort:

- Dateien lesen, erstellen, ändern und speichern.
- Verzeichnisse durchsuchen.
- Shell- und AmigaDOS-Befehle ausführen.
- C-Compiler und Build-Systeme starten.
- Compiler- und Laufzeitfehler analysieren.
- Änderungen selbstständig oder nach Bestätigung anwenden.
- Projekte bauen und testen.
- Mit Claude und später optional OpenAI/Codex-kompatiblen Modellen kommunizieren.

Es soll kein MCP-Mac als notwendige Zwischenstation erforderlich sein. Der Amiga ist die eigentliche Entwicklungsumgebung, Build-Maschine und Ausführungsplattform.

---

## Zielplattform

### Primär

- AmigaOS 3.2 (Kickstart/dos/graphics 47) als Mindestversion.
- 68030 oder höher als Baseline (`-m68030`), 68040/68060 profitieren automatisch.
- Turbokarte.
- Fast RAM.
- Zuerst RTG (Picasso96 und/oder CyberGraphX).
- Muss aber auch auf nativen Modes laufen: PAL/NTSC, auch interlaced (z. B. 640x256, 640x512, 640x200/400), mit wenigen Farben.
- MUI 5 (muimaster.library 19+).

### Konsequenzen für native Modes

- Nur MUI-Mittel, keine RTG-Only-APIs, keine festen Pixelgrößen oder Farben.
- Layout muss bei 640x256 bedienbar sein: dann PageGroup/Register statt Drei-Spalten-Layout.
- Keine Truecolor-Grafiken voraussetzen; Icons/Bilder optional, Text-Fallbacks.
- Neuzeichnen sparsam halten (Chip-RAM-Bandbreite, langsamer Blitter bei Interlace).
- Test regelmäßig auch auf einem PAL-Interlace-Screen.

### Referenz-Testsystem (geprüft am 2026-09-23)

| Komponente | Stand |
|---|---|
| Kickstart / dos / graphics | 47.13 / 47 / 47 (OS 3.2) |
| CPU / FPU | 68040 / ja |
| Fast RAM frei | ca. 342 MB |
| MUI | muimaster.library 19.35 (MUI 5) |
| MCCs | NList, NListview, NListtree, TextEditor, BetterString, TheBar, Term u. a. vorhanden |
| TCP/IP | bsdsocket.library 4.364 (Roadshow) |
| TLS | AmiSSL 5.27, amissl_v362.library (OpenSSL 3.6.2) |
| Zugriff vom Mac | amiagent 0.12.0 (amiga-MCP) im Heimnetz |

## Entwicklungsweg (Entscheidung 2026-09-23)

- Cross-Compile auf dem Mac mit `m68k-amigaos-gcc` 16.2 (bebbo), liegt unter `~/opt/m68k-amigaos-gcc-16.2`; NDK 3.2, MUI-SDK und AmiSSL-SDK sind enthalten.
- Deploy: `make push` (bzw. `python3 push.py <dateien>`) schiebt Dateien direkt über das amimcp-Protokoll nach `Stuff:AmiCode` (übernommen aus AmiSubsonic). Test und Screenshots über den amiga-MCP.
- Erster Provider zum Testen: OpenAI-kompatibel (OpenAI-API-Key vorhanden, Modell `gpt-5.4-mini`). Gemini über denselben Provider möglich, Anthropic folgt mit API-Key.
- Config: `ENVARC:AmiCode/amicode.conf` (wird nach `ENV:` kopiert), API-Key nur dort.
- Die Mac-Kette dient nur der Entwicklung. Im Zielbetrieb läuft AmiCode ohne Mac.
- HTTP zunächst selbst über bsdsocket + AmiSSL, kein libcurl, weil die Toolchain kein libcurl enthält (bei Bedarf später prüfen).

### Spätere Optionen

- AmigaOS 4.
- MorphOS.
- AROS.
- Gemeinsamer, möglichst portabler Agent- und Provider-Kern.

Die Benutzeroberfläche soll normale MUI- und Intuition-Funktionen verwenden. Direkter Picasso96- oder CyberGraphX-Zugriff soll nur nötig sein, wenn MUI für spezielle Darstellungen nicht ausreicht.

---

## Produktidee

Name: **AmiCodeIDE** (MUI-Programm `AmiCodeIDE`, Kommandozeile `amicode`). Icon: `art/icon.jpeg` → `build/AmiCodeIDE.info` über `tools/appicon.py` (klassisch 4 Farben + GlowIcon, 98x43).

Die Anwendung kombiniert:

```text
Visual-Studio-Code-ähnliche MUI-IDE
+
integriertes Amiga-Terminal
+
OpenCode-ähnlicher Coding-Agent
+
direkter Zugriff auf Amiga-Dateien und AmigaDOS
+
Claude- beziehungsweise OpenAI-Anbindung
```

Beispielstart:

```text
AMICODE DH0:Projects/MyGame
```

Beispielauftrag:

```text
Analysiere das Projekt, behebe alle Compilerfehler und baue es anschließend.
```

Der Agent soll daraufhin Dateien untersuchen, Compiler starten, Fehler analysieren, Änderungen durchführen und den Build erneut ausführen.

---

## Zielarchitektur

```text
┌────────────────────────────────────────────────────────────┐
│ AmiCode IDE                                              │
│                                                            │
│ MUI-Oberfläche                                             │
│ Projektbaum | Editor | Agent | Terminal | Build            │
│                                                            │
│ Agent-Kern                                                 │
│ Session | Kontext | Tool-Aufrufe | Freigaben              │
│                                                            │
│ Provider-Adapter                                           │
│ Claude | OpenAI API | OpenRouter | lokaler Endpoint       │
│                                                            │
│ AmigaOS-Tool-Schicht                                       │
│ Dateien | Shell | Compiler | Prozesse | Screen             │
└──────────────────────────────┬─────────────────────────────┘
                               │ HTTPS
                               ▼
                       Claude / OpenAI
```

Der direkte Zielbetrieb lautet:

```text
AmiCode auf dem Amiga
        │
        ├── liest Dateien lokal
        ├── ändert Dateien lokal
        ├── startet Compiler lokal
        ├── führt Shell-Befehle lokal aus
        └── kommuniziert direkt per HTTPS mit dem LLM
```

`amimcp` ist für den bisherigen Mac-Workflow relevant und kann als Referenz für Tool-Namen, Dateizugriff, Shell-Ausführung und Protokollierung dienen. Für den endgültigen Direktbetrieb ist jedoch kein MCP-Mac erforderlich.

---

## MUI-Oberfläche

### Hauptansicht

```text
┌─────────────────────────────────────────────────────────────────────┐
│ AmiCode  Project  Agent  Build  Terminal  Settings                │
├───────────────┬───────────────────────────────────────┬─────────────┤
│ PROJECT       │ EDITOR                                │ AGENT       │
│               │                                       │             │
│ ▾ MyGame      │ main.c                                │ Claude      │
│   ▾ src       │  1 #include <exec/types.h>           │             │
│     main.c    │  2 #include <proto/dos.h>            │ Du: ...     │
│     game.c    │  3                                       │             │
│   ▾ include   │  4 int main(void)                    │ Agent: ...  │
│   Makefile    │                                       │             │
├───────────────┴───────────────────────────────────────┴─────────────┤
│ TERMINAL / BUILD                                                     │
│ 1> make                                                              │
│ game.c:142: error: ...                                               │
├─────────────────────────────────────────────────────────────────────┤
│ [Ask] [Save] [Build] [Run] [Stop]   Model: Claude   Project: MyGame  │
└─────────────────────────────────────────────────────────────────────┘
```

### MUI-Komponenten

- `WindowObject` für das Hauptfenster.
- `VGroup`, `HGroup` und `PageGroup` für Layouts.
- `NListtree.mcc` für den Projektbaum.
- `NList.mcc` für Agent-Aktionen und Listen.
- `TextEditor.mcc` für Quellcode, Agent-Ausgabe und Build-Log.
- `StringObject` für Such- und Prompt-Eingaben.
- `SimpleButton()` für Aktionen.
- `GaugeObject` oder Textobjekte für Statusanzeigen.
- MUI-Menüs für Projekt-, Agent-, Build- und Einstellungsfunktionen.

### Kleine und große RTG-Auflösungen

Bei 640x480 oder ähnlichen Auflösungen:

```text
[Files] [Editor] [Agent] [Terminal] [Build]
```

Bei größeren RTG-Auflösungen:

```text
Projektbaum | Editor | Agent
             Terminal/Build unten
```

Die Bereiche sollen über MUI-Balances veränderbar sein.

---

## Funktionsbereiche

### 1. Projektverwaltung

- Projektverzeichnis öffnen.
- Projektbaum aufbauen.
- Verzeichnisse expandieren und zuklappen.
- Dateien filtern und suchen.
- Aktuelle Datei anzeigen.
- Projektregeln aus `AMICODE.md` laden.
- Projektstatus und Build-Konfiguration anzeigen.

### 2. Editor

Erste Version mit `TextEditor.mcc`:

- Datei laden.
- Datei speichern.
- Read-only-Modus für Logs.
- Suchen und Ersetzen.
- Undo.
- Cursor- und Zeilenposition.
- Datei neu laden.
- Backup vor dem Speichern.

Spätere Funktionen:

- Zeilennummern.
- C-Syntax-Highlighting.
- Klammermatching.
- Fehlerpositionen anklickbar.
- Sprung zu Datei und Zeile aus Compilerfehlern.
- Mehrere geöffnete Dateien.
- Tabs.
- Patch- und Diff-Ansicht.

### 3. Integriertes Terminal

Manueller Modus:

```text
1> list src
2> make
3> execute build
4> MyGame
```

Agent-Modus:

```text
run_command("make", "DH0:Projects/MyGame")
```

Das manuelle Terminal und Agent-Kommandos sollen dieselbe Shell- und Prozessschicht nutzen.

### 4. Agent-Panel

Das Agent-Panel zeigt:

- Benutzeraufträge.
- Modellantworten.
- Laufende Tool-Aufrufe.
- Dateizugriffe.
- Shell-Kommandos.
- Compiler-Ausgaben.
- Fehler.
- Freigabeanfragen.
- Agent-Status.

Beispiel:

```text
[12:31:04] Agent gestartet
[12:31:05] Lese DH0:Projects/MyGame/Makefile
[12:31:06] Lese src/game.c
[12:31:09] Sende Anfrage an Claude
[12:31:14] Tool: run_command("make")
[12:31:15] Compilerfehler empfangen
[12:31:18] Ändere src/game.c
[12:31:20] Build erfolgreich
```

### 5. Build und Diagnose

Register:

```text
[Build Log] [Shell] [Diagnostics]
```

Unterstützen:

- `make`.
- Compiler direkt starten.
- Build-Skripte ausführen.
- Standardausgabe erfassen.
- Fehlerausgabe erfassen.
- Exit-Code auswerten.
- Compilerfehler parsen.
- Fehler anklickbar machen.
- Agent automatisch mit Fehlern weiterarbeiten lassen.

Compilerprofile:

- SAS/C.
- VBCC.
- GCC.
- DICE.
- Aztec C.
- AmigaOS-4-GCC, falls portiert.

---

## Agent-Modell

Der Agent ist ein lokaler Agent-Loop auf dem Amiga:

```text
1. Auftrag einlesen.
2. Projektregeln laden.
3. Relevanten Kontext ermitteln.
4. Anfrage an das LLM senden.
5. Antwort oder Tool-Aufruf empfangen.
6. Tool lokal ausführen.
7. Ergebnis an das LLM zurücksenden.
8. Änderungen anwenden oder bestätigen lassen.
9. Build oder Test ausführen.
10. Ergebnis erneut analysieren.
11. Vorgang beenden oder nächsten Schritt durchführen.
```

Beispiel:

```text
Benutzer:
  Behebe den Fehler und baue das Projekt.

LLM:
  read_file("src/main.c")

Amiga:
  liefert Dateiinhalt

LLM:
  run_command("make")

Amiga:
  liefert Compilerfehler

LLM:
  write_file("src/main.c", "...")

Amiga:
  schreibt Datei

LLM:
  run_command("make")

Amiga:
  Build erfolgreich
```

### Schrittbegrenzung

Der Agent braucht eine maximale Anzahl von Schritten:

```ini
max_steps=30
```

Bei Erreichen des Limits muss er anhalten und den Benutzer informieren.

---

## Lokale Tools

Die Tools werden direkt als C-Funktionen implementiert. MCP ist für den Direktbetrieb nicht notwendig.

```c
int tool_read_file(const char *path, char **result);
int tool_write_file(const char *path, const char *content);
int tool_edit_file(const char *path, const char *old_string, const char *new_string);
int tool_list_directory(const char *path, char **result);
int tool_search_files(const char *path, const char *pattern, char **result);
int tool_create_directory(const char *path);
int tool_rename_file(const char *source, const char *destination);
int tool_delete_file(const char *path);
int tool_run_command(const char *command, const char *directory, char **result);
int tool_get_system_info(char **result);
int tool_read_screen(char **result);
```

### Dateifunktionen

Für den Dateizugriff native AmigaDOS-Funktionen verwenden:

- `Open()`.
- `Read()`.
- `Write()`.
- `Close()`.
- `Lock()`.
- `Examine()`.
- `ExNext()`.
- `DeleteFile()`.
- `Rename()`.
- `CreateDir()`.
- `CurrentDir()`.

Native Dateifunktionen sind Shell-Kommandos vorzuziehen, weil Amiga-Pfade, Assigns und Sonderzeichen sauberer behandelt werden.

### Shell-Funktionen

Für externe Programme und Build-Skripte verwenden:

- `System()`.
- `SystemTagList()`.
- `Execute()`.
- `RunCommand()`.
- `CreateNewProc()`.

Shell-Ausgabe und Fehlerausgabe müssen in den Agent zurückgeführt werden:

```text
stdout     → Agent
stderr     → Agent
returncode → Agent
```

---

## Tool-Format

Intern kann ein einfaches JSON-Format verwendet werden:

```json
{
  "tool": "run_command",
  "arguments": {
    "command": "make",
    "directory": "DH0:Projects/MyGame"
  }
}
```

Antwort:

```json
{
  "tool": "run_command",
  "result": {
    "return_code": 10,
    "stdout": "",
    "stderr": "game.c:142: error: ..."
  }
}
```

Für die Provider kann das jeweilige Function-Calling-Format genutzt werden. Der interne Agent sollte jedoch eine providerunabhängige Struktur verwenden.

---

## Prozess- und MUI-Architektur

Der API-Aufruf darf den MUI-Event-Loop nicht blockieren.

```text
MUI-Hauptprozess
       │ MessagePort
       ▼
Agent-Prozess
       │
       ├── JSON erstellen
       ├── HTTPS-Request senden
       ├── Streaming empfangen
       ├── Dateien lesen/schreiben
       ├── Shell/Compiler ausführen
       └── Nachrichten an MUI senden
```

Nachrichtenstruktur:

```c
struct AgentMessage {
    struct Message msg;
    ULONG command;
    STRPTR payload;
    LONG result;
};
```

Mögliche Kommandos:

```c
#define AGENT_SEND_PROMPT   1
#define AGENT_READ_FILE     2
#define AGENT_WRITE_FILE    3
#define AGENT_RUN_COMMAND   4
#define AGENT_BUILD         5
#define AGENT_STOP          6
```

Mögliche Events:

```c
#define EVENT_STATUS         1
#define EVENT_TEXT           2
#define EVENT_TOOL_START     3
#define EVENT_TOOL_RESULT    4
#define EVENT_ERROR          5
#define EVENT_FINISHED       6
```

---

## Claude-Unterstützung

Primärer Provider ist Claude.

Mögliche Betriebsarten:

### Anthropic API

```text
AmiCode → HTTPS → Anthropic API
```

Konfiguration:

```ini
provider=anthropic
model=claude-sonnet-5
endpoint=https://api.anthropic.com/v1/messages
auth=api-key
```

### Web- beziehungsweise Account-Login

Falls der offizielle Anbieter einen für Drittanwendungen geeigneten Device-Code- oder OAuth-Flow bereitstellt:

```text
1. AmiCode zeigt Login-URL oder Device-Code.
2. Benutzer öffnet die URL auf einem Browser.
3. Benutzer meldet sich an.
4. AmiCode erhält beziehungsweise speichert eine Session.
```

Keine internen Tokens aus einer anderen offiziellen Anwendung kopieren. Nur dokumentierte und zulässige Authentifizierungswege verwenden.

### Claude-Code-Kompatibilität

Ziel ist zunächst eine funktional ähnliche Umgebung:

- Terminalorientiertes Arbeiten.
- Agent-Aufträge.
- Dateien ändern.
- Shell-Befehle ausführen.
- Tool-Loop.
- Projektkontext.
- Freigabemodi.
- Verlauf.

Die originale Claude-Code-CLI muss nicht vollständig auf den 68k-Amiga portiert werden.

---

## OpenAI- und Codex-Unterstützung

Nach Claude kann ein OpenAI-Provider ergänzt werden:

```text
provider=openai
model=...
endpoint=https://api.openai.com/v1/...
```

Mögliche Modi:

- Direkte OpenAI-API mit API-Key.
- Offizieller Login- beziehungsweise Device-Code-Flow, sofern für die Integration geeignet.
- OpenAI-kompatibler lokaler Endpoint.
- OpenRouter oder anderer Gateway.

Die Provider-Schnittstelle:

```c
struct LLMProvider {
    const char *name;
    int (*login)(struct ProviderConfig *config);
    int (*send)(struct AgentRequest *request,
                struct AgentResponse *response);
    int (*parse_response)(struct AgentResponse *response,
                          struct AgentAction *action);
    int (*supports_tools)(void);
    int (*supports_streaming)(void);
};
```

Provider:

```text
anthropic
openai
openrouter
local-openai-compatible
custom-proxy
```

---

## HTTPS und JSON

Für AmigaOS 3.x:

- AmiSSL als TLS-Schicht prüfen.
- libcurl mit AmiSSL prüfen.
- JSON-Parser mit geringer Speicherlast verwenden.
- Große Antworten nicht unkontrolliert im Chip RAM halten.
- Fast RAM für Antwortpuffer und Projektindizes nutzen.
- Streaming-Unterstützung optional ergänzen.
- Zertifikats- und Uhrzeitprobleme berücksichtigen.

Zunächst kann eine nicht-streamende Anfrageimplementierung genügen. Danach Streaming ergänzen.

---

## Rechte- und Sicherheitsmodi

### SAFE

- Dateien lesen erlaubt.
- Dateien schreiben nur nach Bestätigung.
- Shell-Befehle nur nach Bestätigung.
- Löschen immer nach Bestätigung.

### PROJECT

- Vollzugriff innerhalb des Projektverzeichnisses.
- Build- und Testbefehle automatisch erlaubt.
- Schreibzugriff automatisch erlaubt, optional mit Backups.
- Zugriff außerhalb des Projektverzeichnisses nur nach Rückfrage.

### FULL

- Vollzugriff auf das gesamte Amiga-Dateisystem.
- Beliebige Shell-Befehle erlaubt.
- Löschen erlaubt.
- Notabschaltung bleibt aktiv.

Notabschaltung:

```text
CTRL-C  laufenden Shell-Befehl abbrechen
CTRL-K  Agentenauftrag stoppen
/stop   Agent stoppen
/pause  keine weiteren Tool-Aufrufe zulassen
```

Standardmäßig mit `PROJECT` oder `SAFE` starten.

---

## Projektdatei `AMICODE.md`

Beispiel:

```markdown
# MyGame

## Zielsystem

- AmigaOS 3.2
- 68060
- RTG über Picasso96
- Fast RAM vorhanden

## Build

- Compiler: VBCC
- Build-Befehl: make
- Ausgabe: MyGame

## Regeln

- AmigaOS-3-kompatiblen C-Code schreiben.
- Keine AmigaOS-4-Funktionen verwenden.
- Nur vorhandene Includes verwenden.
- Nach jeder Codeänderung `make` ausführen.
- Compilerfehler vollständig analysieren.
- Änderungen möglichst klein halten.
- Vor großen Änderungen Backups erstellen.
```

Diese Datei soll automatisch beim Start eines Projekts eingelesen und dem Agent-Kontext hinzugefügt werden.

---

## Konfiguration

Beispiel `PROGDIR:Config/AmiCode.conf`:

```ini
[provider]
name=anthropic
model=claude-sonnet-5
endpoint=https://api.anthropic.com/v1/messages
auth=api-key

[project]
root=DH0:Projects/MyGame
build=make
compiler=vbcc

[agent]
mode=project
auto_build=yes
auto_apply=no
max_steps=30
streaming=no

[ui]
show_tool_calls=yes
show_build_log=yes
editor_font=topaz
```

API-Keys nicht in Quelltext oder Projektdateien einbauen. Eine separate Konfigurationsdatei mit geeigneten Dateirechten verwenden.

---

## Compilerprofile

Beispiel:

```ini
[compiler.vbcc]
command=make
working_directory=project

[compiler.sasc]
command=smake
working_directory=project

[compiler.gcc]
command=make
working_directory=project
```

Die IDE soll Buildprofile auswählen können:

```text
[VBCC] [SAS/C] [GCC] [Custom]
```

---

## Quellcodeaufteilung

```text
AmiCode/
├── src/
│   ├── main.c
│   ├── mui_ui.c
│   ├── mui_layout.c
│   ├── agent.c
│   ├── agent_loop.c
│   ├── llm.c
│   ├── provider_anthropic.c
│   ├── provider_openai.c
│   ├── json.c
│   ├── tools.c
│   ├── files.c
│   ├── shell.c
│   ├── process.c
│   ├── project.c
│   ├── build.c
│   ├── diff.c
│   ├── settings.c
│   └── logging.c
├── include/
│   ├── amigacode.h
│   ├── agent.h
│   ├── llm.h
│   ├── provider.h
│   ├── tools.h
│   ├── project.h
│   └── build.h
├── classes/
├── catalog/
├── icons/
├── docs/
├── Makefile
├── AMICODE.md
└── README.md
```

---

## Anregungen aus Proteus (MorphOS), gelesen 2026-09-23

Proteus (Aminet `dev/misc/proteus.ppc-morphos`, Freeware ohne Quellcode, Brian Feeny) ist ein Claude-Code-artiger Agent für MorphOS. Übernommen werden nur Ideen aus seiner Dokumentation, kein Code.

Bestätigt (machen wir bereits so): eigene C-Tools statt Shell-first, `edit_file` als exakte Ersetzung mit Eindeutigkeitsprüfung, Pfadauflösung über `Lock`/`NameFromLock`, `NIL:` als Eingabe, Zeitlimit mit ausdrücklichem Hinweis an das Modell, Sitzungen fortsetzen, Kontext kürzen.

Übernehmen (Stand: 1-9 umgesetzt in amicode 0.5 / AmiCodeIDE 0.7, getestet 2026-09-23; offen: 10 Streaming):

1. Tabu-Liste, in keinem Modus automatisch erlaubt: Schreiben in `SYS:`, `C:`, `S:`, `LIBS:`, `DEVS:`, `L:`; `Format`, `Install`, `Relabel`, `Assign`, `Mount`, `AddBuffers`; `Delete ... ALL`; alles außerhalb des Projekts.
2. Enthaltensein im Projekt über `ParentDir()`/`SameLock()` prüfen statt über Pfad-Strings (Assigns, Multi-Assigns, Groß-/Kleinschreibung).
3. Veraltungsschutz: `edit_file`/`write_file` nur auf Dateien, die in dieser Sitzung gelesen wurden und seitdem unverändert sind (Größe + Datum).
4. Freigabe-Dialog zeigt aufgelösten Pfad und Diff; Enter/Esc = ablehnen; zusätzlich „Auftrag abbrechen“.
5. Sicherungen im Projekt (`.amicode/backups/<sitzung>/`) mit Journal; `/undo` und `/diff`.
6. Audit-Log `.amicode/audit.log`, geschrieben *vor* jeder Aktion.
7. `read_file` mit Zeilennummern und `offset`/`limit`; eigenes `find_files` (Dateinamen).
8. Befehlsausgaben: gleiche Zeilen zusammenfassen; Temp-Dateinamen durchprobieren, weil abgebrochene Befehle Dateien bis zum Neustart sperren können.
9. Weitere Befehle: `/compact` (Zusammenfassung), `/prune`, `/context`, `/tokens`, `/model`, `/status`, `!befehl`, `!!befehl` - in CLI und GUI-Eingabezeile gleich (`agent_command()` im Kern).
10. Referenz für Streaming und Tool-Calls auf dem Amiga: AmigaGPT (MIT).

## Toolchains / Sprachen (umgesetzt 2026-09-23, amicode 0.7 / AmiCodeIDE 0.12)

`/toolchains scan` sucht installierte Werkzeuge (auf allen Volumes unter `Developer/`, `Dev/`, `Programming/` oder direkt) und schreibt `ENVARC:AmiCode/toolchains.conf`. `/new <werkzeug> <verzeichnis>` legt ein Projekt an: Hallo-Welt, `build`-Skript (setzt Assigns/Pfad bei jedem Lauf selbst), `AMICODE.md` mit Sprachregeln für den Agenten, `.amicode/settings` (build, run, main). Die GUI wechselt nach `/new` ins neue Projekt. Der Systemprompt nennt die installierten Werkzeuge.

Alle Profile auf dem Referenz-Amiga geprüft (anlegen, bauen, starten):

| Profil | Sprache / Werkzeug | Build | Fehlerformat |
|---|---|---|---|
| vbcc | C, VBCC (+ NDK 3.2 `Include_H`) | `vc +aos68k -O1 -o name name.c` | `error N in line L of "datei"` |
| sasc | C, SAS/C 6.59 | `sc name.c LINK PNAME=name` | `datei L Error N: ...` |
| gcc | C, gcc 2.95.3 (ADE) | `gcc -O2 -noixemul -o name name.c` | `datei:L: ...` (stderr) |
| amiblitz | BASIC, AmiBlitz3 (CLI-Modus) | `Amiblitz3 -s name.ab3 -e name` | `Compiler Error #1 in <datei>:` + `Line L: ...`; **Returncode immer 0** |
| amigae | E, EC 3.3a | `EC name` | `ERROR: ...` + `LINE L: ...` (Hauptdatei) |
| purebasic | BASIC, PureBasic 4.00 | `PBCompiler name.pb TO name` (Stack 40000) | `Error: Line L - ...` (Hauptdatei) |
| vasm | 68k-Assembler, vasm (aus VBCC) | `vasmm68k_mot -Fhunkexe -nosym -o name name.s` | wie VBCC |
| lua | Lua 5.0 | – (`lua name.lua`) | |
| python | Python 2.0 | – (`Execute run`, setzt `Python:`) | |
| arexx | ARexx | – (`SYS:Rexxc/rx name.rexx`) | |

Nicht möglich: AMOS Pro (binärer Quelltext, Compiler nur als AMOS-Programm) - bewusst weggelassen.

Build-Log: ANSI-Farbcodes werden entfernt; Doppelklick versteht auch die zweizeiligen AmiBlitz-Meldungen und die dateilosen E-/PureBasic-Meldungen (Sprung in `main=`).

## Anbieter, Einstellungen und Internet (2026-09-23, amicode 0.8 / AmiCodeIDE 0.13)

- Konfiguration: `[provider] active=` plus je Anbieter `[openai]`, `[openrouter]`, `[ollama]`, `[custom]` mit `base` (…/v1), `api_key`, `model`. Alte `[provider] endpoint/api_key/model` gelten weiter als OpenAI.
- Einstellungsfenster in der GUI (Amiga-E): Anbieter, Basis-URL, API-Key (verdeckt), Modell mit Aufklappliste; „Modelle laden“ holt `GET base/models` (OpenAI: nur Chat-Modelle, OpenRouter: nur Tool-fähige, ohne `:batch`). Speichern schreibt `ENV:`+`ENVARC:`, startet den Agent-Prozess neu und setzt die Sitzung fort.
- `/model` holt die Liste und bietet sie in der `/`-Auswahl an (Filter durch Weitertippen); gilt für die Sitzung.
- HTTP ohne TLS (Ollama im LAN), GET, Weiterleitungen. Getestet: OpenAI, OpenRouter (u. a. Claude), Ollama auf dem PC (qwen3-coder, qwen3.8:27b).
- Internet für alle Modelle: Tools `web_search` (DuckDuckGo lite, kein Key) und `fetch_url` (HTML → Text, Links als `<url>`, 16000 Zeichen pro Aufruf, `offset`). Im Modus safe mit Rückfrage, abschaltbar mit `[agent] web=no`, Audit-Log. Systemprompt: Webinhalte sind fremd, keine Anweisungen daraus befolgen, keine privaten Daten in URLs.
- Sicherheit: `Execute` ohne Rückfrage nur für Textdateien (ein lokales Modell hatte `Execute hello` benutzt, um eine Ablehnung zu umgehen).

## Assistent „Neues Projekt“ (2026-09-23, AmiCodeIDE 0.14)

Menü Projekt → Neues Projekt… (Amiga-N), Knopf „Neu“ in der Werkzeugleiste oder `/new` ohne Argumente. Abgefragt werden: Name, Speicherort (ASL), Sprache/Werkzeug (gefundene Toolchains + „Eigenes Projekt (ohne Vorgaben)“), Programmart (Shell / Fenster / MUI / Sonstiges), CPU und System mindestens, Beschreibung der Aufgabe, „Direkt loslegen“. Programmart, Zielsystem und Aufgabe landen in `AMICODE.md`; mit „Direkt loslegen“ wechselt die IDE ins neue Projekt und schickt die Beschreibung als ersten Auftrag. Beim eigenen Projekt wählt der Agent Sprache/Werkzeug selbst und trägt `build=`/`run=`/`main=` in `.amicode/settings` ein (die GUI liest die Datei nach jedem Auftrag neu). Standard-Speicherort: `[ui] projects=` oder neben dem aktuellen Projekt.

## Entwicklungsphasen (neu, Entscheidung 2026-09-23: erst CLI, dann MUI)

Der Agent-Kern entsteht zuerst als Shell-Programm `amicode`. Die MUI-IDE setzt später auf denselben Kern auf. So wird das riskante Stück (TLS, JSON, Speicher, Tool-Loop) zuerst geprüft, und schon die erste Version ist nutzbar.

Der Kern wird von Anfang an UI-unabhängig gebaut: Ausgaben laufen über Event-Callbacks, nicht über direktes `Printf()`.

### Phase 1: Netzwerk und Claude-API (CLI) - erledigt 2026-09-23

- HTTPS-Client über bsdsocket.library + AmiSSL 5 (Zertifikatsprüfung aktiv).
- Systemzeit prüfen und bei falscher Uhr klar warnen (TLS).
- Kleiner JSON-Builder/Parser, Puffer im Fast RAM.
- Anthropic Messages API, nicht streamend.
- Config `ENVARC:AmiCode/amicode.conf` (API-Key, Modell), Key nie loggen.
- `amicode "Frage"` → Antwort im Shell-Fenster.

### Phase 2: Agent-Loop (CLI) - erledigt 2026-09-23

Stand: `amicode [MODE safe|project|full] [STEPS n] <Auftrag>` im Projektverzeichnis. Test `tests/hello` (Syntaxfehler + unbekannter Bezeichner) wird selbststaendig gebaut, mit zwei minimalen `edit_file`-Aufrufen repariert und neu gebaut (8 Schritte, ca. 10.000 Tokens mit gpt-5.4-mini). Sicherungen landen in `T:AmiCode-Backup`. Offen: CTRL-C bricht einen laufenden Befehl noch nicht ab (SystemTags laeuft synchron), Prompt-Caching/Kontextkuerzung fehlen noch.


- Native Anthropic-Tool-Use.
- Tools: `read_file`, `list_directory`, `search_files`, `edit_file` (old_string → new_string), `write_file` (nur neue Dateien / Komplettersatz), `run_command`.
- Ausgabe von `run_command` in Temp-Datei/`PIPE:` umleiten (stdout+stderr gemeinsam) + Return-Code, lange Ausgaben kürzen.
- Freigaben: SAFE / PROJECT / FULL; Programme ausführen (nicht `make`) ist ein eigenes Freigabe-Level.
- `max_steps`, CTRL-C stoppt den Agent.
- Prompt Caching, `AMICODE.md` als Projektkontext.

### Phase 3: Robustheit - weitgehend erledigt 2026-09-23

Erledigt (AmiCode 0.3, getestet auf dem Amiga):

- Befehle laufen in einem Hilfsprozess (`src/shell.c`). CTRL-C wird als Break an den Befehl weitergereicht, Zeitlimit `agent.cmd_timeout` (Standard 600 s). Der Agent bekommt den Abbruch als Hinweis im Tool-Ergebnis.
- Sitzung wird laufend nach `amicode.session` im Projektverzeichnis geschrieben (eine Nachricht pro Zeile). `amicode RESUME` setzt fort; unvollständige Tool-Aufrufe nach einem Absturz werden mit einem Platzhalter-Ergebnis abgeschlossen.
- Kontext-Kürzung: Ab `agent.max_context` Bytes (Standard 150000) werden alte, große Tool-Ergebnisse durch einen Hinweis ersetzt; die letzten 8 Nachrichten bleiben unangetastet.
- Interaktiver Modus: `amicode` ohne Auftrag öffnet eine Eingabezeile (`/exit`, `/reset`, `/help`), Folgeaufträge laufen in derselben Sitzung.
- Sicherungen vor der ersten Änderung pro Datei in `T:AmiCode-Backup`.

Offen:

- Streaming (SSE) mit zusammengesetzten Tool-Call-Deltas.
- Compilerfehler-Parser (wird erst für die MUI-Oberfläche gebraucht; im CLI liest das Modell die Fehler selbst).
- Tipp: Sicherungen liegen in T: (RAM) und überleben keinen Neustart; ggf. `agent.backup_dir` auf eine Festplatte legen.

Konfigurationsschlüssel im Abschnitt `[agent]`: `mode`, `max_steps`, `cmd_timeout`, `stack`, `allow`, `backup_dir`, `session` (`none` = nicht speichern), `max_context`.

### Phase 4: MUI-Grundgerüst - erledigt 2026-09-23

AmiCodeIDE 0.4 (`build/AmiCodeIDE`, Start: `Run >NIL: AmiCodeIDE [Projektverzeichnis]`), getestet auf RTG 1366x854:

- Agent-Kern unverändert wiederverwendet; Ausgaben laufen über `ui.h`-Hooks (CLI: Shell, GUI: Messages).
- Agent in eigenem Prozess (`src/gui_agent.c`), Befehle/Ereignisse über MessagePorts, Freigaben als MUI-Requester (Erlauben/Immer/Ablehnen). bsdsocket/AmiSSL werden im Agent-Prozess geöffnet (pro Task).
- Regel: libnix-`malloc` ist nicht threadsicher - der GUI-Prozess nutzt nach dem Start des Agenten nur `AllocVec`.
- Layout: Dateien | Editor | Agent mit Balances; unter 800 Pixel Breite (PAL/NTSC) Register. Erzwingen mit `[ui] layout=narrow|wide|auto`.
- MCCs: TextEditor (Editor, Agent-Ausgabe), NList/NListview (Dateien), BetterString (Eingabe, Fallback String), Busy (Fallback Text).
- MUI 3.8-Header aus `vendor/mui` (Toolchain-Header verlangen muimaster V20) und `src/muistubs.c` als eigene Übersetzungseinheit.
- Editor: Doppelklick öffnet Datei, Menü „Datei speichern“; nach jedem Auftrag werden Liste und (unveränderte) Datei neu geladen.

### Phase 5: Editor, Projektbaum, Terminal - erledigt 2026-09-23

AmiCodeIDE 0.10, getestet:

- Projektbaum mit `NListtree.mcc` (rekursiv bis Tiefe 6, max. 2000 Einträge, Ordner fett und oben, `.info`, `.amicode` und `amicode.session` ausgeblendet). Doppelklick öffnet die Datei.
- Offene Dateien: Liste unter dem Projektbaum, `*` = ungespeichert, Klick wechselt, ungespeicherter Text bleibt beim Wechsel erhalten; „Datei schließen“ (Amiga-W) und Beenden/Projektwechsel fragen bei ungespeicherten Änderungen nach. (Register-Reiter lassen sich unter MUI 3.8 nicht zur Laufzeit ergänzen.)
- Syntax-Hervorhebung für C (`.c .h .cpp ...`) und Assembler (`.s .asm .i`): Schlüsselwörter/Amiga-Typen fett, Kommentare kursiv, Präprozessor fett - nur Stile, keine Farben (native Modes). Gespeichert wird mit `ExportHook_NoStyle` (Datei nachweislich byte-identisch), `ConvertTabs` aus (Makefiles), `SoftWrap`.
- Werkzeugleiste mit `TheBar.mcc` (Textmodus; Fallback: normale Knöpfe): Bauen, Starten, Speichern, Diff, Undo, Stop, Neue Sitzung. Bauen erkennt `Execute build` / `make` / `smake` oder `[ui] build=`; Starten nutzt `[ui] run=` aus `.amicode/settings` im Projekt.
- Build-Log (`NList.mcc`) mit allen Befehlsausgaben, Fehler fett, Doppelklick springt zu Datei/Zeile (VBCC, GCC, SAS/C); manuelle Befehle über die `1>`-Zeile.
- Diff-Vorschau im Freigabe-Requester, Streaming im Agent-Fenster.

Offen / verschoben:

- ~~`Term.mcc`~~ verworfen (2026-09-23): das installierte Term.mcc 27.3 gehört zu NapsaTerm (Genesis) und ist undokumentiert. Stattdessen kann die `1>`-Zeile jetzt mehr (AmiCodeIDE 0.21): `CD` bleibt für die folgenden Befehle erhalten (Verzeichnis links angezeigt, Agent bleibt im Projektverzeichnis), Cursor hoch/runter blättert durch die letzten 30 Befehle (unsichtbares Rectangle-Subclass-Objekt mit eigenem Event-Handler, Priorität 50).
- Echte PAL-Interlace-Auflösung noch nicht getestet (nur erzwungenes `[ui] layout=narrow`).

### Phase 6: Provider und Komfort - erledigt 2026-09-23

- OpenAI-/OpenRouter-/Ollama-/eigener Provider, Modellwahl (Einstellungsfenster, /model).
- Syntax-Highlighting, offene Dateien.
- Sitzungen verwalten (amicode 0.12 / AmiCodeIDE 0.21, getestet): Neue Sitzung, /reset und der erste Auftrag nach dem Start verschieben eine Sitzung mit mindestens einem Auftrag nach `.amicode/sessions/<JJJJ-MM-TT_hh-mm-ss>.session` (Zeitpunkt der letzten Änderung). `/sessions` listet (Titel = erster Auftrag), `/resume [nr|name]` lädt und zeigt die letzten 5 Runden, `/sessions delete nr` löscht. GUI: Fenster „Sitzungen...“ (Amiga-J, `gui_sessions.c`) mit Fortsetzen/Löschen; Liste kommt als `UI_SESSIONS` vom Agenten.

---

## Erste Zielversion: AmiCode 0.1 (CLI)

```text
- läuft unter AmigaOS 3.2, 68030+
- HTTPS mit AmiSSL, Claude-API-Anfrage und Antwort in der Shell
- Agent-Loop mit read/list/search/edit/write/run
- SAFE- und PROJECT-Modus, max_steps, CTRL-C
- kann ein kleines C-Projekt bauen und Compilerfehler selbst beheben
```

## Zweite Zielversion: AmiCode 0.2 (MUI)

```text
- MUI-Oberfläche auf demselben Agent-Kern
- läuft auf RTG und nativen PAL/NTSC-(Interlace-)Modes
- Projektbaum, Editor, Agent-Panel, Build-Log
- Session-Resume, Backups, Diff-Vorschau
```

---

## Mögliche technische Lösungen

### Lösung A: Direkte Claude-API

```text
MUI-IDE → Agent-Prozess → libcurl/AmiSSL → Anthropic API
```

Vorteile:

- Kein Mac erforderlich.
- Einfaches Architekturmodell.
- Agent arbeitet direkt auf dem Amiga.
- Provider kann später erweitert werden.

Nachteile:

- API-Key muss auf dem Amiga geschützt werden.
- JSON, TLS und API-Kompatibilität müssen portiert werden.
- Cloud-Zugriff hängt von Netzwerk und Zertifikaten ab.

### Lösung B: Direkter OpenAI-API-Provider

```text
MUI-IDE → Agent-Prozess → OpenAI API
```

Vorteile:

- Alternative zu Claude.
- OpenAI-kompatible Schnittstellen sind verbreitet.

Nachteile:

- Provider-spezifisches Request- und Response-Format.
- Login und Abrechnung müssen sauber getrennt werden.

### Lösung C: Lokaler API-Proxy als optionaler Fallback

```text
AmiCode → einfacher HTTP-Proxy → Claude/OpenAI
```

Der Proxy ist nicht zwingend Bestandteil des Zielsystems, kann aber als Option dienen, wenn:

- TLS auf einer bestimmten Amiga-Konfiguration Probleme macht.
- API-Keys nicht auf dem Amiga liegen sollen.
- mehrere Provider zentral verwaltet werden sollen.
- Antworten komprimiert oder normalisiert werden sollen.

### Lösung D: Übernahme von Ideen aus `amimcp`

`amimcp` kann als Referenz dienen für:

- Tool-Namen.
- Dateioperationen.
- Shell-Kommandos.
- Screen-Capture.
- Tastatur- und Maussteuerung.
- AmigaOS-Daemon-Struktur.
- Protokollierung.

Die endgültige IDE soll diese Funktionen jedoch direkt lokal anbieten und keinen MCP-Mac voraussetzen.

---

## Prompts für Claude Code

Die folgenden Prompts können direkt an Claude Code gegeben werden.

### Projekt analysieren

```text
Lies Projekt.md vollständig und analysiere den geplanten Aufbau von AmiCode IDE. Erstelle zuerst eine technische Roadmap für Phase 1. Berücksichtige AmigaOS 3.x, MUI, RTG, Turbokarte, Fast RAM und Portierbarkeit. Verändere noch keine Dateien.
```

### MUI-Grundgerüst

```text
Implementiere Phase 1 von Projekt.md: ein minimales MUI-Grundgerüst für AmiCode IDE. Erzeuge ein Hauptfenster mit Menü, linker Projektbaum-Spalte, mittlerem Editor-Bereich, rechtem Agent-Bereich und unterem Build-/Terminal-Bereich. Verwende die im Projekt vorhandenen MUI-Header und halte den Code für AmigaOS 3.x kompatibel. Prüfe zuerst vorhandene Includes, Libraries und Compilerprofile.
```

### Projektbaum

```text
Implementiere den Projektbaum gemäß Projekt.md. Verwende NListtree.mcc, sofern die Klasse im Entwicklungssystem vorhanden ist. Falls sie nicht verfügbar ist, implementiere zunächst eine Fallback-Liste mit NList.mcc oder einer Standard-MUI-Liste. Der Baum soll AmigaDOS-Pfade und Assigns wie DH0:, PROGDIR: und RAM: korrekt behandeln.
```

### Editor

```text
Implementiere die Datei-Öffnen- und Speichern-Funktion für den MUI-Editor. Verwende native AmigaDOS-Dateifunktionen. Lege vor dem Überschreiben eine Backup-Datei an. Achte auf Zeilenenden, Zeichensatz und Dateigrößen. Integriere das Öffnen einer Datei aus dem Projektbaum.
```

### Terminal

```text
Implementiere das integrierte Terminal. Shell-Befehle sollen in einem separaten AmigaOS-Prozess ausgeführt werden. Fange stdout, stderr und den Return-Code auf und zeige die Ergebnisse im Build-/Terminal-Panel an. Die MUI-Oberfläche darf während eines laufenden Befehls nicht blockieren. Implementiere eine Stop-Funktion.
```

### Claude-API

```text
Implementiere einen separaten Anthropic-Provider für AmiCode. Verwende die vorhandene AmigaOS-Netzwerkumgebung und prüfe, ob libcurl mit AmiSSL verfügbar ist. Erzeuge zunächst eine einfache nicht-streamende Anfrage. API-Key und Endpoint müssen aus einer Konfigurationsdatei kommen. Keine Zugangsdaten in den Quellcode schreiben. Implementiere saubere Fehlertexte für DNS, TLS, HTTP und JSON-Fehler.
```

### Tool-Loop

```text
Implementiere den Agent-Loop gemäß Projekt.md. Unterstütze zuerst nur read_file, list_directory, write_file und run_command. Verwende ein internes providerunabhängiges Tool-Format. Führe Tool-Aufrufe in einem separaten Agent-Prozess aus und sende Events über MessagePorts an die MUI-Oberfläche. Begrenze die maximale Schrittzahl und implementiere eine sofortige Stop-Funktion.
```

### Compilerfehler

```text
Implementiere einen Compilerdiagnose-Parser für die Ausgabe von make, VBCC, SAS/C und GCC. Erkenne Dateiname, Zeilennummer, Fehlertyp und Meldung. Zeige die Diagnose im Build-Panel und ermögliche einen Klick, der die betreffende Datei im Editor öffnet und zur Zeile springt.
```

### Diff und Freigabe

```text
Implementiere eine Diff-Ansicht für Agentenänderungen. Änderungen dürfen im SAFE-Modus nicht sofort geschrieben werden. Zeige alte und neue Version oder einen Unified Diff an. Biete Apply, Reject und Save as Patch an. Im PROJECT-Modus darf das Verhalten konfigurierbar automatisch sein.
```

### OpenAI-Provider

```text
Implementiere nach dem Anthropic-Provider einen OpenAI-Provider mit derselben internen Provider-Schnittstelle. Trenne Request-Erzeugung, Authentifizierung und Response-Parsing vom Agent-Kern. Bestehende Tools und die MUI-Oberfläche dürfen nicht providerabhängig werden.
```

### Code-Review

```text
Prüfe die aktuelle Implementierung gegen Projekt.md. Suche nach Problemen mit AmigaOS-3.x-Kompatibilität, MUI-Event-Loop, MessagePorts, Speicherverwaltung, Fast RAM, RTG, Shell-Prozessen, Dateipfaden, API-Key-Sicherheit und Fehlerbehandlung. Erstelle zuerst eine priorisierte Liste und ändere noch keinen Code.
```

### Build-Loop

```text
Implementiere einen Agentenworkflow: Projekt analysieren, relevante Dateien lesen, Build-Befehl ausführen, Compilerfehler analysieren, minimale Änderungen vorschlagen oder anwenden und den Build erneut ausführen. Verwende AMICODE.md als Projektregeln. Stoppe bei wiederholten Fehlern oder beim Erreichen von max_steps.
```

---

## Regeln für Claude Code

Claude Code soll bei diesem Projekt:

1. Vor Änderungen zuerst die vorhandene Projektstruktur und den Compiler prüfen.
2. Projekt.md und `AMICODE.md` lesen.
3. Keine modernen POSIX- oder AmigaOS-4-Funktionen verwenden, wenn das Ziel AmigaOS 3.x ist.
4. MUI-Objekte sauber freigeben.
5. Keine blockierenden Netzwerk- oder Compileraufrufe im MUI-Hauptprozess ausführen.
6. AmigaDOS-Pfade und Assigns berücksichtigen.
7. Änderungen klein und kompilierbar halten.
8. Nach jeder sinnvollen Änderung den Build ausführen.
9. Compilerfehler vollständig anzeigen und analysieren.
10. Vor destruktiven Operationen Backups oder Freigaben verwenden.
11. Keine API-Keys in Quellcode, Logs oder Git-Commits schreiben.
12. Providerlogik vom Agent-Kern trennen.
13. MUI- und AmigaOS-Versionen dokumentieren.
14. Bei fehlenden MUI-Klassen einen klaren Fallback vorschlagen.
15. Nicht eigenmächtig die gesamte Architektur ändern.

---

## Akzeptanzkriterien

### MUI

- Die Anwendung startet auf dem Ziel-Amiga.
- Das Hauptfenster öffnet auf dem RTG-Screen.
- Layout passt sich an mindestens zwei Auflösungen an.
- Kein Blockieren des MUI-Event-Loops.

### Dateien

- Datei kann geöffnet werden.
- Datei kann gespeichert werden.
- Backup wird korrekt angelegt.
- AmigaDOS-Pfade funktionieren.

### Terminal

- Shell-Befehl kann ausgeführt werden.
- stdout und stderr werden getrennt oder erkennbar angezeigt.
- Return-Code wird angezeigt.
- Ein laufender Prozess kann beendet werden.

### Build

- `make` oder ein konfigurierter Compiler kann gestartet werden.
- Compilerfehler werden angezeigt.
- Datei und Zeile können erkannt werden.

### Agent

- Claude-Anfrage kann gesendet werden.
- Antwort wird angezeigt.
- Tool-Aufruf kann erkannt werden.
- Tool wird lokal ausgeführt.
- Ergebnis wird zurück an Claude gesendet.
- Agent kann mehrere Schritte durchführen.
- Agent kann gestoppt werden.

### Sicherheit

- SAFE-, PROJECT- und FULL-Modus sind unterscheidbar.
- Destruktive Aktionen können blockiert oder bestätigt werden.
- API-Zugangsdaten stehen nicht im Quelltext.

---

## Nicht-Ziele der ersten Version

Nicht sofort implementieren:

- Vollständiger Ersatz für Visual Studio Code.
- Vollständiger LSP-Server.
- Vollständige Git-GUI.
- Lokales Ausführen eines großen LLM.
- Vollständige originale Claude-Code-CLI-Portierung.
- Vollständige originale Codex-CLI-Portierung.
- Eigene RTG-Grafikengine.
- Vollständige Debugger-Integration.
- Automatisches Löschen ohne Sicherheitsmodus.

Diese Punkte können später ergänzt werden.

---

## Erste konkrete Aufgabe für Claude Code

```text
Lies Projekt.md vollständig. Erstelle jetzt Phase 1 als kompilierbares AmigaOS-3.x-Projekt für eine MUI-Anwendung namens AmiCode. Prüfe zuerst, welche MUI-Header, Libraries und Compiler auf dem System vorhanden sind. Baue ein minimales Hauptfenster mit Menü, Projektbaum-Platzhalter, Editor-Platzhalter, Agent-Panel und Build-/Terminal-Panel. Verwende eine getrennte AppData-Struktur, saubere Fehlerbehandlung und einen nicht blockierenden Architekturentwurf. Führe nach der Implementierung den konfigurierten Build aus und berichte über fehlende Abhängigkeiten.
```

### Tokensparen (amicode 0.13 / AmiCodeIDE 0.22, 2026-09-23)

Anlass: eine MUI-Uhr mit Claude Opus über OpenRouter hat rund 10 Dollar gekostet. Auswertung der Sitzung: 62 Anfragen, ca. 1,4 Mio. Eingabe-Tokens, 77 % davon Tool-Ergebnisse; etwa 50 Schritte gingen für die Suche in VBCC-Headern drauf (MUI-Linken). Umgesetzt und getestet:

- A: `compact()` kürzt erst ab `agent.max_context` und dann in einem Rutsch auf die Hälfte. Vorher wurde bei fast jedem Schritt etwas gekürzt, was den Prompt-Cache des Anbieters jedes Mal ab dieser Stelle ungültig machte.
- B: kleinere Tool-Ergebnisse: read_file 250 Zeilen/8 KB, search_files 40 Treffer/4 KB/5 pro Datei, find_files 100/4 KB, list_directory 200, run_command 2+6 KB, fetch_url 8000 Zeichen.
- C: VBCC-Profil linkt mit `-lamiga`; bei Programmart MUI schreibt der Assistent ein erprobtes MUI-Rezept ins AMICODE.md. Systemprompt: sparsam lesen, nach drei erfolglosen Suchen bauen oder fragen.
- D: OpenRouter-Kosten (`usage.cost`, Anfrage mit `usage.include`) werden summiert: Zeile nach jedem Auftrag, `/tokens`, Statuszeile der IDE. `[agent] max_cost` (Standard 2.00 $) hält einen Auftrag an.
- E: `[<anbieter>] effort=low|medium|high|default` (OpenRouter: `reasoning.effort`, sonst `reasoning_effort`); OpenRouter-Standard low; Einstellungsfenster "Denkaufwand".
- Test mit Haiku 4.5: 3 Schritte, 11377 Tokens ein, davon 4263 aus dem Cache, $0.010.

### Skills (amicode 0.14 / AmiCodeIDE 0.23, 2026-09-23)

Wissensdateien fuer Modelle ohne Amiga-Kenntnisse (qwen3.8 ueber Ollama). `skills/*.md` im Repo, auf dem Amiga `PROGDIR:Skills` (make push), pro Projekt `.amicode/skills` (Vorrang). Format: `# Titel`, `Description: ...`, Inhalt (englisch, je ca. 1k Tokens). Mitgeliefert: amigados, c-amiga, mui (Befehlsschablonen auf dem Amiga geprueft).

- Modus je Skill `[skills] name=always|auto|off` (Standard auto). always: Inhalt im Systemprompt; auto: Titel/Beschreibung im Prompt, Inhalt ueber das neue Werkzeug `read_skill`.
- `/skills` listet; IDE-Fenster "Skills..." (Amiga-K, `gui_skills.c`), Uebernehmen startet den Agenten neu und setzt die Sitzung fort.
- Test qwen3.8 (Frage nach List/Search-Befehlen): ohne Skills erfundene Befehle (FindFile, grep -r), mit always richtig, mit auto ruft qwen selbst read_skill auf und antwortet richtig.
- Nebenbei: Pfad-Argument `""` (Modelle schicken das fuer "Projekt") gilt jetzt als leer; Kosten stehen in der Statuszeile vorn.

Test qwen3.8 mit Skills (always), Auftrag "analoge Uhr, Zeit Berlin" (CLI, Modus project, 40 Schritte):
- Lauf 1 (Skills ohne Codebeispiel): viel Header-Suche, falscher Code (lokale Library-Basen, SetRast statt SetAPen, OpenDevice falsch), Schrittlimit ohne erfolgreichen Build.
- c-amiga-Skill um ein getestetes Komplettbeispiel (Fenster + timer.device + Schliessknopf) und Hinweise (-lmieee, M_PI, SetRast) erweitert.
- Lauf 2 (neues Projekt AnalogUhr2): 28 Schritte, baut fehlerfrei, laeuft. Zeiger waren senkrecht gespiegelt (y-Achse); nach Fehlerbeschreibung in 5 Schritten korrigiert. Kosten: keine (lokal), ca. 460k Tokens.
- Erkenntnis: fuer kleine Modelle wirken vollstaendige, erprobte Codebeispiele in Skills deutlich besser als Regeln in Prosa.

### Ollama Cloud (amicode 0.15 / AmiCodeIDE 0.24, 2026-09-23)

Neuer Anbieter `ollamacloud` (Einstellungsfenster "Ollama Cloud"): Basis `https://ollama.com/v1` (OpenAI-kompatibel), Key von ollama.com/settings/keys, Standardmodell gpt-oss:120b. Modellliste ohne Key getestet (20 Modelle, u. a. qwen3.5:397b, kimi-k2.7-code, glm-5.3, deepseek-v4-pro). Chat und Tool-Aufrufe noch nicht getestet (kein Key). Alternative ohne Aenderung: Cloud-Modelle `name:cloud` ueber den lokalen Ollama-Server nach `ollama signin`.

Test nemotron-3-ultra (Ollama Cloud, frei), gleicher Uhr-Auftrag, Skills auto (Projekt AnalogUhr3): 23 Schritte, zweimal HTTP 503 "overloaded" (daraufhin automatische Wiederholung bei 429/502/503/504 eingebaut, 10/20/40 s, amicode 0.16 / IDE 0.25). Hat keinen Skill geladen, waehlte eine MUI-Custom-Class, Build ok, aber Dispatcher ohne Registerparameter, MUI_NewObject mit Klassenzeiger und keine MUIM_Application_NewInput-Schleife -> nicht gestartet (Absturzgefahr). Neue C-Projekte weisen im AMICODE.md jetzt an, vorher `c-amiga` (und bei MUI `mui`) mit read_skill zu laden.

Modellvergleich Uhr-Auftrag (2026-09-23), Ergebnis:
- moonshotai/kimi-k2.7-code ueber OpenRouter (Skills auto): laedt selbst c-amiga, 8 Schritte, 43 s, $0.021, laeuft auf Anhieb mit richtiger Zeit (Digitalzeit zusaetzlich im Fenstertitel). Klarer Sieger.
- qwen3.8:27b lokal (Skills always): laeuft nach einem Hinweis (gespiegelte Zeiger), ca. 25 min, kostenlos.
- qwen3-coder:30b, gemma4, nemotron-3-ultra: verworfen.
- Kimi in der IDE: AnalogUhr7 zur MUI-Anwendung (eigene Zeichenklasse, Knopf Ende) umgebaut, 10 Schritte, $0.041, laeuft auf Anhieb. Standardmodell jetzt moonshotai/kimi-k2.7-code ueber OpenRouter.

OpenAI-Test (2026-09-23): gpt-5.6-sol und gpt-5.6-luna lehnen Werkzeuge auf /chat/completions ab, wenn reasoning_effort nicht explizit "none" ist (400, obwohl das Feld fehlt - Server nimmt sonst Reasoning-Modus an). provider_chat wiederholt das jetzt automatisch einmal mit reasoning_effort=none (amicode 0.19 / IDE 0.28). Danach beide Modelle erfolgreich: Uhr + MUI-Umbau (AnalogUhr8/9), beide haben Skills selbst geladen, beide Builds ohne Fehler, beide Programme laufen und beenden sich sauber. OpenAI meldet keine Kosten pro Anfrage; geschaetzt (Listenpreise) ca. $0.05 (sol) bzw. $0.01 (luna) fuer beide Aufgaben zusammen.
