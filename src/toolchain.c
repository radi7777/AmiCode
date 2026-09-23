/* Entwicklungswerkzeuge erkennen und Projekte dafuer anlegen.

   Jedes Profil wurde auf dem Referenz-Amiga (OS 3.2, 2026-09-23) mit einem
   Hallo-Welt-Programm geprueft. Die Build-Skripte setzen ihre Assigns bei jedem
   Lauf neu, weil sich Werkzeuge gegenseitig stoeren (SAS/C und gcc wollen
   beide INCLUDE:). */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <exec/types.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <proto/exec.h>
#include <proto/dos.h>

#include "toolchain.h"

typedef struct {
    const char *id;
    const char *name;       /* Anzeige: Sprache (Werkzeug) */
    const char *dirnames;   /* moegliche Installationsverzeichnisse, '|' getrennt */
    const char *marker;     /* Datei relativ zum Installationsverzeichnis */
    const char *ext;        /* Endung der Hauptdatei */
    const char *setup;      /* Zeilen fuer das build-Skript; {dir}, {ndk} */
    const char *build;      /* NULL = Skriptsprache ohne Build */
    const char *run;        /* Startbefehl; {name} */
    const char *hello;      /* Hallo-Welt; {name} */
    const char *rules;      /* Hinweise fuer AMICODE.md */
} Profile;

static const Profile profiles[] = {
{
    "vbcc", "C (VBCC)", "VBCC|vbcc", "bin/vc", "c",
    "Assign >NIL: vbcc: \"{dir}\"\n"
    "Assign >NIL: vincludeos3: vbcc:targets/m68k-amigaos/include\n"
    "{ndkadd}"
    "Assign >NIL: vlibos3: vbcc:targets/m68k-amigaos/lib\n"
    "SetEnv VBCC vbcc:\n"
    "Path >NIL: vbcc:bin ADD\n",
    "vc +aos68k -O1 -lamiga -o {name} {name}.c",
    "{name}",
    "#include <stdio.h>\n\nint main(void)\n{\n    printf(\"Hallo von {name}!\\n\");\n    return 0;\n}\n",
    "- Sprache: C (C89/C99), Compiler VBCC fuer AmigaOS 3.x (`vc +aos68k`).\n"
    "- Weitere .c-Dateien in der vc-Zeile des build-Skripts ergaenzen.\n"
    "- Amiga-Header (proto/exec.h usw.) kommen aus dem NDK 3.2.\n"
    "- Gelinkt wird mit `-lamiga` (amiga.lib: DoMethod, CreateExtIO, Varargs-Stubs wie MUI_NewObject).\n"
    "  Eine mui.lib oder `-lmui` gibt es nicht.\n"
    "- Fehlerformat: `error 82 in line 18 of \"datei.c\": ...`\n"
},
{
    "sasc", "C (SAS/C 6)", "SASC|SAS-C|sasc", "c/sc", "c",
    "Assign >NIL: SASC: \"{dir}\"\n"
    "Assign >NIL: SC: SASC:\n"
    "Assign >NIL: lib: SC:lib\n"
    "Assign >NIL: include: SC:include\n"
    "Path >NIL: SC:c ADD\n"
    "Stack 200000\n",
    "sc {name}.c LINK PNAME={name}",
    "{name}",
    "#include <stdio.h>\n\nint main(void)\n{\n    printf(\"Hallo von {name}!\\n\");\n    return 0;\n}\n",
    "- Sprache: C (C89), Compiler SAS/C 6 (`sc`, `slink`).\n"
    "- Weitere Dateien in der sc-Zeile des build-Skripts ergaenzen.\n"
    "- Fehlerformat: `datei.c 5 Error 9: ...`\n"
},
{
    "gcc", "C (gcc 2.95, ADE)", "ADE|ade", "bin/gcc", "c",
    "Assign >NIL: ADE: \"{dir}\"\n"
    "Assign >NIL: GG: ADE:\n"
    "Assign >NIL: USR: ADE:\n"
    "Assign >NIL: INCLUDE: ADE:include\n"
    "Path >NIL: ADE:bin ADE:lib/gcc-lib/m68k-amigaos/2.95.3 ADD\n"
    "Stack 200000\n",
    "gcc -O2 -noixemul -o {name} {name}.c",
    "{name}",
    "#include <stdio.h>\n\nint main(void)\n{\n    printf(\"Hallo von {name}!\\n\");\n    return 0;\n}\n",
    "- Sprache: C, Compiler gcc 2.95.3 aus ADE, immer mit `-noixemul` (libnix, kein ixemul.library).\n"
    "- gcc 2.95 kennt kein C99 vollstaendig; Variablen am Blockanfang deklarieren.\n"
    "- Fehlerformat: `datei.c:5: ...`\n"
},
{
    "amiblitz", "BASIC (AmiBlitz3)", "AmiBlitz3|Amiblitz3|AmiBlitz", "Amiblitz3", "ab3",
    "",
    "\"{dir}/Amiblitz3\" -s {name}.ab3 -e {name}",
    "{name}",
    "; {name} - AmiBlitz3\nNPrint \"Hallo von {name}!\"\nEnd\n",
    "- Sprache: AmiBlitz3 (Blitz Basic), Quelltext ist Text (`.ab3`).\n"
    "- Die Einstellungen stehen als `; XTRA`-Kommentarblock am Dateianfang - nicht entfernen.\n"
    "- ACHTUNG: Der Compiler liefert auch bei Fehlern Returncode 0. Erfolg nur bei `0 errors` in der Ausgabe.\n"
    "- Fehlerformat: `Compiler Error #1 in <datei>:` und in der naechsten Zeile `Line 4: ...`\n"
},
{
    "amigae", "E (Amiga E 3.3)", "AmigaE|E|Amiga-E", "bin/EC", "e",
    "Assign >NIL: E: \"{dir}\"\n"
    "Assign >NIL: EMODULES: E:Modules\n"
    "Path >NIL: E:bin ADD\n",
    "EC {name}",
    "{name}",
    "PROC main()\n  WriteF('Hallo von {name}!\\n')\nENDPROC\n",
    "- Sprache: Amiga E, Compiler `EC` (v3.3a). Aufruf ohne Endung: `EC name` baut `name.e`.\n"
    "- Fehlerformat: `ERROR: ...` und danach `LINE 3: ...` (ohne Dateiname, gemeint ist die Hauptdatei).\n"
},
{
    "purebasic", "BASIC (PureBasic 4)", "PureBasic|purebasic", "Compilers/PBCompiler", "pb",
    "Assign >NIL: PureBasic: \"{dir}\"\n"
    "Stack 40000\n",
    "PureBasic:Compilers/PBCompiler {name}.pb TO {name}",
    "{name}",
    "PrintN(\"Hallo von {name}!\")\nEnd\n",
    "- Sprache: PureBasic 4.00 fuer AmigaOS (68k). Kein OpenConsole() - PrintN geht direkt.\n"
    "- Fehlerformat: `Error: Line 1 - ...` (Hauptdatei).\n"
},
{
    "vasm", "Assembler (vasm)", "VBCC|vbcc", "bin/vasmm68k_mot", "s",
    "",
    "\"{dir}/bin/vasmm68k_mot\" -Fhunkexe -nosym -quiet -o {name} {name}.s",
    "{name}",
    "; {name} - gibt Text ueber dos.library/PutStr aus (vasm, Motorola-Syntax)\n"
    "        section code,code\n"
    "start:  move.l  4.w,a6\n"
    "        lea     dosname(pc),a1\n"
    "        moveq   #36,d0\n"
    "        jsr     -552(a6)                ; OpenLibrary\n"
    "        tst.l   d0\n"
    "        beq.s   .fail\n"
    "        move.l  d0,a6\n"
    "        lea     text(pc),a0\n"
    "        move.l  a0,d1\n"
    "        jsr     -948(a6)                ; PutStr\n"
    "        move.l  a6,a1\n"
    "        move.l  4.w,a6\n"
    "        jsr     -414(a6)                ; CloseLibrary\n"
    ".fail:  moveq   #0,d0\n"
    "        rts\n"
    "dosname: dc.b   \"dos.library\",0\n"
    "text:    dc.b   \"Hallo von {name}!\",10,0\n",
    "- Sprache: 68000-Assembler (Motorola-Syntax), Assembler vasm, erzeugt direkt ein Hunk-Programm.\n"
    "- Fehlerformat: `error 2 in line 3 of \"datei.s\": ...`\n"
},
{
    "lua", "Lua 5.0", "Lua|lua", "lua", "lua",
    "",
    NULL,
    "\"{dir}/lua\" {name}.lua",
    "print(\"Hallo von {name}!\")\n",
    "- Sprache: Lua 5.0 (Interpreter, kein Build).\n"
},
{
    "python", "Python 2.0", "Python-2.0|Python|python", "Python", "py",
    "Assign >NIL: Python: \"{dir}\"\n",
    NULL,
    "Python:Python {name}.py",
    "print \"Hallo von {name}!\"\n",
    "- Sprache: Python 2.0 (alte Syntax: `print \"text\"`, keine f-Strings).\n"
},
{
    "arexx", "ARexx", "", "", "rexx",
    "",
    NULL,
    "SYS:Rexxc/rx {name}.rexx",
    "/* {name} - ARexx */\nSAY 'Hallo von {name}!'\n",
    "- Sprache: ARexx. RexxMast muss laufen. Das Skript muss mit einem Kommentar beginnen.\n"
},
{ NULL }
};

/* Wo nach Installationen gesucht wird: <Volume>:<Praefix><Verzeichnis> */
static const char *search_prefixes[] = { "Developer/", "", "Dev/", "Programming/", "Programmieren/", NULL };

static int exists(const char *path)
{
    BPTR lock = Lock(path, SHARED_LOCK);
    if (lock)
        UnLock(lock);
    return lock != 0;
}

static void sbf(StrBuf *sb, const char *fmt, ...)
{
    char buf[600];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    sb_add(sb, buf);
}

/* Namen aller eingebundenen Volumes (durch '\n' getrennt) */
static void volume_names(StrBuf *out)
{
    struct DosList *dl = LockDosList(LDF_VOLUMES | LDF_READ);

    while ((dl = NextDosEntry(dl, LDF_VOLUMES))) {
        UBYTE *b = (UBYTE *)BADDR(dl->dol_Name);
        if (b && b[0]) {
            sb_addn(out, (const char *)b + 1, b[0]);
            sb_add(out, "\n");
        }
    }
    UnLockDosList(LDF_VOLUMES | LDF_READ);
}

/* Installationsverzeichnis eines Profils suchen */
static int find_dir(const Profile *p, const char *volumes, char *dir, int len)
{
    const char *v = volumes;
    char vol[64], cand[300];

    if (!*p->marker) {
        /* ARexx: im System, nur rx pruefen */
        if (exists("SYS:Rexxc/rx")) {
            strcpy(dir, "SYS:Rexxc");
            return 1;
        }
        return 0;
    }
    while (*v) {
        const char *e = strchr(v, '\n');
        int n = e ? (int)(e - v) : (int)strlen(v);
        const char *names = p->dirnames;

        if (n >= (int)sizeof(vol) - 1)
            n = sizeof(vol) - 2;
        memcpy(vol, v, n);
        vol[n] = 0;
        v = e ? e + 1 : v + n;

        while (*names) {
            const char *ne = strchr(names, '|');
            int nl = ne ? (int)(ne - names) : (int)strlen(names);
            int i;
            for (i = 0; search_prefixes[i]; i++) {
                snprintf(cand, sizeof(cand), "%s:%s%.*s/%s", vol, search_prefixes[i], nl, names, p->marker);
                if (exists(cand)) {
                    snprintf(dir, len, "%s:%s%.*s", vol, search_prefixes[i], nl, names);
                    return 1;
                }
            }
            names = ne ? ne + 1 : names + nl;
        }
    }
    return 0;
}

/* NDK-Include-Verzeichnis fuer VBCC suchen */
static int find_ndk(const char *volumes, char *dir, int len)
{
    static const char *names[] = { "NDK3.2R4", "NDK3.2", "NDK_3.2", "NDK3.9", "NDK_3.9", "NDK", NULL };
    static const char *subs[] = { "Include_H", "include_h", "Include/include_h", NULL };
    const char *v = volumes;
    char vol[64], cand[300];

    while (*v) {
        const char *e = strchr(v, '\n');
        int n = e ? (int)(e - v) : (int)strlen(v), i, j, k;

        if (n >= (int)sizeof(vol) - 1)
            n = sizeof(vol) - 2;
        memcpy(vol, v, n);
        vol[n] = 0;
        v = e ? e + 1 : v + n;
        for (i = 0; names[i]; i++)
            for (j = 0; search_prefixes[j]; j++)
                for (k = 0; subs[k]; k++) {
                    snprintf(cand, sizeof(cand), "%s:%s%s/%s/exec/types.h",
                             vol, search_prefixes[j], names[i], subs[k]);
                    if (exists(cand)) {
                        snprintf(dir, len, "%s:%s%s/%s", vol, search_prefixes[j], names[i], subs[k]);
                        return 1;
                    }
                }
    }
    return 0;
}

/* Verzeichnis samt fehlender Elternverzeichnisse anlegen. 1 = existiert danach */
static int make_dirs(const char *path)
{
    char buf[512];
    char *p;
    BPTR lock;

    if (exists(path))
        return 1;
    strncpy(buf, path, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = 0;
    /* jede Ebene nach dem Volume-Doppelpunkt nacheinander anlegen */
    for (p = strchr(buf, ':'); p && *p; p++) {
        if (*p == '/') {
            *p = 0;
            if (!exists(buf) && (lock = CreateDir(buf)))
                UnLock(lock);
            *p = '/';
        }
    }
    if ((lock = CreateDir(buf)))
        UnLock(lock);
    return exists(path);
}

static void write_text(const char *path, const char *text)
{
    BPTR fh = Open(path, MODE_NEWFILE);
    if (fh) {
        Write(fh, text, strlen(text));
        Close(fh);
    }
}

int toolchains_scan(StrBuf *report)
{
    StrBuf vols, conf;
    char dir[300];
    int i, found = 0;
    BPTR lock;

    sb_init(&vols);
    sb_init(&conf);
    volume_names(&vols);
    sb_add(&conf, "# Von AmiCode gefundene Entwicklungswerkzeuge (/toolchains scan erneuert die Liste)\n");
    for (i = 0; profiles[i].id; i++) {
        if (!find_dir(&profiles[i], vols.buf ? vols.buf : "", dir, sizeof(dir)))
            continue;
        sbf(&conf, "[%s]\ndir=%s\n", profiles[i].id, dir);
        sbf(report, "  %-10s %-22s %s\n", profiles[i].id, profiles[i].name, dir);
        found++;
    }
    if (find_ndk(vols.buf ? vols.buf : "", dir, sizeof(dir))) {
        sbf(&conf, "[ndk]\ndir=%s\n", dir);
        sbf(report, "  %-10s %-22s %s\n", "ndk", "NDK-Header (fuer VBCC)", dir);
    }
    if ((lock = CreateDir("ENVARC:AmiCode")))
        UnLock(lock);
    if ((lock = CreateDir("ENV:AmiCode")))
        UnLock(lock);
    if (conf.buf) {
        write_text(TOOLCHAINS_CONF_ARC, conf.buf);
        write_text(TOOLCHAINS_CONF, conf.buf);
    }
    sb_free(&conf);
    sb_free(&vols);
    return found;
}

static int load_conf(Config *cfg)
{
    return config_load(cfg, TOOLCHAINS_CONF) || config_load(cfg, TOOLCHAINS_CONF_ARC);
}

void toolchains_list(StrBuf *out)
{
    Config cfg;
    char key[40];
    int i, n = 0;

    if (!load_conf(&cfg)) {
        sb_add(out, "Suche Entwicklungswerkzeuge ...\n");
        n = toolchains_scan(out);
        sbf(out, "%d Werkzeuge gefunden und in " TOOLCHAINS_CONF_ARC " gespeichert.\n", n);
        return;
    }
    sb_add(out, "Bekannte Entwicklungswerkzeuge (/toolchains scan sucht neu):\n");
    for (i = 0; profiles[i].id; i++) {
        const char *dir;
        snprintf(key, sizeof(key), "%s.dir", profiles[i].id);
        if ((dir = config_get(&cfg, key, NULL))) {
            sbf(out, "  %-10s %-22s %s\n", profiles[i].id, profiles[i].name, dir);
            n++;
        }
    }
    if (!n)
        sb_add(out, "  (keine)\n");
    sb_add(out, "Neues Projekt: /new <werkzeug> <verzeichnis>, z. B. /new vbcc Work:Projekte/demo\n");
    config_free(&cfg);
}

void toolchains_prompt(StrBuf *out)
{
    Config cfg;
    char key[40];
    int i, n = 0;

    if (!load_conf(&cfg))
        return;
    for (i = 0; profiles[i].id; i++) {
        snprintf(key, sizeof(key), "%s.dir", profiles[i].id);
        if (config_get(&cfg, key, NULL)) {
            sb_add(out, n ? ", " : "\nDevelopment tools installed on this Amiga: ");
            sb_add(out, profiles[i].name);
            n++;
        }
    }
    if (n)
        sb_add(out, ". Use the toolchain named in AMICODE.md; the project's build script sets up assigns and paths.\n");
    config_free(&cfg);
}

/* Platzhalter {name}, {dir}, {ndkadd} ersetzen */
static void expand(StrBuf *out, const char *tpl, const char *name, const char *dir, const char *ndk)
{
    const char *p = tpl;

    while (*p) {
        if (strncmp(p, "{name}", 6) == 0) {
            sb_add(out, name);
            p += 6;
        } else if (strncmp(p, "{dir}", 5) == 0) {
            sb_add(out, dir);
            p += 5;
        } else if (strncmp(p, "{ndkadd}", 8) == 0) {
            if (ndk && *ndk)
                sbf(out, "Assign >NIL: vincludeos3: \"%s\" ADD\n", ndk);
            p += 8;
        } else {
            sb_addn(out, p, 1);
            p++;
        }
    }
}

/* Erprobte Rezepte je Programmart: ersparen dem Agenten die (teure) Suche in Headern */
static void add_kind_recipe(StrBuf *md, const ProjectOptions *opt, const char *profile)
{
    if (!opt || !opt->kind || !strstr(opt->kind, "MUI"))
        return;
    sb_add(md, "\n## MUI-Rezept (erprobt, nicht erst in Headern suchen)\n\n"
               "- Vor dem ersten Code zusaetzlich den Skill `mui` mit read_skill laden.\n"
               "- Header: `#include <libraries/mui.h>`, `<proto/muimaster.h>`, `<proto/exec.h>`, "
               "`<proto/intuition.h>`, `<clib/alib_protos.h>`; MAKE_ID aus `<libraries/iffparse.h>`.\n"
               "- `struct Library *MUIMasterBase;` und `struct IntuitionBase *IntuitionBase;` global und "
               "NICHT static anlegen, oeffnen mit `OpenLibrary(MUIMASTER_NAME, MUIMASTER_VMIN)`.\n"
               "- Objekte mit den Makros aus libraries/mui.h (ApplicationObject, WindowObject, TextObject, "
               "End) bzw. MUI_NewObject(); `set()`/`get()`/`DoMethod()` sind vorhanden.\n"
               "- Hauptschleife: `while (DoMethod(app, MUIM_Application_NewInput, &sigs) != "
               "MUIV_Application_ReturnID_Quit) { if (sigs) sigs = Wait(sigs | SIGBREAKF_CTRL_C | eigene); }`\n"
               "- Zeitgeber (z. B. Uhr): timer.device mit eigenem MsgPort, dessen Signal in das Wait() "
               "aufnehmen und bei jedem Signal die Anzeige per set() erneuern.\n");
    if (stricmp(profile, "vbcc") == 0)
        sb_add(md, "- VBCC: das build-Skript linkt schon mit `-lamiga`; mehr braucht MUI nicht.\n");
}

/* Abschnitte der AMICODE.md mit den Angaben aus dem Assistenten */
static void add_project_info(StrBuf *md, const ProjectOptions *opt)
{
    if (!opt)
        return;
    if ((opt->kind && *opt->kind) || (opt->cpu && *opt->cpu) || (opt->os && *opt->os)) {
        sb_add(md, "\n## Zielsystem\n\n");
        if (opt->kind && *opt->kind)
            sbf(md, "- Programmart: %s\n", opt->kind);
        if (opt->cpu && *opt->cpu)
            sbf(md, "- CPU: mindestens %s (Code muss darauf laufen, keine Befehle neuerer CPUs)\n", opt->cpu);
        if (opt->os && *opt->os)
            sbf(md, "- Betriebssystem: mindestens %s (nur Funktionen, die es dort gibt)\n", opt->os);
    }
    if (opt->description && *opt->description) {
        sb_add(md, "\n## Aufgabe\n\n");
        sb_add(md, opt->description);
        if (opt->description[strlen(opt->description) - 1] != '\n')
            sb_add(md, "\n");
    }
}

/* Projekt ohne Vorgaben: nur Verzeichnis, AMICODE.md und Einstellungen.
   Der Agent waehlt Sprache und Werkzeug selbst. */
static int new_custom_project(const char *dir, const char *name, const ProjectOptions *opt, StrBuf *out)
{
    StrBuf md;
    char path[600];

    sb_init(&md);
    sbf(&md, "# %s\n\n", name);
    sb_add(&md, "- Eigenes Projekt ohne Vorgaben: Sprache und Werkzeug waehlt der Agent passend zur Aufgabe\n"
                "  aus den installierten Werkzeugen (siehe /toolchains).\n"
                "- Der Agent legt Quelltext und ein `build`-Skript an (Assigns/Pfade im Skript setzen) und\n"
                "  traegt in `.amicode/settings` unter [ui] `build=`, `run=` und `main=` ein.\n"
                "- Nach jeder Codeaenderung bauen und die Ausgabe pruefen.\n");
    add_project_info(&md, opt);
    snprintf(path, sizeof(path), "%s/AMICODE.md", dir);
    write_text(path, md.buf ? md.buf : "");
    sb_free(&md);

    snprintf(path, sizeof(path), "%s/.amicode/settings", dir);
    write_text(path, "# Projekteinstellungen fuer AmiCodeIDE (vom Agenten zu ergaenzen)\n[ui]\n");
    sbf(out, "Projekt %s angelegt in %s (ohne Vorgaben): AMICODE.md, .amicode/settings", name, dir);
    return 1;
}

void toolchains_choices(StrBuf *out)
{
    Config cfg;
    char key[40];
    int i;

    if (!load_conf(&cfg)) {
        StrBuf dummy;
        sb_init(&dummy);
        toolchains_scan(&dummy);
        sb_free(&dummy);
        if (!load_conf(&cfg))
            cfg.first = NULL;
    }
    for (i = 0; profiles[i].id; i++) {
        snprintf(key, sizeof(key), "%s.dir", profiles[i].id);
        if (config_get(&cfg, key, NULL))
            sbf(out, "%s\t%s\n", profiles[i].id, profiles[i].name);
    }
    sb_add(out, "custom\tEigenes Projekt (ohne Vorgaben)\n");
    config_free(&cfg);
}

int toolchains_new_project(const char *id, const char *dir, StrBuf *out)
{
    return toolchains_new_project2(id, dir, NULL, out);
}

int toolchains_new_project2(const char *id, const char *dir, const ProjectOptions *opt, StrBuf *out)
{
    const Profile *p = NULL;
    Config cfg;
    char key[40], name[64], path[600];
    const char *tdir = NULL, *ndk = NULL;
    StrBuf sb;
    BPTR lock;
    int i, custom = stricmp(id, "custom") == 0;

    for (i = 0; profiles[i].id; i++)
        if (stricmp(profiles[i].id, id) == 0)
            p = &profiles[i];
    if (!p && !custom) {
        sbf(out, "Unbekanntes Werkzeug '%s'. /toolchains zeigt die bekannten.", id);
        return 0;
    }
    cfg.first = NULL;
    if (!custom) {
        if (!load_conf(&cfg)) {
            StrBuf dummy;
            sb_init(&dummy);
            toolchains_scan(&dummy);
            sb_free(&dummy);
            if (!load_conf(&cfg)) {
                sb_add(out, "Keine Werkzeugliste - /toolchains scan ausfuehren.");
                return 0;
            }
        }
        snprintf(key, sizeof(key), "%s.dir", p->id);
        tdir = config_get(&cfg, key, NULL);
        ndk = config_get(&cfg, "ndk.dir", NULL);
        if (!tdir) {
            sbf(out, "%s ist auf diesem Amiga nicht gefunden worden (/toolchains scan).", p->name);
            config_free(&cfg);
            return 0;
        }
    }

    /* Projektname = letzter Teil des Pfads */
    strncpy(name, FilePart((STRPTR)dir), sizeof(name) - 1);
    name[sizeof(name) - 1] = 0;
    if (!name[0]) {
        sb_add(out, "Bitte ein Verzeichnis mit Namen angeben, z. B. Work:Projekte/demo");
        config_free(&cfg);
        return 0;
    }
    if (exists(dir)) {
        snprintf(path, sizeof(path), "%s/AMICODE.md", dir);
        if (exists(path)) {
            sbf(out, "In %s gibt es schon ein Projekt (AMICODE.md). Nichts veraendert.", dir);
            config_free(&cfg);
            return 0;
        }
    } else if (!make_dirs(dir)) {
        sbf(out, "Verzeichnis %s konnte nicht angelegt werden.", dir);
        config_free(&cfg);
        return 0;
    }
    snprintf(path, sizeof(path), "%s/.amicode", dir);
    if ((lock = CreateDir(path)))
        UnLock(lock);

    if (custom) {
        i = new_custom_project(dir, name, opt, out);
        config_free(&cfg);
        return i;
    }

    /* Hauptdatei */
    sb_init(&sb);
    expand(&sb, p->hello, name, tdir, ndk);
    snprintf(path, sizeof(path), "%s/%s.%s", dir, name, p->ext);
    write_text(path, sb.buf ? sb.buf : "");
    sb_free(&sb);

    /* build-Skript (nur fuer uebersetzte Sprachen) */
    if (p->build) {
        sb_init(&sb);
        sbf(&sb, "; Baut %s mit %s (von AmiCode angelegt)\n", name, p->name);
        expand(&sb, p->setup, name, tdir, ndk);
        expand(&sb, p->build, name, tdir, ndk);
        sb_add(&sb, "\n");
        snprintf(path, sizeof(path), "%s/build", dir);
        write_text(path, sb.buf ? sb.buf : "");
        sb_free(&sb);
    }

    /* Startskript: fuer Skriptsprachen mit Setup (Python), sonst direkter Befehl */
    sb_init(&sb);
    if (!p->build && *p->setup) {
        StrBuf run;
        sb_init(&run);
        sbf(&run, "; Startet %s mit %s (von AmiCode angelegt)\n", name, p->name);
        expand(&run, p->setup, name, tdir, ndk);
        expand(&run, p->run, name, tdir, ndk);
        sb_add(&run, "\n");
        snprintf(path, sizeof(path), "%s/run", dir);
        write_text(path, run.buf ? run.buf : "");
        sb_free(&run);
        sb_add(&sb, "Execute run");
    } else {
        expand(&sb, p->run, name, tdir, ndk);
    }

    /* .amicode/settings fuer AmiCodeIDE (Bauen/Starten) */
    {
        StrBuf st;
        sb_init(&st);
        sbf(&st, "# Projekteinstellungen fuer AmiCodeIDE\n[ui]\n");
        if (p->build)
            sb_add(&st, "build=Execute build\n");
        sbf(&st, "run=%s\nmain=%s.%s\ntoolchain=%s\n", sb.buf ? sb.buf : "", name, p->ext, p->id);
        snprintf(path, sizeof(path), "%s/.amicode/settings", dir);
        write_text(path, st.buf ? st.buf : "");
        sb_free(&st);
    }

    /* AMICODE.md: Regeln fuer den Agenten */
    {
        StrBuf md;
        sb_init(&md);
        sbf(&md, "# %s\n\n", name);
        sbf(&md, "- Werkzeug: %s (AmiCode-Profil `%s`)\n", p->name, p->id);
        sbf(&md, "- Hauptdatei: `%s.%s`\n", name, p->ext);
        if (p->build)
            sbf(&md, "- Bauen: `Execute build` (setzt Assigns und Pfad selbst). Ergebnis: Programm `%s`.\n", name);
        sbf(&md, "- Starten: `%s`\n", sb.buf ? sb.buf : "");
        sb_add(&md, p->rules);
        if (p->build)
            sb_add(&md, "- Nach jeder Codeaenderung bauen und die Ausgabe pruefen.\n");
        if (stricmp(p->ext, "c") == 0)
            sb_add(&md, "- Vor dem ersten Code: Skill `c-amiga` mit read_skill laden (Amiga-C, erprobtes Beispiel, Linkoptionen).\n");
        add_project_info(&md, opt);
        add_kind_recipe(&md, opt, p->id);
        snprintf(path, sizeof(path), "%s/AMICODE.md", dir);
        write_text(path, md.buf ? md.buf : "");
        sb_free(&md);
    }

    sbf(out, "Projekt %s angelegt in %s (%s):\n  %s.%s", name, dir, p->name, name, p->ext);
    if (p->build)
        sb_add(out, ", build");
    if (!p->build && *p->setup)
        sb_add(out, ", run");
    sb_add(out, ", AMICODE.md, .amicode/settings\n");
    sbf(out, "Starten mit: %s", sb.buf ? sb.buf : "");
    sb_free(&sb);
    config_free(&cfg);
    return 1;
}
