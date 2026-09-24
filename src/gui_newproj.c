/* Assistent "Neues Projekt" fuer AmiCodeIDE. Fragt alles ab, was der Agent
   braucht, legt das Projekt ueber den Agent-Prozess an und liefert auf Wunsch
   den ersten Auftrag, damit es direkt losgeht. */

#include <stdio.h>
#include <string.h>

#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <libraries/mui.h>
#include <libraries/asl.h>
#include <libraries/iffparse.h>
#ifndef IPTR
#define IPTR ULONG
#endif
#include <mui/NList_mcc.h>
#include <mui/NListview_mcc.h>
#include <mui/TextEditor_mcc.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>

#include "gui_newproj.h"
#include "gui_agent.h"
#include "amiloc.h"
#include "theme.h"

#define MAX_CHOICES 24

enum { NID_CREATE = NEWPROJ_ID_BASE, NID_CANCEL };

/* Programmart: angezeigt in der Sprache des Katalogs, in AMICODE.md immer englisch */
static const char *const kinds_en[] = {
    "Shell program (CLI)", "Window program (Intuition/GadTools)", "MUI application",
    "Other (library, tool, game ...)", NULL
};
static const long kind_msgs[] = { MSG_NP_KIND_CLI, MSG_NP_KIND_WINDOW, MSG_NP_KIND_MUI, MSG_NP_KIND_OTHER };
static const char *kinds[5];
static const char *cpus[] = { "68000", "68020", "68030", "68040", "68060", NULL };
static const char *oses[] = { "AmigaOS 3.0/3.1", "AmigaOS 3.2", "AmigaOS 3.9", NULL };

static Object *win, *name_str, *loc_str, *tclist, *kind_cy, *cpu_cy, *os_cy;
static Object *desc_ed, *go_cm, *info, *bt_create, *bt_cancel;
static char choice_id[MAX_CHOICES][24];
static int nchoices;
static char spec[6000], first[6000];
static char base_dir[512];          /* Projektordner, in dem das Projekt entsteht */

Object *newproj_create(void)
{
    Object *slider = ScrollbarObject, End;
    int i;

    for (i = 0; i < 4; i++)
        kinds[i] = GetStr(kind_msgs[i]);

    tclist = MUI_NewObject(MUIC_NList,
                           MUIA_Frame, MUIV_Frame_InputList,
                           MUIA_NList_ConstructHook, MUIV_NList_ConstructHook_String,
                           MUIA_NList_DestructHook, MUIV_NList_DestructHook_String,
                           TAG_DONE);
    desc_ed = MUI_NewObject(MUIC_TextEditor,
                            MUIA_TextEditor_Slider, (ULONG)slider,
                            MUIA_TextEditor_ExportHook, MUIV_TextEditor_ExportHook_NoStyle,
                            MUIA_CycleChain, 1,
                            TAG_DONE);
    if (!tclist || !desc_ed || !slider)
        return NULL;


    win = WindowObject,
        MUIA_Window_Title, (ULONG)GetStr(MSG_NP_TITLE),
        MUIA_Window_ID, MAKE_ID('A','M','C','N'),
        MUIA_HelpNode, (ULONG)"NEWPROJECT",
        WindowContents, VGroup,
            Child, VGroup, GroupFrameT(GetStr(MSG_NP_GROUP_PROJECT)),
                Child, ColGroup(2),
                    Child, Label2(GetStr(MSG_NP_NAME)),
                    Child, name_str = StringObject, StringFrame, MUIA_String_MaxLen, 30,
                                      MUIA_String_Accept, (ULONG)"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-.",
                                      MUIA_CycleChain, 1, End,
                    Child, Label2(GetStr(MSG_NP_LOCATION)),
                    Child, loc_str = TextObject, TextFrame, MUIA_Background, MUII_TextBack, End,
                End,
            End,
            Child, HGroup,
                Child, VGroup, GroupFrameT(GetStr(MSG_NP_GROUP_TOOL)),
                    Child, MUI_NewObject(MUIC_NListview, MUIA_NListview_NList, (ULONG)tclist,
                                         MUIA_FixHeightTxt, (ULONG)"\n\n\n\n\n\n\n\n\n\n",
                                         MUIA_CycleChain, 1, TAG_DONE),
                End,
                Child, VGroup, GroupFrameT(GetStr(MSG_NP_GROUP_TARGET)),
                    Child, ColGroup(2),
                        Child, Label1(GetStr(MSG_NP_KIND)),
                        Child, kind_cy = CycleObject, MUIA_Cycle_Entries, (ULONG)kinds, MUIA_CycleChain, 1, End,
                        Child, Label1(GetStr(MSG_NP_CPU)),
                        Child, cpu_cy = CycleObject, MUIA_Cycle_Entries, (ULONG)cpus,
                                        MUIA_Cycle_Active, 1, MUIA_CycleChain, 1, End,
                        Child, Label1(GetStr(MSG_NP_OS)),
                        Child, os_cy = CycleObject, MUIA_Cycle_Entries, (ULONG)oses,
                                       MUIA_Cycle_Active, 1, MUIA_CycleChain, 1, End,
                    End,
                    Child, VSpace(0),
                End,
            End,
            Child, VGroup, GroupFrameT(GetStr(MSG_NP_GROUP_DESC)),
                Child, HGroup, MUIA_Group_Spacing, 0,
                    MUIA_FixHeightTxt, (ULONG)"\n\n\n\n\n\n",
                    Child, desc_ed,
                    Child, slider,
                End,
            End,
            Child, HGroup,
                Child, go_cm = CheckMark(TRUE),
                Child, LLabel1(GetStr(MSG_NP_GO)),
                Child, HSpace(0),
            End,
            Child, info = TextObject, TextFrame, MUIA_Background, MUII_TextBack,
                          MUIA_Text_Contents, (ULONG)"", End,
            Child, HGroup,
                Child, bt_create = SimpleButton(GetStr(MSG_NP_CREATE)),
                Child, bt_cancel = SimpleButton(GetStr(MSG_NP_CANCEL)),
            End,
        End,
    End;
    return win;
}

void newproj_notify(Object *app)
{
    DoMethod(win, MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
             (ULONG)app, 2, MUIM_Application_ReturnID, NID_CANCEL);
    DoMethod(bt_create, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)app, 2, MUIM_Application_ReturnID, NID_CREATE);
    DoMethod(bt_cancel, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)app, 2, MUIM_Application_ReturnID, NID_CANCEL);
}

int newproj_is_id(ULONG id)
{
    return id >= NEWPROJ_ID_BASE && id < NEWPROJ_ID_BASE + 20;
}

void newproj_open(const char *default_dir)
{
    set(name_str, MUIA_String_Contents, "");
    strncpy(base_dir, default_dir ? default_dir : "", sizeof(base_dir) - 1);
    base_dir[sizeof(base_dir) - 1] = 0;
    set(loc_str, MUIA_Text_Contents, base_dir);
    DoMethod(desc_ed, MUIM_TextEditor_ClearText);
    DoMethod(tclist, MUIM_NList_Clear);
    nchoices = 0;
    set(info, MUIA_Text_Contents, GetStr(MSG_NP_READING));
    gui_agent_choices();
    set(win, MUIA_Window_Open, TRUE);
    theme_list(tclist, TA_LIST);
    set(win, MUIA_Window_ActiveObject, name_str);
}

void newproj_close(void)
{
    set(win, MUIA_Window_Open, FALSE);
}

void newproj_choices(const char *list)
{
    static char line[120];
    const char *p = list;

    DoMethod(tclist, MUIM_NList_Clear);
    nchoices = 0;
    while (*p && nchoices < MAX_CHOICES) {
        const char *tab = strchr(p, '\t'), *e = strchr(p, '\n');
        if (!tab || !e || tab > e)
            break;
        snprintf(choice_id[nchoices], sizeof(choice_id[0]), "%.*s", (int)(tab - p), p);
        snprintf(line, sizeof(line), "%.*s", (int)(e - tab - 1), tab + 1);
        DoMethod(tclist, MUIM_NList_InsertSingle, (ULONG)line, MUIV_NList_Insert_Bottom);
        nchoices++;
        p = e + 1;
    }
    set(tclist, MUIA_NList_Active, 0);
    set(info, MUIA_Text_Contents, GetStr(MSG_NP_HINT));
}

const char *newproj_spec(void)
{
    return spec;
}

const char *newproj_first_prompt(void)
{
    return first[0] ? first : NULL;
}

static char *get_str(Object *o)
{
    char *s = NULL;
    get(o, MUIA_String_Contents, &s);
    return s ? s : "";
}

/* Eingaben pruefen und Auftrag zusammenbauen. 1 = ok */
static int collect(void)
{
    char *name = get_str(name_str), *loc = base_dir, *desc;
    LONG tc = -1, kind = 0, cpu = 1, os = 1, go = TRUE;
    char dir[520];
    BPTR lock;
    int custom, n;

    get(tclist, MUIA_NList_Active, &tc);
    get(kind_cy, MUIA_Cycle_Active, &kind);
    get(cpu_cy, MUIA_Cycle_Active, &cpu);
    get(os_cy, MUIA_Cycle_Active, &os);
    get(go_cm, MUIA_Selected, &go);

    if (!*name) {
        set(info, MUIA_Text_Contents, GetStr(MSG_NP_ERR_NAME));
        set(win, MUIA_Window_ActiveObject, name_str);
        return 0;
    }
    if (!*loc || !(lock = Lock(loc, SHARED_LOCK))) {
        set(info, MUIA_Text_Contents, GetStr(MSG_NP_ERR_BASE));
        return 0;
    }
    UnLock(lock);
    {
        /* Name schon vergeben? */
        char test[520];
        strncpy(test, loc, sizeof(test) - 1);
        test[sizeof(test) - 1] = 0;
        AddPart(test, name, sizeof(test));
        if ((lock = Lock(test, SHARED_LOCK))) {
            UnLock(lock);
            set(info, MUIA_Text_Contents, GetStr(MSG_NP_ERR_EXISTS));
            set(win, MUIA_Window_ActiveObject, name_str);
            return 0;
        }
    }
    if (tc < 0 || tc >= nchoices) {
        set(info, MUIA_Text_Contents, GetStr(MSG_NP_ERR_TOOL));
        return 0;
    }
    custom = strcmp(choice_id[tc], "custom") == 0;

    strncpy(dir, loc, sizeof(dir) - 1);
    dir[sizeof(dir) - 1] = 0;
    AddPart(dir, name, sizeof(dir));

    desc = (char *)DoMethod(desc_ed, MUIM_TextEditor_ExportText);
    if (custom && go && (!desc || !*desc || !strcmp(desc, "\n"))) {
        if (desc)
            FreeVec(desc);
        set(info, MUIA_Text_Contents, GetStr(MSG_NP_ERR_DESC));
        set(win, MUIA_Window_ActiveObject, desc_ed);
        return 0;
    }

    n = snprintf(spec, sizeof(spec), "%s\n%s\n%s\n%s\n%s\n%s", choice_id[tc], dir,
                 kinds_en[kind], cpus[cpu], oses[os], desc ? desc : "");
    (void)n;
    first[0] = 0;
    if (go && desc && *desc && strcmp(desc, "\n")) {
        if (custom)
            snprintf(first, sizeof(first), GetStr(MSG_NP_FIRST_CUSTOM), name, desc);
        else
            snprintf(first, sizeof(first), GetStr(MSG_NP_FIRST), name, desc);
    }
    if (desc)
        FreeVec(desc);
    return 1;
}

int newproj_handle(ULONG id)
{
    if (id == NID_CANCEL) {
        newproj_close();
        return NP_CANCEL;
    }
    if (id == NID_CREATE) {
        if (!collect())
            return NP_NONE;
        newproj_close();
        return NP_CREATE;
    }
    return NP_NONE;
}
