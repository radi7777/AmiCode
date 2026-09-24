/* Farben und Schriften der IDE - siehe theme.h.

   Laeuft nur im GUI-Prozess. Die MUIM_GetConfigItem-Nummern von TextEditor.mcc
   und BetterString.mcc sind nicht oeffentlich; sie stammen aus einem Test auf
   der Amiga (TextEditor 15.56, BetterString 11.x): Unterklasse, die jede
   Abfrage protokolliert, dann einzelne Nummern mit Signalfarben ueberschrieben. */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <graphics/text.h>
#include <libraries/mui.h>
#include <utility/tagitem.h>
#ifndef IPTR
#define IPTR ULONG
#endif
#include <mui/NList_mcc.h>
#include <mui/NListtree_mcc.h>
#include <mui/TextEditor_mcc.h>
#include <mui/BetterString_mcc.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/diskfont.h>
#include <proto/intuition.h>
#include <proto/utility.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>

#include "theme.h"

/* TextEditor.mcc: MUICFG_TextEditor_* */
#define TE_CFG_BACKGROUND   0xAD000051
#define TE_CFG_CURSOR       0xAD000054
#define TE_CFG_FIXEDFONT    0xAD000057
#define TE_CFG_MARKED       0xAD00005A
#define TE_CFG_NORMALFONT   0xAD00005B
#define TE_CFG_TEXT         0xAD00005F
/* BetterString.mcc: MUICFG_BetterString_* */
#define BS_CFG_INACTIVE_BG  0xAD000300
#define BS_CFG_INACTIVE_FG  0xAD000301
#define BS_CFG_ACTIVE_BG    0xAD000302
#define BS_CFG_ACTIVE_FG    0xAD000303
#define BS_CFG_CURSOR       0xAD000304
#define BS_CFG_MARKED       0xAD000305
/* NList.mcc: MUICFG_NList_Pen_List, MUICFG_NList_BG_List */
#define NL_CFG_PEN_LIST     0x9D510002
#define NL_CFG_BG_LIST      0x9D510007

#define MODE_FILE_ENV   "ENV:AmiCode/theme"
#define MODE_FILE_ARC   "ENVARC:AmiCode/theme"

Theme theme;

static const char *const area_keys[TA_COUNT] = {
    "editor", "output", "log", "list", "input", "status", "window", "button"
};
static const char *const syn_keys[TS_COUNT] = {
    "keyword", "comment", "string", "preproc", "cursor", "mark"
};

/* ---------- Farbsaetze ---------- */

static void rgb(char *spec, ULONG c)
{
    ULONG r = (c >> 16) & 0xff, g = (c >> 8) & 0xff, b = c & 0xff;
    snprintf(spec, THEME_SPEC, "r%08lX,%08lX,%08lX", r * 0x01010101UL, g * 0x01010101UL, b * 0x01010101UL);
}

static void pen(char *spec, const char *mui)
{
    strncpy(spec, mui, THEME_SPEC - 1);
    spec[THEME_SPEC - 1] = 0;
}

void theme_defaults(ThemeSet *s, int dark)
{
    static const ULONG dark_bg[TA_COUNT] = { 0x1e1e1e, 0x1e1e1e, 0x181818, 0x252526, 0x2d2d2d, 0x2d2d2d, 0x333333, 0xa0a0a0 };
    static const ULONG dark_fg[TA_COUNT] = { 0xd4d4d4, 0xd4d4d4, 0xc8c8c8, 0xd0d0d0, 0xe0e0e0, 0xc0c0c0, 0xc8c8c8, 0x000000 };
    static const ULONG dark_syn[TS_COUNT] = { 0x569cd6, 0x6a9955, 0xce9178, 0xc586c0, 0xaeafad, 0x264f78 };
    static const ULONG light_syn[TS_COUNT] = { 0x0000a0, 0x008000, 0xa31515, 0x800080, 0x000000, 0xadd6ff };
    int i;

    memset(s, 0, sizeof(*s));
    for (i = 0; i < TA_COUNT; i++) {
        if (dark) {
            rgb(s->bg[i], dark_bg[i]);
            rgb(s->fg[i], dark_fg[i]);
        } else {
            /* Light sieht aus wie bisher: weisse Textflaechen, sonst MUI-Standard */
            if (i != TA_STATUS && i != TA_WINDOW && i != TA_BUTTON)
                rgb(s->bg[i], 0xffffff);
            pen(s->fg[i], "m5");
        }
    }
    for (i = 0; i < TS_COUNT; i++)
        rgb(s->syn[i], dark ? dark_syn[i] : light_syn[i]);
}

/* Nur Specs uebernehmen, die MUI versteht (r..., m..., p...) */
static void take(char *dst, const char *v)
{
    if (v && (v[0] == 'r' || v[0] == 'm' || v[0] == 'p') && strlen(v) < THEME_SPEC)
        strcpy(dst, v);
}

static int read_mode(const char *path)
{
    char buf[8];
    BPTR fh = Open(path, MODE_OLDFILE);
    LONG n;

    if (!fh)
        return -1;
    n = Read(fh, buf, sizeof(buf) - 1);
    Close(fh);
    if (n <= 0)
        return -1;
    buf[n] = 0;
    return strnicmp(buf, "dark", 4) == 0;
}

void theme_load(const Config *cfg)
{
    char key[48];
    int s, i, m;

    for (s = 0; s < 2; s++) {
        theme_defaults(&theme.set[s], s);
        for (i = 0; i < TA_COUNT; i++) {
            snprintf(key, sizeof(key), "theme.%s.%s.bg", s ? "dark" : "light", area_keys[i]);
            take(theme.set[s].bg[i], config_get(cfg, key, NULL));
            snprintf(key, sizeof(key), "theme.%s.%s.fg", s ? "dark" : "light", area_keys[i]);
            take(theme.set[s].fg[i], config_get(cfg, key, NULL));
        }
        for (i = 0; i < TS_COUNT; i++) {
            snprintf(key, sizeof(key), "theme.%s.%s", s ? "dark" : "light", syn_keys[i]);
            take(theme.set[s].syn[i], config_get(cfg, key, NULL));
        }
    }
    strncpy(theme.font_fixed, config_get(cfg, "theme.font_fixed", ""), THEME_FONT - 1);
    strncpy(theme.font_text, config_get(cfg, "theme.font_text", ""), THEME_FONT - 1);
    if ((m = read_mode(MODE_FILE_ENV)) < 0 && (m = read_mode(MODE_FILE_ARC)) < 0)
        m = 0;
    theme.dark = m;
}

void theme_store(Config *cfg)
{
    char key[48];
    int s, i;

    for (s = 0; s < 2; s++) {
        for (i = 0; i < TA_COUNT; i++) {
            snprintf(key, sizeof(key), "theme.%s.%s.bg", s ? "dark" : "light", area_keys[i]);
            config_set(cfg, key, theme.set[s].bg[i]);
            snprintf(key, sizeof(key), "theme.%s.%s.fg", s ? "dark" : "light", area_keys[i]);
            config_set(cfg, key, theme.set[s].fg[i]);
        }
        for (i = 0; i < TS_COUNT; i++) {
            snprintf(key, sizeof(key), "theme.%s.%s", s ? "dark" : "light", syn_keys[i]);
            config_set(cfg, key, theme.set[s].syn[i]);
        }
    }
    config_set(cfg, "theme.font_fixed", theme.font_fixed);
    config_set(cfg, "theme.font_text", theme.font_text);
}

static void write_mode(const char *path)
{
    BPTR fh = Open(path, MODE_NEWFILE);

    if (fh) {
        Write(fh, theme.dark ? "dark\n" : "light\n", theme.dark ? 5 : 6);
        Close(fh);
    }
}

void theme_save_mode(void)
{
    BPTR lock;

    if ((lock = CreateDir("ENV:AmiCode")))
        UnLock(lock);
    if ((lock = CreateDir("ENVARC:AmiCode")))
        UnLock(lock);
    write_mode(MODE_FILE_ENV);
    write_mode(MODE_FILE_ARC);
}

const ThemeSet *theme_cur(void)
{
    return &theme.set[theme.dark ? 1 : 0];
}

const char *theme_image(int area)
{
    static char img[TA_COUNT][THEME_SPEC + 4];

    if (!theme_cur()->bg[area][0])
        return NULL;
    snprintf(img[area], sizeof(img[area]), "2:%s", theme_cur()->bg[area]);
    return img[area];
}

void theme_background(Object *obj, int area, ULONG def)
{
    const char *img = theme_image(area);

    if (obj)
        set(obj, MUIA_Background, img ? (ULONG)img : def);
}

/* ---------- TextEditor- und BetterString-Unterklassen ---------- */

/* MUI_ObtainPen liefert -1 bei Fehler; gueltige Pens haben Flags in den
   oberen Bits und sind oft negativ - also nie auf >= 0 pruefen. */
struct TEData {
    int area;
    LONG pens[4];           /* Syntaxfarben, MUI_ObtainPen */
    ULONG cmap[8];          /* MUIA_TextEditor_ColorMap: \33p[n] nimmt cmap[n-1] */
};

static struct MUI_CustomClass *te_class, *bs_class, *nl_class, *nlt_class, *tx_class;
static int new_area;        /* Bereich fuer das naechste OM_NEW */

/* Wert fuer MUIM_GetConfigItem liefern; 0 = an die Oberklasse weiterreichen */
static ULONG answer(struct MUIP_GetConfigItem *m, const char *value)
{
    if (!value || !*value)
        return 0;
    *m->storage = (ULONG)value;
    return 1;
}

static ULONG te_dispatch(struct IClass *cl __asm("a0"), Object *obj __asm("a2"), Msg msg __asm("a1"))
{
    struct TEData *data;
    int i;

    switch (msg->MethodID) {
    case OM_NEW: {
        struct TagItem tags[2];
        if (!(obj = (Object *)DoSuperMethodA(cl, obj, msg)))
            return 0;
        data = INST_DATA(cl, obj);
        data->area = new_area;
        for (i = 0; i < 4; i++)
            data->pens[i] = -1;
        for (i = 0; i < 8; i++)
            data->cmap[i] = 1;
        tags[0].ti_Tag = MUIA_TextEditor_ColorMap;
        tags[0].ti_Data = (ULONG)data->cmap;
        tags[1].ti_Tag = TAG_DONE;
        SetAttrsA(obj, tags);
        return (ULONG)obj;
    }
    case MUIM_Setup:
        if (!DoSuperMethodA(cl, obj, msg))
            return FALSE;
        data = INST_DATA(cl, obj);
        for (i = 0; i < 4; i++) {
            data->pens[i] = MUI_ObtainPen(muiRenderInfo(obj),
                                          (struct MUI_PenSpec *)theme_cur()->syn[TS_KEYWORD + i], 0);
            data->cmap[i] = data->pens[i] != -1 ? MUIPEN(data->pens[i]) : 1;
        }
        return TRUE;
    case MUIM_Cleanup:
        data = INST_DATA(cl, obj);
        for (i = 0; i < 4; i++)
            if (data->pens[i] != -1) {
                MUI_ReleasePen(muiRenderInfo(obj), data->pens[i]);
                data->pens[i] = -1;
            }
        break;
    case MUIM_GetConfigItem: {
        struct MUIP_GetConfigItem *m = (struct MUIP_GetConfigItem *)msg;
        const ThemeSet *s = theme_cur();
        data = INST_DATA(cl, obj);
        switch (m->id) {
        case TE_CFG_TEXT:       if (answer(m, s->fg[data->area])) return TRUE; break;
        case TE_CFG_BACKGROUND: if (answer(m, theme_image(data->area))) return TRUE; break;
        case TE_CFG_CURSOR:     if (answer(m, s->syn[TS_CURSOR])) return TRUE; break;
        case TE_CFG_MARKED:     if (answer(m, s->syn[TS_MARK])) return TRUE; break;
        case TE_CFG_NORMALFONT: if (answer(m, theme.font_text)) return TRUE; break;
        case TE_CFG_FIXEDFONT:  if (answer(m, theme.font_fixed)) return TRUE; break;
        }
        break;
    }
    }
    return DoSuperMethodA(cl, obj, msg);
}

static ULONG bs_dispatch(struct IClass *cl __asm("a0"), Object *obj __asm("a2"), Msg msg __asm("a1"))
{
    if (msg->MethodID == MUIM_GetConfigItem) {
        struct MUIP_GetConfigItem *m = (struct MUIP_GetConfigItem *)msg;
        const ThemeSet *s = theme_cur();
        const char *v = NULL;

        switch (m->id) {
        case BS_CFG_INACTIVE_BG:
        case BS_CFG_ACTIVE_BG:   v = theme_image(TA_INPUT); break;
        case BS_CFG_INACTIVE_FG:
        case BS_CFG_ACTIVE_FG:   v = s->fg[TA_INPUT]; break;
        case BS_CFG_CURSOR:      v = s->syn[TS_CURSOR]; break;
        case BS_CFG_MARKED:      v = s->syn[TS_MARK]; break;
        }
        if (answer(m, v))
            return TRUE;
    }
    return DoSuperMethodA(cl, obj, msg);
}

/* NList und NListtree: Instanzdaten nur der Bereich */
static ULONG nl_dispatch(struct IClass *cl __asm("a0"), Object *obj __asm("a2"), Msg msg __asm("a1"))
{
    int *area;

    switch (msg->MethodID) {
    case OM_NEW:
        if (!(obj = (Object *)DoSuperMethodA(cl, obj, msg)))
            return 0;
        area = INST_DATA(cl, obj);
        *area = new_area;
        return (ULONG)obj;
    case MUIM_GetConfigItem: {
        struct MUIP_GetConfigItem *m = (struct MUIP_GetConfigItem *)msg;
        area = INST_DATA(cl, obj);
        if (m->id == NL_CFG_PEN_LIST && answer(m, theme_cur()->fg[*area]))
            return TRUE;
        if (m->id == NL_CFG_BG_LIST && answer(m, theme_image(*area)))
            return TRUE;
        break;
    }
    }
    return DoSuperMethodA(cl, obj, msg);
}

/* Text: "\33P[pen]" vor den Inhalt setzen. Der Inhalt ohne Farbe bleibt
   gespeichert, damit ein neuer Pen (Theme-Wechsel) ihn neu setzen kann. */
#define TX_MAX  320

struct TXData {
    int area;
    char text[TX_MAX];
};

static LONG text_pens[TA_COUNT] = { -1, -1, -1, -1, -1, -1, -1, -1 };

/* ti_Data eines MUIA_Text_Contents-Tags durch die eingefaerbte Fassung ersetzen */
static void tx_color(struct TXData *data, struct TagItem *tags, char *buf, int len)
{
    struct TagItem *t = FindTagItem(MUIA_Text_Contents, tags);
    const char *v;

    if (!t)
        return;
    v = t->ti_Data ? (const char *)t->ti_Data : "";
    if (v != data->text) {
        strncpy(data->text, v, TX_MAX - 1);
        data->text[TX_MAX - 1] = 0;
    }
    if (text_pens[data->area] != -1 && theme_cur()->fg[data->area][0]) {
        snprintf(buf, len, "\33P[%ld]%s", (long)MUIPEN(text_pens[data->area]), data->text);
        t->ti_Data = (ULONG)buf;
    }
}

static ULONG tx_dispatch(struct IClass *cl __asm("a0"), Object *obj __asm("a2"), Msg msg __asm("a1"))
{
    struct TXData *data;
    char buf[TX_MAX + 16];

    switch (msg->MethodID) {
    case OM_NEW:
        if (!(obj = (Object *)DoSuperMethodA(cl, obj, msg)))
            return 0;
        data = INST_DATA(cl, obj);
        data->area = new_area;
        data->text[0] = 0;
        {
            struct TagItem *t = FindTagItem(MUIA_Text_Contents, ((struct opSet *)msg)->ops_AttrList);
            if (t && t->ti_Data)
                strncpy(data->text, (const char *)t->ti_Data, TX_MAX - 1);
        }
        return (ULONG)obj;
    case OM_SET: {
        ULONG r;
        struct TagItem *t;
        ULONG old = 0;
        data = INST_DATA(cl, obj);
        t = FindTagItem(MUIA_Text_Contents, ((struct opSet *)msg)->ops_AttrList);
        if (t)
            old = t->ti_Data;
        tx_color(data, ((struct opSet *)msg)->ops_AttrList, buf, sizeof(buf));
        r = DoSuperMethodA(cl, obj, msg);
        if (t)
            t->ti_Data = old;       /* Tagliste des Aufrufers unveraendert zurueckgeben */
        return r;
    }
    case MUIM_ThemeText_Refresh: {
        struct TagItem tags[2];
        data = INST_DATA(cl, obj);
        tags[0].ti_Tag = MUIA_Text_Contents;
        tags[0].ti_Data = (ULONG)data->text;
        tags[1].ti_Tag = TAG_DONE;
        SetAttrsA(obj, tags);
        return 0;
    }
    }
    return DoSuperMethodA(cl, obj, msg);
}

int theme_classes_init(void)
{
    if (!te_class)
        te_class = MUI_CreateCustomClass(NULL, MUIC_TextEditor, NULL, sizeof(struct TEData), (APTR)te_dispatch);
    if (!bs_class)
        bs_class = MUI_CreateCustomClass(NULL, MUIC_BetterString, NULL, 4, (APTR)bs_dispatch);
    if (!nl_class)
        nl_class = MUI_CreateCustomClass(NULL, MUIC_NList, NULL, sizeof(int), (APTR)nl_dispatch);
    if (!nlt_class)
        nlt_class = MUI_CreateCustomClass(NULL, MUIC_NListtree, NULL, sizeof(int), (APTR)nl_dispatch);
    if (!tx_class)
        tx_class = MUI_CreateCustomClass(NULL, MUIC_Text, NULL, sizeof(struct TXData), (APTR)tx_dispatch);
    return te_class != NULL;
}

void theme_classes_free(void)
{
    if (te_class)
        MUI_DeleteCustomClass(te_class);
    if (bs_class)
        MUI_DeleteCustomClass(bs_class);
    if (nl_class)
        MUI_DeleteCustomClass(nl_class);
    if (nlt_class)
        MUI_DeleteCustomClass(nlt_class);
    if (tx_class)
        MUI_DeleteCustomClass(tx_class);
    te_class = bs_class = nl_class = nlt_class = tx_class = NULL;
}

Object *theme_texteditor(int area, ULONG tag1, ...)
{
    if (!te_class)
        return NULL;
    new_area = area;
    return NewObjectA(te_class->mcc_Class, NULL, (struct TagItem *)&tag1);
}

Object *theme_betterstring(ULONG tag1, ...)
{
    if (!bs_class)
        return NULL;
    return NewObjectA(bs_class->mcc_Class, NULL, (struct TagItem *)&tag1);
}

Object *theme_nlist(int area, ULONG tag1, ...)
{
    if (!nl_class)
        return NULL;
    new_area = area;
    return NewObjectA(nl_class->mcc_Class, NULL, (struct TagItem *)&tag1);
}

Object *theme_nlisttree(int area, ULONG tag1, ...)
{
    if (!nlt_class)
        return NULL;
    new_area = area;
    return NewObjectA(nlt_class->mcc_Class, NULL, (struct TagItem *)&tag1);
}

Object *theme_text(int area, ULONG tag1, ...)
{
    if (!tx_class)
        return NULL;
    new_area = area;
    return NewObjectA(tx_class->mcc_Class, NULL, (struct TagItem *)&tag1);
}

/* ---------- Listen und Texte ---------- */

void theme_list(Object *list, int area)
{
    const char *img = theme_image(area);

    if (!list)
        return;
    if (theme_cur()->fg[area][0])
        set(list, MUIA_NList_ListPen, (ULONG)theme_cur()->fg[area]);
    if (img)
        set(list, MUIA_NList_ListBackground, (ULONG)img);
}

void theme_pens_obtain(Object *obj)
{
    static const int areas[] = { TA_STATUS, TA_WINDOW };
    int i;

    theme_pens_release(obj);
    for (i = 0; i < 2; i++)
        text_pens[areas[i]] = MUI_ObtainPen(muiRenderInfo(obj),
                                            (struct MUI_PenSpec *)theme_cur()->fg[areas[i]], 0);
}

void theme_pens_release(Object *obj)
{
    int i;

    for (i = 0; i < TA_COUNT; i++)
        if (text_pens[i] != -1) {
            MUI_ReleasePen(muiRenderInfo(obj), text_pens[i]);
            text_pens[i] = -1;
        }
}

/* ---------- Schriften ---------- */

struct Library *DiskfontBase;
static struct TextFont *fonts[2];

static struct TextFont *open_font(const char *spec)
{
    char name[THEME_FONT + 8];
    struct TextAttr ta;
    const char *slash = strchr(spec, '/');
    int nl;

    if (!*spec || !slash || slash == spec)
        return NULL;
    if (!DiskfontBase && !(DiskfontBase = OpenLibrary("diskfont.library", 36)))
        return NULL;
    nl = slash - spec;
    if (nl > THEME_FONT - 1)
        nl = THEME_FONT - 1;
    snprintf(name, sizeof(name), "%.*s.font", nl, spec);
    ta.ta_Name = (STRPTR)name;
    ta.ta_YSize = atoi(slash + 1);
    ta.ta_Style = 0;
    ta.ta_Flags = 0;
    if (ta.ta_YSize <= 0)
        return NULL;
    return OpenDiskFont(&ta);
}

struct TextFont *theme_font(int fixed)
{
    int i = fixed ? 1 : 0;

    if (!fonts[i])
        fonts[i] = open_font(fixed ? theme.font_fixed : theme.font_text);
    return fonts[i];
}

void theme_fonts_close(void)
{
    int i;

    for (i = 0; i < 2; i++)
        if (fonts[i]) {
            CloseFont(fonts[i]);
            fonts[i] = NULL;
        }
    if (DiskfontBase) {
        CloseLibrary(DiskfontBase);
        DiskfontBase = NULL;
    }
}
