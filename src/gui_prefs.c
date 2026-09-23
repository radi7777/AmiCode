/* Einstellungsfenster fuer AmiCodeIDE: Anbieter (OpenAI, OpenRouter, Ollama,
   eigener OpenAI-kompatibler Server), Basis-URL, API-Key und Modell.
   Die Modellliste holt der Agent-Prozess (Netzwerk gehoert dorthin). */

#include <stdio.h>
#include <string.h>

#include <exec/types.h>
#include <exec/memory.h>
#include <libraries/mui.h>
#include <libraries/iffparse.h>
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

#define MAX_PAGES   8

enum {
    PID_SAVE = PREFS_ID_BASE, PID_USE, PID_CANCEL,
    PID_LOAD = PREFS_ID_BASE + 10,      /* + Seite */
    PID_PICK = PREFS_ID_BASE + 30       /* + Seite */
};

static Object *win, *cycle, *pages, *stream_cm, *info;
static Object *pr_base[MAX_PAGES], *pr_key[MAX_PAGES], *pr_model[MAX_PAGES];
static Object *pr_pop[MAX_PAGES], *pr_list[MAX_PAGES], *pr_load[MAX_PAGES], *pr_ctx[MAX_PAGES];
static Object *pr_eff[MAX_PAGES], *maxcost_str;
/* Denkaufwand: Anzeige und Wert in der Konfiguration ([id] effort=) */
static const char *effort_labels[] = { "Standard des Modells", "niedrig", "mittel", "hoch", NULL };
static const char *effort_values[] = { "default", "low", "medium", "high" };
static Object *bt_save, *bt_use, *bt_cancel;
static const char *cycle_labels[MAX_PAGES + 1];
static int npages, loading_page = -1;

static Object *make_page(int i, const ProviderDef *d)
{
    Object *lv;
    const char *hint = stricmp(d->id, "ollamacloud") == 0 ? "Grosse Modelle auf den Servern von ollama.com. API-Key: ollama.com/settings/keys" :
                       d->needs_key ? "API-Key noetig. Kontext 0 = Grenze des Anbieters" :
                       stricmp(d->id, "ollama") == 0 ? "Ollama: kein Key noetig, http://rechner:11434/v1. Kontext wird als num_ctx gesetzt (mehr braucht mehr Grafikspeicher)" :
                       "Key nur, wenn der Server einen verlangt. Kontext 0 = unbegrenzt";

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
            Child, Label2("Basis-URL:"),
            Child, pr_base[i] = StringObject, StringFrame, MUIA_String_MaxLen, 250,
                                MUIA_CycleChain, 1, End,
            Child, Label2("API-Key:"),
            Child, pr_key[i] = StringObject, StringFrame, MUIA_String_MaxLen, 250,
                               MUIA_String_Secret, TRUE, MUIA_CycleChain, 1, End,
            Child, Label2("Modell:"),
            Child, HGroup,
                Child, pr_pop[i],
                Child, pr_load[i] = MUI_MakeObject(MUIO_Button, (ULONG)"Modelle _laden"),
            End,
            Child, Label2("Kontext (Tokens):"),
            Child, HGroup,
                Child, pr_ctx[i] = StringObject, StringFrame, MUIA_String_MaxLen, 8,
                                   MUIA_String_Accept, (ULONG)"0123456789",
                                   MUIA_FixWidthTxt, (ULONG)"00000000", MUIA_CycleChain, 1, End,
                Child, LLabel1("ab 70% fasst AmiCode die Sitzung automatisch zusammen"),
                Child, HSpace(0),
            End,
            Child, Label1("Denkaufwand:"),
            Child, HGroup,
                Child, pr_eff[i] = CycleObject, MUIA_Cycle_Entries, (ULONG)effort_labels,
                                   MUIA_CycleChain, 1, End,
                Child, LLabel1("Denken kostet Ausgabe-Tokens; niedrig reicht meist"),
                Child, HSpace(0),
            End,
        End,
        Child, TextObject, MUIA_Font, MUIV_Font_Tiny, MUIA_Text_Contents, (ULONG)hint, End,
    End;
}

Object *prefs_create(void)
{
    const ProviderDef *defs = provider_defs_list();
    Object *pg;
    int i;

    for (npages = 0; defs[npages].id && npages < MAX_PAGES; npages++)
        cycle_labels[npages] = defs[npages].name;
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
        MUIA_Window_Title, (ULONG)"AmiCodeIDE - Einstellungen",
        MUIA_Window_ID, MAKE_ID('A','M','C','P'),
        MUIA_HelpNode, (ULONG)"PREFS",
        WindowContents, VGroup,
            Child, HGroup,
                Child, Label1("Anbieter:"),
                Child, cycle = CycleObject, MUIA_Cycle_Entries, (ULONG)cycle_labels,
                                MUIA_CycleChain, 1, End,
            End,
            Child, VGroup, GroupFrameT("Zugang und Modell"),
                Child, pages,
            End,
            Child, HGroup,
                Child, stream_cm = CheckMark(TRUE),
                Child, LLabel1("Antworten streamen (live anzeigen)"),
                Child, HSpace(0),
            End,
            Child, HGroup,
                Child, Label2("Kostenlimit pro Auftrag ($):"),
                Child, maxcost_str = StringObject, StringFrame, MUIA_String_MaxLen, 8,
                                     MUIA_String_Accept, (ULONG)"0123456789.",
                                     MUIA_FixWidthTxt, (ULONG)"0000000", MUIA_CycleChain, 1, End,
                Child, LLabel1("0 = keins; nur bei Anbietern, die Kosten melden (OpenRouter)"),
                Child, HSpace(0),
            End,
            Child, info = TextObject, TextFrame, MUIA_Background, MUII_TextBack,
                          MUIA_Text_Contents, (ULONG)"", End,
            Child, HGroup,
                Child, bt_save = SimpleButton("_Speichern"),
                Child, bt_use = SimpleButton("_Verwenden"),
                Child, bt_cancel = SimpleButton("_Abbrechen"),
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
    set(info, MUIA_Text_Contents, "Modell eintippen oder \"Modelle laden\" und aus der Liste waehlen.");
    set(win, MUIA_Window_Open, TRUE);
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
    set(info, MUIA_Text_Contents, "Lade Modelle ...");
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
    snprintf(line, sizeof(line), "%d Modelle - Doppelklick waehlt eins aus.", n);
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
}
