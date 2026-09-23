/* Fenster "Skills" fuer AmiCodeIDE: welches Amiga-Wissen das Modell bekommt.
   Laeuft im GUI-Prozess: skills_scan kommt ohne malloc aus. */

#include <stdio.h>
#include <string.h>

#include <exec/types.h>
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

#include "gui_skills.h"
#include "skills.h"

enum { KID_ALWAYS = SKILLS_ID_BASE, KID_AUTO, KID_OFF, KID_SAVE, KID_CANCEL, KID_ACTIVE };

static Object *win, *list, *desc, *bt_always, *bt_auto, *bt_off, *bt_save, *bt_cancel;
static SkillInfo skills[SKILLS_MAX];
static int modes[SKILLS_MAX];
static int count;

Object *skillswin_create(void)
{
    list = MUI_NewObject(MUIC_NList,
                         MUIA_Frame, MUIV_Frame_InputList,
                         MUIA_NList_ConstructHook, MUIV_NList_ConstructHook_String,
                         MUIA_NList_DestructHook, MUIV_NList_DestructHook_String,
                         TAG_DONE);
    if (!list)
        return NULL;
    win = WindowObject,
        MUIA_Window_Title, (ULONG)"AmiCodeIDE - Skills",
        MUIA_Window_ID, MAKE_ID('A','M','S','K'),
        MUIA_HelpNode, (ULONG)"SKILLS",
        WindowContents, VGroup,
            Child, TextObject, MUIA_Text_Contents, (ULONG)
                "Wissen, das der Agent ueber den Amiga bekommt.\n"
                "\33bImmer\33n: steht fest im Prompt (fuer lokale Modelle wie qwen).\n"
                "\33bAutomatisch\33n: das Modell holt es bei Bedarf (spart Tokens).", End,
            Child, MUI_NewObject(MUIC_NListview, MUIA_NListview_NList, (ULONG)list,
                                 MUIA_FixHeightTxt, (ULONG)"\n\n\n\n\n\n\n\n",
                                 MUIA_FixWidthTxt, (ULONG)"xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
                                 MUIA_CycleChain, 1, TAG_DONE),
            Child, desc = TextObject, TextFrame, MUIA_Background, MUII_TextBack,
                          MUIA_Text_Contents, (ULONG)"", MUIA_FixHeightTxt, (ULONG)"\n\n", End,
            Child, HGroup,
                Child, bt_always = SimpleButton("_Immer"),
                Child, bt_auto = SimpleButton("_Automatisch"),
                Child, bt_off = SimpleButton("A_us"),
            End,
            Child, HGroup,
                Child, bt_save = SimpleButton("_Speichern"),
                Child, bt_cancel = SimpleButton("Abbre_chen"),
            End,
        End,
    End;
    return win;
}

void skillswin_notify(Object *app)
{
    DoMethod(win, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, (ULONG)app, 2, MUIM_Application_ReturnID, KID_CANCEL);
    DoMethod(bt_always, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)app, 2, MUIM_Application_ReturnID, KID_ALWAYS);
    DoMethod(bt_auto, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)app, 2, MUIM_Application_ReturnID, KID_AUTO);
    DoMethod(bt_off, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)app, 2, MUIM_Application_ReturnID, KID_OFF);
    DoMethod(bt_save, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)app, 2, MUIM_Application_ReturnID, KID_SAVE);
    DoMethod(bt_cancel, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)app, 2, MUIM_Application_ReturnID, KID_CANCEL);
    DoMethod(list, MUIM_Notify, MUIA_NList_Active, MUIV_EveryTime, (ULONG)app, 2, MUIM_Application_ReturnID, KID_ACTIVE);
}

int skillswin_is_id(ULONG id)
{
    return id >= SKILLS_ID_BASE && id < SKILLS_ID_BASE + 20;
}

static void fill(LONG active)
{
    static const char *label[] = { "aus        ", "\33bautomatisch\33n", "\33bimmer\33n      " };
    static char line[160];
    int i;

    set(list, MUIA_NList_Quiet, TRUE);
    DoMethod(list, MUIM_NList_Clear);
    for (i = 0; i < count; i++) {
        snprintf(line, sizeof(line), "%s  %-10.31s %.63s", label[modes[i]], skills[i].name, skills[i].title);
        DoMethod(list, MUIM_NList_InsertSingle, (ULONG)line, MUIV_NList_Insert_Bottom);
    }
    set(list, MUIA_NList_Quiet, FALSE);
    set(list, MUIA_NList_Active, count ? active : MUIV_NList_Active_Off);
}

void skillswin_open(const Config *cfg)
{
    int i;

    count = skills_scan(skills, SKILLS_MAX);
    for (i = 0; i < count; i++)
        modes[i] = skills_mode(cfg, skills[i].name);
    fill(0);
    set(desc, MUIA_Text_Contents, count ? skills[0].desc :
        "Keine Skills gefunden (Skills neben dem Programm oder .amicode/skills im Projekt).");
    set(win, MUIA_Window_Open, TRUE);
    set(win, MUIA_Window_ActiveObject, list);
}

void skillswin_close(void)
{
    set(win, MUIA_Window_Open, FALSE);
}

int skillswin_handle(ULONG id)
{
    LONG pos = MUIV_NList_Active_Off;

    get(list, MUIA_NList_Active, &pos);
    switch (id) {
    case KID_ACTIVE:
        if (pos >= 0 && pos < count)
            set(desc, MUIA_Text_Contents, skills[pos].desc);
        break;
    case KID_ALWAYS:
    case KID_AUTO:
    case KID_OFF:
        if (pos >= 0 && pos < count) {
            modes[pos] = id == KID_ALWAYS ? SKILL_ALWAYS : id == KID_AUTO ? SKILL_AUTO : SKILL_OFF;
            fill(pos);
        }
        break;
    case KID_SAVE:
        skillswin_close();
        return SK_SAVE;
    case KID_CANCEL:
        skillswin_close();
        return SK_CANCEL;
    }
    return SK_NONE;
}

void skillswin_apply(Config *cfg)
{
    char key[48];
    int i;

    for (i = 0; i < count; i++) {
        snprintf(key, sizeof(key), "skills.%.31s", skills[i].name);
        config_set(cfg, key, skills_mode_name(modes[i]));
    }
}
