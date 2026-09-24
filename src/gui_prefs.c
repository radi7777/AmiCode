/* Einstellungsfenster fuer AmiCodeIDE: Anbieter (OpenAI, OpenRouter, Ollama,
   eigener OpenAI-kompatibler Server), Basis-URL, API-Key und Modell.
   Die Modellliste holt der Agent-Prozess (Netzwerk gehoert dorthin). */

#include <stdio.h>
#include <string.h>

#include <exec/types.h>
#include <exec/memory.h>
#include <libraries/mui.h>
#include <libraries/iffparse.h>
#include <libraries/asl.h>
#ifndef IPTR
#define IPTR ULONG
#endif
#include <mui/NList_mcc.h>
#include <mui/NListview_mcc.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>

#include "gui_prefs.h"
#include "gui_agent.h"
#include "provider.h"
#include "amiloc.h"
#include "theme.h"

#define MAX_PAGES   8

enum {
    PID_SAVE = PREFS_ID_BASE, PID_USE, PID_CANCEL, PID_THEMESET, PID_THEMEDEF,
    PID_LOAD = PREFS_ID_BASE + 10,      /* + Seite */
    PID_PICK = PREFS_ID_BASE + 30       /* + Seite */
};

static Object *win, *cycle, *pages, *stream_cm, *info;
static Object *pr_base[MAX_PAGES], *pr_key[MAX_PAGES], *pr_model[MAX_PAGES];
static Object *pr_pop[MAX_PAGES], *pr_list[MAX_PAGES], *pr_load[MAX_PAGES], *pr_ctx[MAX_PAGES];
static Object *pr_eff[MAX_PAGES], *maxcost_str;
/* Denkaufwand: Anzeige und Wert in der Konfiguration ([id] effort=) */
static const long effort_msgs[] = { MSG_PR_EFFORT_DEFAULT, MSG_PR_EFFORT_LOW, MSG_PR_EFFORT_MEDIUM, MSG_PR_EFFORT_HIGH };
static const char *effort_labels[5];
static const char *effort_values[] = { "default", "low", "medium", "high" };
static Object *bt_save, *bt_use, *bt_cancel;
static const char *cycle_labels[MAX_PAGES + 1];
static int npages, loading_page = -1;

/* Reiter Darstellung: bearbeitet eine Kopie beider Farbsaetze */
static Object *set_cy, *bt_defaults, *pp_bg[TA_COUNT], *pp_fg[TA_COUNT], *pp_syn[TS_COUNT];
static Object *font_fixed_str, *font_text_str;
static ThemeSet edit[2];
static int edit_cur;
static const char *set_labels[3], *tab_labels[3];
static const long area_msgs[TA_COUNT] = {
    MSG_AP_EDITOR, MSG_AP_OUTPUT, MSG_AP_LOG, MSG_AP_LIST, MSG_AP_INPUT, MSG_AP_STATUS, MSG_AP_WINDOW,
    MSG_AP_BUTTON
};
/* Poppen kennt kein "MUI-Standard": ein leerer Hintergrund wird als dieser Pen
   gezeigt und bleibt leer, solange ihn niemand aendert */
#define EMPTY_BG    "m2"
static const long syn_msgs[TS_COUNT] = {
    MSG_AP_KEYWORD, MSG_AP_COMMENT, MSG_AP_STRING, MSG_AP_PREPROC, MSG_AP_CURSOR, MSG_AP_MARK
};

static Object *font_pop(Object **str, int fixed)
{
    return PopaslObject,
        MUIA_Popstring_String, *str = StringObject, StringFrame, MUIA_String_MaxLen, THEME_FONT - 1,
                                      MUIA_CycleChain, 1, End,
        MUIA_Popstring_Button, PopButton(MUII_PopUp),
        MUIA_Popasl_Type, ASL_FontRequest,
        ASLFO_FixedWidthOnly, fixed,
    End;
}

static Object *make_appearance(void)
{
    Object *areas, *syn, *g;
    int i;

    set_labels[0] = GetStr(MSG_THEME_LIGHT);
    set_labels[1] = GetStr(MSG_THEME_DARK);
    areas = ColGroup(3), GroupFrameT(GetStr(MSG_AP_GROUP_AREAS)),
        Child, HSpace(0),
        Child, TextObject, MUIA_Text_PreParse, (ULONG)"\33c", MUIA_Text_Contents, (ULONG)GetStr(MSG_AP_BG), End,
        Child, TextObject, MUIA_Text_PreParse, (ULONG)"\33c", MUIA_Text_Contents, (ULONG)GetStr(MSG_AP_FG), End,
    End;
    syn = ColGroup(4), GroupFrameT(GetStr(MSG_AP_GROUP_SYNTAX)), End;
    if (!areas || !syn)
        return NULL;
    for (i = 0; i < TA_COUNT; i++) {
        DoMethod(areas, OM_ADDMEMBER, (ULONG)Label1(GetStr(area_msgs[i])));
        DoMethod(areas, OM_ADDMEMBER, (ULONG)(pp_bg[i] = PoppenObject, MUIA_CycleChain, 1, End));
        if (i == TA_BUTTON) {       /* Knopfschrift bleibt MUI-schwarz */
            pp_fg[i] = NULL;
            DoMethod(areas, OM_ADDMEMBER, (ULONG)HSpace(0));
        } else {
            DoMethod(areas, OM_ADDMEMBER, (ULONG)(pp_fg[i] = PoppenObject, MUIA_CycleChain, 1, End));
        }
        if (!pp_bg[i] || (!pp_fg[i] && i != TA_BUTTON))
            return NULL;
    }
    for (i = 0; i < TS_COUNT; i++) {
        DoMethod(syn, OM_ADDMEMBER, (ULONG)Label1(GetStr(syn_msgs[i])));
        DoMethod(syn, OM_ADDMEMBER, (ULONG)(pp_syn[i] = PoppenObject, MUIA_CycleChain, 1, End));
        if (!pp_syn[i])
            return NULL;
    }
    g = VGroup,
        Child, HGroup,
            Child, Label1(GetStr(MSG_AP_SET)),
            Child, set_cy = CycleObject, MUIA_Cycle_Entries, (ULONG)set_labels, MUIA_CycleChain, 1, End,
            Child, HSpace(0),
            Child, bt_defaults = SimpleButton(GetStr(MSG_AP_DEFAULTS)),
        End,
        Child, areas,
        Child, syn,
        Child, ColGroup(2), GroupFrameT(GetStr(MSG_AP_GROUP_FONTS)),
            Child, Label2(GetStr(MSG_AP_FONT_FIXED)),
            Child, font_pop(&font_fixed_str, TRUE),
            Child, Label2(GetStr(MSG_AP_FONT_TEXT)),
            Child, font_pop(&font_text_str, FALSE),
        End,
        Child, TextObject, MUIA_Font, MUIV_Font_Tiny, MUIA_Text_Contents, (ULONG)GetStr(MSG_AP_HINT), End,
    End;
    return g;
}

static void set_spec(Object *pp, const char *spec)
{
    static struct MUI_PenSpec ps;

    memset(&ps, 0, sizeof(ps));
    strncpy(ps.buf, spec, sizeof(ps.buf) - 1);
    set(pp, MUIA_Pendisplay_Spec, (ULONG)&ps);
}

static void get_spec(Object *pp, char *dst)
{
    struct MUI_PenSpec *ps = NULL;

    if (!pp)
        return;
    get(pp, MUIA_Pendisplay_Spec, &ps);
    if (!dst[0] && ps && strcmp(ps->buf, EMPTY_BG) == 0)
        return;             /* leer geblieben: weiter MUI-Standard */
    if (ps && ps->buf[0]) {
        strncpy(dst, ps->buf, THEME_SPEC - 1);
        dst[THEME_SPEC - 1] = 0;
    }
}

static void appearance_show(int set)
{
    int i;

    for (i = 0; i < TA_COUNT; i++) {
        set_spec(pp_bg[i], edit[set].bg[i][0] ? edit[set].bg[i] : EMPTY_BG);
        if (pp_fg[i])
            set_spec(pp_fg[i], edit[set].fg[i]);
    }
    for (i = 0; i < TS_COUNT; i++)
        set_spec(pp_syn[i], edit[set].syn[i]);
}

static void appearance_take(int set)
{
    int i;

    for (i = 0; i < TA_COUNT; i++) {
        get_spec(pp_bg[i], edit[set].bg[i]);
        get_spec(pp_fg[i], edit[set].fg[i]);
    }
    for (i = 0; i < TS_COUNT; i++)
        get_spec(pp_syn[i], edit[set].syn[i]);
}

static Object *make_page(int i, const ProviderDef *d)
{
    Object *lv;
    const char *hint = GetStr(stricmp(d->id, "ollamacloud") == 0 ? MSG_PR_HINT_OLLAMACLOUD :
                              d->needs_key ? MSG_PR_HINT_KEY :
                              stricmp(d->id, "ollama") == 0 ? MSG_PR_HINT_OLLAMA : MSG_PR_HINT_CUSTOM);

    pr_list[i] = MUI_NewObject(MUIC_NList,
                               MUIA_Frame, MUIV_Frame_InputList,
                               MUIA_NList_ConstructHook, MUIV_NList_ConstructHook_String,
                               MUIA_NList_DestructHook, MUIV_NList_DestructHook_String,
                               TAG_DONE);
    if (!pr_list[i])
        return NULL;
    lv = MUI_NewObject(MUIC_NListview, MUIA_NListview_NList, (ULONG)pr_list[i],
                       MUIA_FixHeightTxt, (ULONG)"\n\n\n\n\n\n\n\n\n\n\n",
                       MUIA_FixWidthTxt, (ULONG)"anthropic/claude-sonnet-4.5-xxxxxxxxxx",
                       TAG_DONE);

    pr_pop[i] = PopobjectObject,
        MUIA_Popstring_String, pr_model[i] = StringObject, StringFrame,
            MUIA_String_MaxLen, 128, MUIA_CycleChain, 1, End,
        MUIA_Popstring_Button, PopButton(MUII_PopUp),
        MUIA_Popobject_Object, lv,
    End;

    return VGroup,
        Child, ColGroup(2),
            Child, Label2(GetStr(MSG_PR_BASE)),
            Child, pr_base[i] = StringObject, StringFrame, MUIA_String_MaxLen, 250,
                                MUIA_CycleChain, 1, End,
            Child, Label2(GetStr(MSG_PR_KEY)),
            Child, pr_key[i] = StringObject, StringFrame, MUIA_String_MaxLen, 250,
                               MUIA_String_Secret, TRUE, MUIA_CycleChain, 1, End,
            Child, Label2(GetStr(MSG_PR_MODEL)),
            Child, HGroup,
                Child, pr_pop[i],
                Child, pr_load[i] = MUI_MakeObject(MUIO_Button, (ULONG)GetStr(MSG_PR_LOAD)),
            End,
            Child, Label2(GetStr(MSG_PR_CONTEXT)),
            Child, HGroup,
                Child, pr_ctx[i] = StringObject, StringFrame, MUIA_String_MaxLen, 8,
                                   MUIA_String_Accept, (ULONG)"0123456789",
                                   MUIA_FixWidthTxt, (ULONG)"00000000", MUIA_CycleChain, 1, End,
                Child, LLabel1(GetStr(MSG_PR_CONTEXT_HINT)),
                Child, HSpace(0),
            End,
            Child, Label1(GetStr(MSG_PR_EFFORT)),
            Child, HGroup,
                Child, pr_eff[i] = CycleObject, MUIA_Cycle_Entries, (ULONG)effort_labels,
                                   MUIA_CycleChain, 1, End,
                Child, LLabel1(GetStr(MSG_PR_EFFORT_HINT)),
                Child, HSpace(0),
            End,
        End,
        Child, TextObject, MUIA_Font, MUIV_Font_Tiny, MUIA_Text_Contents, (ULONG)hint, End,
    End;
}

Object *prefs_create(void)
{
    const ProviderDef *defs = provider_defs_list();
    Object *pg, *appearance;
    int i;

    for (i = 0; i < 4; i++)
        effort_labels[i] = GetStr(effort_msgs[i]);
    tab_labels[0] = GetStr(MSG_PR_TAB_PROVIDER);
    tab_labels[1] = GetStr(MSG_PR_TAB_APPEARANCE);
    if (!(appearance = make_appearance()))
        return NULL;
    for (npages = 0; defs[npages].id && npages < MAX_PAGES; npages++)
        cycle_labels[npages] = stricmp(defs[npages].id, "custom") == 0 ? GetStr(MSG_PR_CUSTOM) : defs[npages].name;
    cycle_labels[npages] = NULL;

    pg = MUI_NewObject(MUIC_Group, MUIA_Group_PageMode, TRUE, TAG_DONE);
    if (!pg)
        return NULL;
    for (i = 0; i < npages; i++) {
        Object *page = make_page(i, &defs[i]);
        if (!page)
            return NULL;
        DoMethod(pg, OM_ADDMEMBER, (ULONG)page);
    }
    pages = pg;

    win = WindowObject,
        MUIA_Window_Title, (ULONG)GetStr(MSG_PR_TITLE),
        MUIA_Window_ID, MAKE_ID('A','M','C','P'),
        MUIA_HelpNode, (ULONG)"PREFS",
        WindowContents, VGroup,
          Child, RegisterGroup(tab_labels), MUIA_CycleChain, 1,
           Child, VGroup,
            Child, HGroup,
                Child, Label1(GetStr(MSG_PR_PROVIDER)),
                Child, cycle = CycleObject, MUIA_Cycle_Entries, (ULONG)cycle_labels,
                                MUIA_CycleChain, 1, End,
            End,
            Child, VGroup, GroupFrameT(GetStr(MSG_PR_GROUP)),
                Child, pages,
            End,
            Child, HGroup,
                Child, stream_cm = CheckMark(TRUE),
                Child, LLabel1(GetStr(MSG_PR_STREAM)),
                Child, HSpace(0),
            End,
            Child, HGroup,
                Child, Label2(GetStr(MSG_PR_MAXCOST)),
                Child, maxcost_str = StringObject, StringFrame, MUIA_String_MaxLen, 8,
                                     MUIA_String_Accept, (ULONG)"0123456789.",
                                     MUIA_FixWidthTxt, (ULONG)"0000000", MUIA_CycleChain, 1, End,
                Child, LLabel1(GetStr(MSG_PR_MAXCOST_HINT)),
                Child, HSpace(0),
            End,
            Child, VSpace(0),
           End,
           Child, appearance,
          End,
            Child, info = TextObject, TextFrame, MUIA_Background, MUII_TextBack,
                          MUIA_Text_Contents, (ULONG)"", End,
            Child, HGroup,
                Child, bt_save = SimpleButton(GetStr(MSG_SAVE)),
                Child, bt_use = SimpleButton(GetStr(MSG_PR_USE)),
                Child, bt_cancel = SimpleButton(GetStr(MSG_CANCEL)),
            End,
        End,
    End;
    return win;
}

void prefs_notify(Object *app)
{
    int i;

    DoMethod(cycle, MUIM_Notify, MUIA_Cycle_Active, MUIV_EveryTime,
             (ULONG)pages, 3, MUIM_Set, MUIA_Group_ActivePage, MUIV_TriggerValue);
    DoMethod(win, MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
             (ULONG)app, 2, MUIM_Application_ReturnID, PID_CANCEL);
    DoMethod(bt_save, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)app, 2, MUIM_Application_ReturnID, PID_SAVE);
    DoMethod(bt_use, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)app, 2, MUIM_Application_ReturnID, PID_USE);
    DoMethod(bt_cancel, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)app, 2, MUIM_Application_ReturnID, PID_CANCEL);
    DoMethod(set_cy, MUIM_Notify, MUIA_Cycle_Active, MUIV_EveryTime,
             (ULONG)app, 2, MUIM_Application_ReturnID, PID_THEMESET);
    DoMethod(bt_defaults, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)app, 2, MUIM_Application_ReturnID, PID_THEMEDEF);
    for (i = 0; i < npages; i++) {
        DoMethod(pr_load[i], MUIM_Notify, MUIA_Pressed, FALSE,
                 (ULONG)app, 2, MUIM_Application_ReturnID, PID_LOAD + i);
        DoMethod(pr_list[i], MUIM_Notify, MUIA_NList_DoubleClick, MUIV_EveryTime,
                 (ULONG)app, 2, MUIM_Application_ReturnID, PID_PICK + i);
    }
}

int prefs_is_id(ULONG id)
{
    return id >= PREFS_ID_BASE && id < PREFS_ID_BASE + 100;
}

void prefs_open(const Config *cfg)
{
    const ProviderDef *defs = provider_defs_list();
    const char *active = provider_active(cfg);
    const char *st = config_get(cfg, "provider.stream", "yes");
    int i;

    for (i = 0; i < npages; i++) {
        ProviderSettings ps;
        provider_settings(cfg, defs[i].id, &ps);
        set(pr_base[i], MUIA_String_Contents, ps.base);
        set(pr_key[i], MUIA_String_Contents, ps.key);
        set(pr_model[i], MUIA_String_Contents, ps.model);
        {
            char key[64];
            snprintf(key, sizeof(key), "%s.context", defs[i].id);
            set(pr_ctx[i], MUIA_String_Contents,
                config_get(cfg, key, stricmp(defs[i].id, "ollama") == 0 ? "32768" : "0"));
        }
        {
            const char *eff = provider_effort(cfg, defs[i].id);
            int k, sel = 0;
            for (k = 1; k < 4; k++)
                if (stricmp(eff, effort_values[k]) == 0)
                    sel = k;
            set(pr_eff[i], MUIA_Cycle_Active, sel);
        }
        if (stricmp(defs[i].id, active) == 0)
            set(cycle, MUIA_Cycle_Active, i);
    }
    set(maxcost_str, MUIA_String_Contents, config_get(cfg, "agent.max_cost", "2.00"));
    set(stream_cm, MUIA_Selected, !(stricmp(st, "no") == 0 || stricmp(st, "0") == 0));
    set(info, MUIA_Text_Contents, GetStr(MSG_PR_INFO));
    edit[0] = theme.set[0];
    edit[1] = theme.set[1];
    edit_cur = theme.dark ? 1 : 0;
    nnset(set_cy, MUIA_Cycle_Active, edit_cur);
    appearance_show(edit_cur);
    set(font_fixed_str, MUIA_String_Contents, theme.font_fixed);
    set(font_text_str, MUIA_String_Contents, theme.font_text);
    set(win, MUIA_Window_Open, TRUE);
    for (i = 0; i < npages; i++)
        theme_list(pr_list[i], TA_LIST);
}

void prefs_close(void)
{
    set(win, MUIA_Window_Open, FALSE);
}

static char *get_str(Object *o)
{
    char *s = NULL;
    get(o, MUIA_String_Contents, &s);
    return s ? s : "";
}

static void load_models(int page)
{
    static char spec[700];
    const ProviderDef *defs = provider_defs_list();

    snprintf(spec, sizeof(spec), "%s\n%s\n%s", defs[page].id, get_str(pr_base[page]), get_str(pr_key[page]));
    loading_page = page;
    DoMethod(pr_list[page], MUIM_NList_Clear);
    set(info, MUIA_Text_Contents, GetStr(MSG_PR_LOADING));
    gui_agent_models(spec);
    memset(spec, 0, sizeof(spec));      /* Key nicht liegen lassen */
}

void prefs_models(const char *list)
{
    static char line[200];
    const char *p = list;
    int n = 0;

    if (loading_page < 0)
        return;
    set(pr_list[loading_page], MUIA_NList_Quiet, TRUE);
    while (*p) {
        const char *e = strchr(p, '\n');
        int len = e ? (int)(e - p) : (int)strlen(p);
        if (len > 0 && len < (int)sizeof(line)) {
            memcpy(line, p, len);
            line[len] = 0;
            DoMethod(pr_list[loading_page], MUIM_NList_InsertSingle, (ULONG)line, MUIV_NList_Insert_Bottom);
            n++;
        }
        if (!e)
            break;
        p = e + 1;
    }
    set(pr_list[loading_page], MUIA_NList_Quiet, FALSE);
    snprintf(line, sizeof(line), GetStr(MSG_PR_LOADED), n);
    set(info, MUIA_Text_Contents, line);
    if (n)
        DoMethod(pr_pop[loading_page], MUIM_Popstring_Open);
}

int prefs_handle(ULONG id)
{
    if (id == PID_SAVE)
        return PREFS_SAVE;
    if (id == PID_USE)
        return PREFS_USE;
    if (id == PID_CANCEL) {
        prefs_close();
        return PREFS_CANCEL;
    }
    if (id == PID_THEMESET) {
        LONG a = 0;
        get(set_cy, MUIA_Cycle_Active, &a);
        appearance_take(edit_cur);
        edit_cur = a ? 1 : 0;
        appearance_show(edit_cur);
        return PREFS_NONE;
    }
    if (id == PID_THEMEDEF) {
        theme_defaults(&edit[edit_cur], edit_cur);
        appearance_show(edit_cur);
        return PREFS_NONE;
    }
    if (id >= PID_LOAD && id < PID_LOAD + MAX_PAGES) {
        load_models(id - PID_LOAD);
        return PREFS_NONE;
    }
    if (id >= PID_PICK && id < PID_PICK + MAX_PAGES) {
        int page = id - PID_PICK;
        char *entry = NULL;
        DoMethod(pr_list[page], MUIM_NList_GetEntry, MUIV_NList_GetEntry_Active, (ULONG)&entry);
        if (entry)
            set(pr_model[page], MUIA_String_Contents, entry);
        DoMethod(pr_pop[page], MUIM_Popstring_Close, FALSE);
        return PREFS_NONE;
    }
    return PREFS_NONE;
}

void prefs_apply(Config *cfg)
{
    const ProviderDef *defs = provider_defs_list();
    LONG active = 0, stream = TRUE;
    char key[64];
    int i;

    get(cycle, MUIA_Cycle_Active, &active);
    get(stream_cm, MUIA_Selected, &stream);
    config_set(cfg, "provider.active", defs[active].id);
    config_set(cfg, "provider.stream", stream ? "yes" : "no");
    /* alte Eintraege aus der Zeit vor den Anbieter-Abschnitten entfernen */
    config_remove(cfg, "provider.endpoint");
    config_remove(cfg, "provider.api_key");
    config_remove(cfg, "provider.model");
    config_remove(cfg, "provider.name");
    for (i = 0; i < npages; i++) {
        snprintf(key, sizeof(key), "%s.base", defs[i].id);
        config_set(cfg, key, get_str(pr_base[i]));
        snprintf(key, sizeof(key), "%s.api_key", defs[i].id);
        config_set(cfg, key, get_str(pr_key[i]));
        snprintf(key, sizeof(key), "%s.model", defs[i].id);
        config_set(cfg, key, get_str(pr_model[i]));
        snprintf(key, sizeof(key), "%s.context", defs[i].id);
        config_set(cfg, key, *get_str(pr_ctx[i]) ? get_str(pr_ctx[i]) : "0");
        {
            LONG e = 0;
            get(pr_eff[i], MUIA_Cycle_Active, &e);
            snprintf(key, sizeof(key), "%s.effort", defs[i].id);
            config_set(cfg, key, effort_values[e >= 0 && e < 4 ? e : 0]);
        }
    }
    config_set(cfg, "agent.max_cost", *get_str(maxcost_str) ? get_str(maxcost_str) : "0");

    appearance_take(edit_cur);
    theme.set[0] = edit[0];
    theme.set[1] = edit[1];
    strncpy(theme.font_fixed, get_str(font_fixed_str), THEME_FONT - 1);
    strncpy(theme.font_text, get_str(font_text_str), THEME_FONT - 1);
    theme_store(cfg);
}
