/* Skills: Amiga-Wissen fuer das Modell, als Markdown-Dateien neben dem Programm
   oder im Projekt. Kleine lokale Modelle (qwen & Co.) kennen AmigaDOS, Amiga-C
   und MUI kaum - mit "always" steht das Wissen fest im Systemprompt, mit "auto"
   holt es sich das Modell bei Bedarf ueber read_skill (spart Tokens). */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <exec/types.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>

#include "skills.h"

#define SKILL_MAXSIZE   (16 * 1024)

static const char *skill_dirs[] = { ".amicode/skills", "PROGDIR:Skills", NULL };

static void trim(char *s)
{
    int n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r' || s[n - 1] == ' '))
        s[--n] = 0;
}

/* Kopf lesen: "# Titel" und "Description: ..." */
static void read_head(SkillInfo *si)
{
    char line[256];
    BPTR fh = Open(si->path, MODE_OLDFILE);
    int n = 0;

    if (!fh)
        return;
    while (n++ < 6 && FGets(fh, line, sizeof(line))) {
        trim(line);
        if (line[0] == '#' && line[1] == ' ' && !si->title[0]) {
            strncpy(si->title, line + 2, sizeof(si->title) - 1);
        } else if (strnicmp(line, "Description:", 12) == 0) {
            char *d = line + 12;
            while (*d == ' ')
                d++;
            strncpy(si->desc, d, sizeof(si->desc) - 1);
            break;
        }
    }
    Close(fh);
}

int skills_scan(SkillInfo *out, int max)
{
    static struct FileInfoBlock fib __attribute__((aligned(4)));
    int d, n = 0, i;

    for (d = 0; skill_dirs[d]; d++) {
        BPTR lock = Lock(skill_dirs[d], SHARED_LOCK);
        if (!lock)
            continue;
        if (Examine(lock, &fib) && fib.fib_DirEntryType > 0) {
            while (n < max && ExNext(lock, &fib)) {
                int len = strlen(fib.fib_FileName), dup = 0;
                if (fib.fib_DirEntryType > 0 || len < 4 || len - 3 >= (int)sizeof(out->name) ||
                    stricmp(fib.fib_FileName + len - 3, ".md") != 0)
                    continue;
                for (i = 0; i < n && !dup; i++)       /* Projekt-Skill hat Vorrang */
                    dup = strnicmp(out[i].name, fib.fib_FileName, len - 3) == 0 && !out[i].name[len - 3];
                if (dup)
                    continue;
                memset(&out[n], 0, sizeof(out[n]));
                memcpy(out[n].name, fib.fib_FileName, len - 3);
                snprintf(out[n].path, sizeof(out[n].path), "%s/%s", skill_dirs[d], fib.fib_FileName);
                read_head(&out[n]);
                if (!out[n].title[0])
                    strcpy(out[n].title, out[n].name);
                n++;
            }
        }
        UnLock(lock);
    }
    /* nach Namen sortieren (wenige Eintraege) */
    for (i = 1; i < n; i++) {
        int j = i;
        while (j > 0 && stricmp(out[j - 1].name, out[j].name) > 0) {
            SkillInfo t = out[j];
            out[j] = out[j - 1];
            out[j - 1] = t;
            j--;
        }
    }
    return n;
}

int skills_mode(const Config *cfg, const char *name)
{
    char key[48];
    const char *v;

    snprintf(key, sizeof(key), "skills.%s", name);
    v = config_get(cfg, key, "auto");
    if (stricmp(v, "always") == 0)
        return SKILL_ALWAYS;
    if (stricmp(v, "off") == 0)
        return SKILL_OFF;
    return SKILL_AUTO;
}

const char *skills_mode_name(int mode)
{
    return mode == SKILL_ALWAYS ? "always" : mode == SKILL_OFF ? "off" : "auto";
}

/* Inhalt ohne Titel- und Description-Zeile anhaengen */
static int add_body(const SkillInfo *si, StrBuf *out)
{
    BPTR fh = Open(si->path, MODE_OLDFILE);
    char *buf, *p;
    long n;

    if (!fh)
        return 0;
    if (!(buf = malloc(SKILL_MAXSIZE + 1))) {
        Close(fh);
        return 0;
    }
    n = Read(fh, buf, SKILL_MAXSIZE);
    Close(fh);
    if (n < 0)
        n = 0;
    buf[n] = 0;
    p = buf;
    if (p[0] == '#' && p[1] == ' ' && (p = strchr(p, '\n')))
        p++;
    else
        p = buf;
    while (*p == '\n')
        p++;
    if (strnicmp(p, "Description:", 12) == 0 && (p = strchr(p, '\n')))
        p++;
    while (p && *p == '\n')
        p++;
    sb_add(out, p ? p : "");
    free(buf);
    return 1;
}

void skills_prompt(const Config *cfg, StrBuf *sys)
{
    static SkillInfo si[SKILLS_MAX];
    int n = skills_scan(si, SKILLS_MAX), i, autos = 0;

    for (i = 0; i < n; i++) {
        if (skills_mode(cfg, si[i].name) != SKILL_ALWAYS)
            continue;
        sb_add(sys, "\n# Skill: ");
        sb_add(sys, si[i].title);
        sb_add(sys, "\n");
        add_body(&si[i], sys);
    }
    for (i = 0; i < n; i++) {
        if (skills_mode(cfg, si[i].name) != SKILL_AUTO)
            continue;
        if (!autos++)
            sb_add(sys, "\nSkills with Amiga knowledge. Before starting such work, load the matching "
                        "skill with read_skill (once per session):\n");
        sb_add(sys, "- ");
        sb_add(sys, si[i].name);
        sb_add(sys, ": ");
        sb_add(sys, si[i].desc[0] ? si[i].desc : si[i].title);
        sb_add(sys, "\n");
    }
}

void skills_read(const Config *cfg, const char *name, StrBuf *out)
{
    static SkillInfo si[SKILLS_MAX];
    int n = skills_scan(si, SKILLS_MAX), i;

    for (i = 0; i < n; i++) {
        if (stricmp(si[i].name, name) != 0)
            continue;
        if (skills_mode(cfg, si[i].name) == SKILL_OFF)
            sb_add(out, "Fehler: dieser Skill ist abgeschaltet.");
        else if (!add_body(&si[i], out))
            sb_add(out, "Fehler: Skill konnte nicht gelesen werden.");
        return;
    }
    sb_add(out, "Fehler: kein Skill mit diesem Namen. Vorhanden:");
    for (i = 0; i < n; i++) {
        sb_add(out, " ");
        sb_add(out, si[i].name);
    }
}

void skills_list(const Config *cfg, StrBuf *out)
{
    static SkillInfo si[SKILLS_MAX];
    static const char *label[] = { "aus  ", "auto ", "immer" };
    int n = skills_scan(si, SKILLS_MAX), i;
    char line[300];

    if (!n) {
        sb_add(out, "Keine Skills gefunden (PROGDIR:Skills, .amicode/skills).");
        return;
    }
    for (i = 0; i < n; i++) {
        snprintf(line, sizeof(line), "%s  %-12.31s %.63s\n", label[skills_mode(cfg, si[i].name)],
                 si[i].name, si[i].title);
        sb_add(out, line);
    }
    sb_add(out, "Modus aendern: IDE-Menue Projekt/Skills... oder [skills] name=always|auto|off "
                "in der Konfiguration.");
}
