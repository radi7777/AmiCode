/* Fenster "Sitzungen" fuer AmiCodeIDE. Laeuft im GUI-Prozess, daher kein malloc:
   Namen liegen in einem festen Feld, die Liste kommt fertig vom Agenten. */

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

#include "gui_sessions.h"
#include "amiloc.h"
#include "theme.h"

#define MAX_SESSIONS 200

enum { SSID_RESUME = SESSIONS_ID_BASE, SSID_DELETE, SSID_CLOSE, SSID_DCLICK };

static Object *win, *list, *info, *bt_resume, *bt_delete, *bt_close;
static char names[MAX_SESSIONS][40];
static int count;
static char selected[40];

Object *sessions_create(void)
{
    list = MUI_NewObject(MUIC_NList,
                         MUIA_Frame, MUIV_Frame_InputList,
                         MUIA_NList_ConstructHook, MUIV_NList_ConstructHook_String,
                         MUIA_NList_DestructHook, MUIV_NList_DestructHook_String,
                         TAG_DONE);
    if (!list)
        return NULL;
    win = WindowObject,
        MUIA_Window_Title, (ULONG)GetStr(MSG_SS_TITLE),
        MUIA_Window_ID, MAKE_ID('A','M','S','S'),
        MUIA_HelpNode, (ULONG)"SESSIONS",
        WindowContents, VGroup,
            Child, info = TextObject, MUIA_Text_Contents, (ULONG)GetStr(MSG_SS_LOADING), End,
            Child, MUI_NewObject(MUIC_NListview, MUIA_NListview_NList, (ULONG)list,
                                 MUIA_FixHeightTxt, (ULONG)"\n\n\n\n\n\n\n\n\n\n\n\n",
                                 MUIA_FixWidthTxt, (ULONG)"xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
                                 MUIA_CycleChain, 1, TAG_DONE),
            Child, HGroup,
                Child, bt_resume = SimpleButton(GetStr(MSG_SS_RESUME)),
                Child, bt_delete = SimpleButton(GetStr(MSG_SS_DELETE)),
                Child, bt_close = SimpleButton(GetStr(MSG_SS_CLOSE)),
            End,
        End,
    End;
    return win;
}

void sessions_notify(Object *app)
{
    DoMethod(win, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, (ULONG)app, 2, MUIM_Application_ReturnID, SSID_CLOSE);
    DoMethod(bt_resume, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)app, 2, MUIM_Application_ReturnID, SSID_RESUME);
    DoMethod(bt_delete, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)app, 2, MUIM_Application_ReturnID, SSID_DELETE);
    DoMethod(bt_close, MUIM_Notify, MUIA_Pressed, FALSE, (ULONG)app, 2, MUIM_Application_ReturnID, SSID_CLOSE);
    DoMethod(list, MUIM_Notify, MUIA_NList_DoubleClick, MUIV_EveryTime, (ULONG)app, 2, MUIM_Application_ReturnID, SSID_DCLICK);
}

int sessions_is_id(ULONG id)
{
    return id >= SESSIONS_ID_BASE && id < SESSIONS_ID_BASE + 20;
}

void sessions_open(void)
{
    set(info, MUIA_Text_Contents, GetStr(MSG_SS_LOADING));
    set(win, MUIA_Window_Open, TRUE);
    theme_list(list, TA_LIST);
    set(win, MUIA_Window_ActiveObject, list);
}

void sessions_close(void)
{
    set(win, MUIA_Window_Open, FALSE);
}

int sessions_is_open(void)
{
    ULONG open = FALSE;
    get(win, MUIA_Window_Open, &open);
    return open != FALSE;
}

/* Zeilen "name\tdatum\tgroesse\ttitel"; name "*" = aktuelle Sitzung */
void sessions_fill(const char *text)
{
    static char line[200];
    const char *p = text;
    LONG active = 0;

    set(list, MUIA_NList_Quiet, TRUE);
    DoMethod(list, MUIM_NList_Clear);
    count = 0;
    while (*p && count < MAX_SESSIONS) {
        const char *f[4], *end = strchr(p, '\n');
        int k, len[4];

        if (!end)
            end = p + strlen(p);
        f[0] = p;
        for (k = 0; k < 4; k++) {
            const char *tab = k < 3 ? memchr(f[k], '\t', end - f[k]) : NULL;
            len[k] = (tab ? tab : end) - f[k];
            if (k < 3)
                f[k + 1] = tab ? tab + 1 : end;
        }
        if (len[0] > 0) {
            snprintf(names[count], sizeof(names[0]), "%.*s", len[0] > 39 ? 39 : len[0], f[0]);
            if (strcmp(names[count], "*") == 0)
                snprintf(line, sizeof(line), GetStr(MSG_SS_CURRENT), len[3], f[3]);
            else
                snprintf(line, sizeof(line), "%.*s  %6.*s  %.*s", len[1], f[1], len[2], f[2], len[3], f[3]);
            DoMethod(list, MUIM_NList_InsertSingle, (ULONG)line, MUIV_NList_Insert_Bottom);
            count++;
        }
        p = *end ? end + 1 : end;
    }
    /* aktuelle Sitzung steht oben; vorgewaehlt wird die neueste archivierte */
    if (count > 1 && strcmp(names[0], "*") == 0)
        active = 1;
    set(list, MUIA_NList_Quiet, FALSE);
    set(list, MUIA_NList_Active, count ? active : MUIV_NList_Active_Off);
    set(info, MUIA_Text_Contents, GetStr(count ? MSG_SS_LIST : MSG_SS_EMPTY));
}

const char *sessions_selected(void)
{
    return selected;
}

int sessions_handle(ULONG id)
{
    LONG pos = MUIV_NList_Active_Off;

    switch (id) {
    case SSID_RESUME:
    case SSID_DCLICK:
    case SSID_DELETE:
        get(list, MUIA_NList_Active, &pos);
        if (pos < 0 || pos >= count)
            return SS_NONE;
        strcpy(selected, strcmp(names[pos], "*") == 0 ? "" : names[pos]);
        if (id == SSID_DELETE)
            return selected[0] ? SS_DELETE : SS_NONE;   /* aktuelle: "Neue Sitzung" nehmen */
        sessions_close();
        return SS_RESUME;
    case SSID_CLOSE:
        sessions_close();
        return SS_CLOSE;
    }
    return SS_NONE;
}
