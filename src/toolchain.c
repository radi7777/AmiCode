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
#include "amiloc.h"

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
    "#include <stdio.h>\n\nint main(void)\n{\n    printf(\"Hello from {name}!\\n\");\n    return 0;\n}\n",
    "- Language: C (C89/C99), compiler VBCC for AmigaOS 3.x (`vc +aos68k`).\n"
    "- Add further .c files to the vc line of the build script.\n"
    "- Amiga headers (proto/exec.h etc.) come from the NDK 3.2.\n"
    "- Linked with `-lamiga` (amiga.lib: DoMethod, CreateExtIO, varargs stubs like MUI_NewObject).\n"
    "  There is no mui.lib or `-lmui`.\n"
    "- Error format: `error 82 in line 18 of \"file.c\": ...`\n"
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
    "#include <stdio.h>\n\nint main(void)\n{\n    printf(\"Hello from {name}!\\n\");\n    return 0;\n}\n",
    "- Language: C (C89), compiler SAS/C 6 (`sc`, `slink`).\n"
    "- Add further files to the sc line of the build script.\n"
    "- Error format: `file.c 5 Error 9: ...`\n"
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
    "#include <stdio.h>\n\nint main(void)\n{\n    printf(\"Hello from {name}!\\n\");\n    return 0;\n}\n",
    "- Language: C, compiler gcc 2.95.3 from ADE, always with `-noixemul` (libnix, no ixemul.library).\n"
    "- gcc 2.95 does not fully know C99; declare variables at the start of a block.\n"
    "- Error format: `file.c:5: ...`\n"
},
{
    "amiblitz", "BASIC (AmiBlitz3)", "AmiBlitz3|Amiblitz3|AmiBlitz", "Amiblitz3", "ab3",
    "",
    "\"{dir}/Amiblitz3\" -s {name}.ab3 -e {name}",
    "{name}",
    "; {name} - AmiBlitz3\nNPrint \"Hello from {name}!\"\nEnd\n",
    "- Language: AmiBlitz3 (Blitz Basic), the source is plain text (`.ab3`).\n"
    "- The settings are a `; XTRA` comment block at the start of the file - do not remove it.\n"
    "- WARNING: the compiler returns code 0 even on errors. Success only with `0 errors` in the output.\n"
    "- Error format: `Compiler Error #1 in <file>:` and on the next line `Line 4: ...`\n"
},
{
    "amigae", "E (Amiga E 3.3)", "AmigaE|E|Amiga-E", "bin/EC", "e",
    "Assign >NIL: E: \"{dir}\"\n"
    "Assign >NIL: EMODULES: E:Modules\n"
    "Path >NIL: E:bin ADD\n",
    "EC {name}",
    "{name}",
    "PROC main()\n  WriteF('Hello from {name}!\\n')\nENDPROC\n",
    "- Language: Amiga E, compiler `EC` (v3.3a). Call without extension: `EC name` builds `name.e`.\n"
    "- Error format: `ERROR: ...` followed by `LINE 3: ...` (no file name, it means the main file).\n"
},
{
    "purebasic", "BASIC (PureBasic 4)", "PureBasic|purebasic", "Compilers/PBCompiler", "pb",
    "Assign >NIL: PureBasic: \"{dir}\"\n"
    "Stack 40000\n",
    "PureBasic:Compilers/PBCompiler {name}.pb TO {name}",
    "{name}",
    "PrintN(\"Hello from {name}!\")\nEnd\n",
    "- Language: PureBasic 4.00 for AmigaOS (68k). No OpenConsole() - PrintN works directly.\n"
    "- Error format: `Error: Line 1 - ...` (main file).\n"
},
{
    "vasm", "Assembler (vasm)", "VBCC|vbcc", "bin/vasmm68k_mot", "s",
    "",
    "\"{dir}/bin/vasmm68k_mot\" -Fhunkexe -nosym -quiet -o {name} {name}.s",
    "{name}",
    "; {name} - prints text with dos.library/PutStr (vasm, Motorola syntax)\n"
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
    "text:    dc.b   \"Hello from {name}!\",10,0\n",
    "- Language: 68000 assembler (Motorola syntax), assembler vasm, creates a hunk executable directly.\n"
    "- Error format: `error 2 in line 3 of \"file.s\": ...`\n"
},
{
    "lua", "Lua 5.0", "Lua|lua", "lua", "lua",
    "",
    NULL,
    "\"{dir}/lua\" {name}.lua",
    "print(\"Hello from {name}!\")\n",
    "- Language: Lua 5.0 (interpreter, no build).\n"
},
{
    "python", "Python 2.0", "Python-2.0|Python|python", "Python", "py",
    "Assign >NIL: Python: \"{dir}\"\n",
    NULL,
    "Python:Python {name}.py",
    "print \"Hello from {name}!\"\n",
    "- Language: Python 2.0 (old syntax: `print \"text\"`, no f-strings).\n"
},
{
    "arexx", "ARexx", "", "", "rexx",
    "",
    NULL,
    "SYS:Rexxc/rx {name}.rexx",
    "/* {name} - ARexx */\nSAY 'Hello from {name}!'\n",
    "- Language: ARexx. RexxMast must be running. The script must start with a comment.\n"
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
    sb_add(&conf, "# Development tools found by AmiCode (/toolchains scan renews the list)\n");
    for (i = 0; profiles[i].id; i++) {
        if (!find_dir(&profiles[i], vols.buf ? vols.buf : "", dir, sizeof(dir)))
            continue;
        sbf(&conf, "[%s]\ndir=%s\n", profiles[i].id, dir);
        sbf(report, "  %-10s %-22s %s\n", profiles[i].id, profiles[i].name, dir);
        found++;
    }
    if (find_ndk(vols.buf ? vols.buf : "", dir, sizeof(dir))) {
        sbf(&conf, "[ndk]\ndir=%s\n", dir);
        sbf(report, "  %-10s %-22s %s\n", "ndk", GetStr(MSG_TC_NDK), dir);
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
        sb_add(out, GetStr(MSG_TC_SEARCHING));
        n = toolchains_scan(out);
        sbf(out, GetStr(MSG_TC_SAVED), n, TOOLCHAINS_CONF_ARC);
        return;
    }
    sb_add(out, GetStr(MSG_TC_KNOWN));
    for (i = 0; profiles[i].id; i++) {
        const char *dir;
        snprintf(key, sizeof(key), "%s.dir", profiles[i].id);
        if ((dir = config_get(&cfg, key, NULL))) {
            sbf(out, "  %-10s %-22s %s\n", profiles[i].id, profiles[i].name, dir);
            n++;
        }
    }
    if (!n)
        sb_add(out, GetStr(MSG_TC_NONE));
    sb_add(out, GetStr(MSG_TC_NEW_HINT));
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
    sb_add(md, "\n## MUI recipe (tested, do not search the headers first)\n\n"
               "- Before the first code, also load the skill `mui` with read_skill.\n"
               "- Headers: `#include <libraries/mui.h>`, `<proto/muimaster.h>`, `<proto/exec.h>`, "
               "`<proto/intuition.h>`, `<clib/alib_protos.h>`; MAKE_ID from `<libraries/iffparse.h>`.\n"
               "- Declare `struct Library *MUIMasterBase;` and `struct IntuitionBase *IntuitionBase;` global and "
               "NOT static, open with `OpenLibrary(MUIMASTER_NAME, MUIMASTER_VMIN)`.\n"
               "- Objects with the macros from libraries/mui.h (ApplicationObject, WindowObject, TextObject, "
               "End) or MUI_NewObject(); `set()`/`get()`/`DoMethod()` are available.\n"
               "- Main loop: `while (DoMethod(app, MUIM_Application_NewInput, &sigs) != "
               "MUIV_Application_ReturnID_Quit) { if (sigs) sigs = Wait(sigs | SIGBREAKF_CTRL_C | own); }`\n"
               "- Timers (e.g. a clock): timer.device with its own MsgPort, add its signal to the Wait() "
               "and update the display with set() on every signal.\n");
    if (stricmp(profile, "vbcc") == 0)
        sb_add(md, "- VBCC: the build script already links with `-lamiga`; MUI needs nothing more.\n");
}

/* Abschnitte der AMICODE.md mit den Angaben aus dem Assistenten */
static void add_project_info(StrBuf *md, const ProjectOptions *opt)
{
    if (!opt)
        return;
    if ((opt->kind && *opt->kind) || (opt->cpu && *opt->cpu) || (opt->os && *opt->os)) {
        sb_add(md, "\n## Target system\n\n");
        if (opt->kind && *opt->kind)
            sbf(md, "- Program type: %s\n", opt->kind);
        if (opt->cpu && *opt->cpu)
            sbf(md, "- CPU: at least %s (the code must run on it, no instructions of newer CPUs)\n", opt->cpu);
        if (opt->os && *opt->os)
            sbf(md, "- Operating system: at least %s (only functions that exist there)\n", opt->os);
    }
    if (opt->description && *opt->description) {
        sb_add(md, "\n## Task\n\n");
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
    sb_add(&md, "- Own project without presets: the agent chooses language and tool to suit the task\n"
                "  from the installed tools (see /toolchains).\n"
                "- The agent creates the source and a `build` script (set assigns/paths in the script) and\n"
                "  enters `build=`, `run=` and `main=` under [ui] in `.amicode/settings`.\n"
                "- Build after every code change and check the output.\n");
    add_project_info(&md, opt);
    snprintf(path, sizeof(path), "%s/AMICODE.md", dir);
    write_text(path, md.buf ? md.buf : "");
    sb_free(&md);

    snprintf(path, sizeof(path), "%s/.amicode/settings", dir);
    write_text(path, "# Project settings for AmiCodeIDE (to be completed by the agent)\n[ui]\n");
    sbf(out, GetStr(MSG_TC_CREATED_CUSTOM), name, dir);
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
    sbf(out, "custom\t%s\n", GetStr(MSG_TC_CUSTOM));
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
        sbf(out, GetStr(MSG_TC_UNKNOWN), id);
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
                sb_add(out, GetStr(MSG_TC_NO_LIST));
                return 0;
            }
        }
        snprintf(key, sizeof(key), "%s.dir", p->id);
        tdir = config_get(&cfg, key, NULL);
        ndk = config_get(&cfg, "ndk.dir", NULL);
        if (!tdir) {
            sbf(out, GetStr(MSG_TC_NOT_FOUND), p->name);
            config_free(&cfg);
            return 0;
        }
    }

    /* Projektname = letzter Teil des Pfads */
    strncpy(name, FilePart((STRPTR)dir), sizeof(name) - 1);
    name[sizeof(name) - 1] = 0;
    if (!name[0]) {
        sb_add(out, GetStr(MSG_TC_NEED_DIR));
        config_free(&cfg);
        return 0;
    }
    if (exists(dir)) {
        snprintf(path, sizeof(path), "%s/AMICODE.md", dir);
        if (exists(path)) {
            sbf(out, GetStr(MSG_TC_EXISTS), dir);
            config_free(&cfg);
            return 0;
        }
    } else if (!make_dirs(dir)) {
        sbf(out, GetStr(MSG_TC_MKDIR_FAIL), dir);
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
        sbf(&sb, "; Builds %s with %s (created by AmiCode)\n", name, p->name);
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
        sbf(&run, "; Starts %s with %s (created by AmiCode)\n", name, p->name);
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
        sbf(&st, "# Project settings for AmiCodeIDE\n[ui]\n");
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
        sbf(&md, "- Tool: %s (AmiCode profile `%s`)\n", p->name, p->id);
        sbf(&md, "- Main file: `%s.%s`\n", name, p->ext);
        if (p->build)
            sbf(&md, "- Build: `Execute build` (sets assigns and path itself). Result: program `%s`.\n", name);
        sbf(&md, "- Run: `%s`\n", sb.buf ? sb.buf : "");
        sb_add(&md, p->rules);
        if (p->build)
            sb_add(&md, "- Build after every code change and check the output.\n");
        if (stricmp(p->ext, "c") == 0)
            sb_add(&md, "- Before the first code: load the skill `c-amiga` with read_skill (Amiga C, tested example, link options).\n");
        add_project_info(&md, opt);
        add_kind_recipe(&md, opt, p->id);
        snprintf(path, sizeof(path), "%s/AMICODE.md", dir);
        write_text(path, md.buf ? md.buf : "");
        sb_free(&md);
    }

    sbf(out, GetStr(MSG_TC_CREATED), name, dir, p->name, name, p->ext);
    if (p->build)
        sb_add(out, ", build");
    if (!p->build && *p->setup)
        sb_add(out, ", run");
    sb_add(out, ", AMICODE.md, .amicode/settings\n");
    sbf(out, GetStr(MSG_TC_RUN_WITH), sb.buf ? sb.buf : "");
    sb_free(&sb);
    config_free(&cfg);
    return 1;
}
