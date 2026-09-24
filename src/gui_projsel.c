/* Fenster "Projekt oeffnen" fuer AmiCodeIDE: zeigt alle Projekte im
   Projektordner. Laeuft im GUI-Prozess, daher nur Lock/ExNext, kein malloc. */

#include <stdio.h>
#include <string.h>

#include <exec/types.h>
#include <dos/dos.h>
#include <libraries/mui.h>
#include <libraries/iffparse.h>
#ifndef IPTR
#define IPTR ULONG
#endif
#include <mui/NList_mcc.h>
#include <mui/NListview_mcc.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>

#include "gui_projsel.h"
#include "amiloc.h"
#include "theme.h"

enum { SID_OPEN = PROJSEL_ID_BASE, SID_NEW, SID_OTHER, SID_CANCEL, SID_DCLICK };

static Object *win, *list, *info, *bt_open, *bt_new, *bt_other, *bt_cancel;
static char base[512], selected[600];

Object *projsel_create(void)
{
    list = MUI_NewObject(MUIC_NList,
                         MUIA_Frame, MUIV_Frame_InputList,
                         MUIA_NList_ConstructHook, MUIV_NList_ConstructHook_String,
                         MUIA_NList_DestructHook, MUIV_NList_DestructHook_String,
                         TAG_DONE);
    if (!list)
        return NULL;
    win = WindowObject,
        MUIA_Window_Title, (ULONG)GetStr(MSG_PS_TITLE),
        MUIA_Window_ID, MAKE_ID('A','M','C','O'),
        MUIA_HelpNode, (ULONG)"NEWPROJECT",
        WindowContents, VGroup,
            Child, info = TextObject, MUIA_Text_Contents, (ULONG)"", End,
            Child, MUI_NewObject(MUIC_NListview, MUIA_NListview_NList, (ULONG)list,
                                 MUIA_FixHeightTxt, (ULONG)"\n\n\n\n\n\n\n\n\n\n\n\n",
                                 MUIA_FixWidthTxt, (ULONG)"xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
                                 MUIA_CycleChain, 1, TAG_DONE),
            Child, HGroup,
                Child, bt_open = SimpleButton(GetStr(MSG_PS_OPEN)),
                Child, bt_new = SimpleButton(GetStr(MSG_PS_NEW)),
                Child, bt_other = SimpleButton(GetStr(MSG_PS_OTHER)),
                Child, bt_cancel = SimpleButton(GetStr(MSG_CANCEL_C)),
            End,
        End,
    End;
    return win;
}

void projsel_notify(Object *app)
{
    DoMethod(win, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, (ULONG)app, 2, MUIM_Application_ReturnID, SID_CANCEL);
    DoMethod(bt_open, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)app, 2, MUIM_Application_ReturnID, SID_OPEN);
    DoMethod(bt_new, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)app, 2, MUIM_Application_ReturnID, SID_NEW);
    DoMethod(bt_other, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)app, 2, MUIM_Application_ReturnID, SID_OTHER);
    DoMethod(bt_cancel, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)app, 2, MUIM_Application_ReturnID, SID_CANCEL);
    DoMethod(list, MUIM_Notify, MUIA_NList_DoubleClick, MUIV_EveryTime, (ULONG)app, 2, MUIM_Application_ReturnID, SID_DCLICK);
}

int projsel_is_id(ULONG id)
{
    return id >= PROJSEL_ID_BASE && id < PROJSEL_ID_BASE + 20;
}

int projsel_open(const char *projects_dir, const char *current)
{
    static char line[200], text[600];
    struct FileInfoBlock *fib;
    BPTR lock;
    int n = 0, active = 0;

    strncpy(base, projects_dir, sizeof(base) - 1);
    base[sizeof(base) - 1] = 0;
    set(list, MUIA_NList_Quiet, TRUE);
    DoMethod(list, MUIM_NList_Clear);
    if ((lock = Lock(base, SHARED_LOCK))) {
        if ((fib = AllocDosObject(DOS_FIB, NULL))) {
            if (Examine(lock, fib)) {
                while (ExNext(lock, fib)) {
                    if (fib->fib_DirEntryType <= 0)
                        continue;
                    snprintf(line, sizeof(line), "%s", fib->fib_FileName);
                    DoMethod(list, MUIM_NList_InsertSingle, (ULONG)line, MUIV_NList_Insert_Sorted);
                    n++;
                }
            }
            FreeDosObject(DOS_FIB, fib);
        }
        UnLock(lock);
    }
    /* aktuelles Projekt vorauswaehlen */
    if (current) {
        const char *cur = FilePart((STRPTR)current);
        int i;
        for (i = 0; i < n; i++) {
            char *e = NULL;
            DoMethod(list, MUIM_NList_GetEntry, i, (ULONG)&e);
            if (e && stricmp(e, cur) == 0)
                active = i;
        }
    }
    set(list, MUIA_NList_Quiet, FALSE);
    set(list, MUIA_NList_Active, n ? active : MUIV_NList_Active_Off);
    snprintf(text, sizeof(text), GetStr(n ? MSG_PS_LIST : MSG_PS_EMPTY), base);
    set(info, MUIA_Text_Contents, text);
    set(win, MUIA_Window_Open, TRUE);
    theme_list(list, TA_LIST);
    set(win, MUIA_Window_ActiveObject, list);
    return n;
}

void projsel_close(void)
{
    set(win, MUIA_Window_Open, FALSE);
}

const char *projsel_selected(void)
{
    return selected;
}

int projsel_handle(ULONG id)
{
    char *entry = NULL;

    switch (id) {
    case SID_OPEN:
    case SID_DCLICK:
        DoMethod(list, MUIM_NList_GetEntry, MUIV_NList_GetEntry_Active, (ULONG)&entry);
        if (!entry)
            return PS_NONE;
        strncpy(selected, base, sizeof(selected) - 1);
        selected[sizeof(selected) - 1] = 0;
        AddPart(selected, entry, sizeof(selected));
        projsel_close();
        return PS_OPEN;
    case SID_NEW:
        projsel_close();
        return PS_NEW;
    case SID_OTHER:
        projsel_close();
        return PS_OTHER;
    case SID_CANCEL:
        projsel_close();
        return PS_CANCEL;
    }
    return PS_NONE;
}
