# MUI 3.8 GUI programming
Description: Building GUIs with MUI (Magic User Interface) 3.8 in C: objects, notifications, main loop, cleanup. Read before writing or changing MUI code.

## Setup
```c
#include <libraries/mui.h>
#include <libraries/iffparse.h>     /* MAKE_ID */
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>       /* DoMethod */

struct Library *MUIMasterBase;       /* global, NOT static */
struct IntuitionBase *IntuitionBase;

MUIMasterBase = OpenLibrary(MUIMASTER_NAME, MUIMASTER_VMIN);
```
- Link with amiga.lib (`-lamiga` in vbcc). There is no mui.lib.
- Only use MUI 3.8 features; no MUI 4/5 attributes (they fail silently or crash on 3.8).

## Building the object tree
```c
Object *app, *win, *txt, *btn;
app = ApplicationObject,
    MUIA_Application_Title, (ULONG)"Uhr",
    MUIA_Application_Version, (ULONG)"$VER: Uhr 1.0 (23.9.2026)",
    MUIA_Application_Base, (ULONG)"UHR",
    SubWindow, win = WindowObject,
        MUIA_Window_Title, (ULONG)"Uhr",
        MUIA_Window_ID, MAKE_ID('U','H','R','1'),
        WindowContents, VGroup,
            Child, txt = TextObject, TextFrame, MUIA_Background, MUII_TextBack,
                MUIA_Text_PreParse, (ULONG)"\33c", MUIA_Text_Contents, (ULONG)"--:--", End,
            Child, btn = SimpleButton("_Ende"),
        End,
    End,
End;
if (!app) { /* a class is missing or out of memory: clean up and quit */ }
```
- Every `...Object,` needs a matching `End` - NEVER close a macro object with `TAG_DONE` (that leaves the tree open: "identifier expected" at the last End). `TAG_DONE` only ends NewObject()/MUI_NewObject() calls. Pointers and strings in tag lists are cast to ULONG.
- Groups: VGroup, HGroup, ColGroup(n), GroupFrameT("Titel"). Spacing: HSpace(0), VSpace(0).
- Gadgets: SimpleButton("_Label"), StringObject/StringFrame, Label1/Label2("Text:"), CheckMark(TRUE), CycleObject + MUIA_Cycle_Entries, ListviewObject + ListObject.
- Text escapes: `\33c` centre, `\33r` right, `\33b` bold, `\33i` italic, `\33n` normal.

## Notifications (instead of callbacks)
```c
DoMethod(win, MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
         app, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
DoMethod(btn, MUIM_Notify, MUIA_Pressed, FALSE,
         app, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
```
- Own actions: define IDs (enum { ID_START = 1, ... }) and return them with MUIM_Application_ReturnID.
- Change or read attributes: `set(txt, MUIA_Text_Contents, (ULONG)buf);` `get(str, MUIA_String_Contents, &ptr);` The text is copied by MUI.

## Main loop
```c
ULONG sigs = 0, id;
set(win, MUIA_Window_Open, TRUE);
while ((id = DoMethod(app, MUIM_Application_NewInput, &sigs)) != MUIV_Application_ReturnID_Quit) {
    if (id == ID_START) { /* ... */ }
    if (sigs) {
        sigs = Wait(sigs | SIGBREAKF_CTRL_C | timersig);
        if (sigs & SIGBREAKF_CTRL_C) break;
        if (sigs & timersig) { /* handle timer, update with set() */ }
    }
}
set(win, MUIA_Window_Open, FALSE);
MUI_DisposeObject(app);          /* disposes all child objects and windows */
CloseLibrary(MUIMasterBase);
```
- Add your own signal bits (timer.device port, other ports) to the Wait() call; do not use Delay() loops in a GUI.
- Check that the window really opened: `get(win, MUIA_Window_Open, &open)`.

## Own drawing area (custom class) - complete tested example
Use this when you need to draw (clock, graph, game field): a subclass of Area with its own MUIM_Draw,
redrawn every second from the main loop. Copy the structure, especially the dispatcher declaration.
```c
#include <stdio.h>
#include <exec/types.h>
#include <dos/dos.h>
#include <devices/timer.h>
#include <libraries/mui.h>
#include <libraries/iffparse.h>          /* MAKE_ID */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <clib/alib_protos.h>            /* DoMethod, DoSuperMethodA */

struct Library *MUIMasterBase;           /* global, NOT static */
struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;

/* MUI calls the dispatcher with arguments in registers a0/a2/a1.
   A plain C function gets garbage there and crashes the machine. */
#if defined(__VBCC__)
#define DISPATCHER_ARGS __reg("a0") struct IClass *cl, __reg("a2") Object *obj, __reg("a1") Msg msg
#elif defined(__GNUC__)
#define DISPATCHER_ARGS struct IClass *cl __asm("a0"), Object *obj __asm("a2"), Msg msg __asm("a1")
#else /* SAS/C */
#define DISPATCHER_ARGS register __a0 struct IClass *cl, register __a2 Object *obj, register __a1 Msg msg
#endif

struct PaintData { LONG count; };        /* instance data of the class */

static ULONG paint_dispatcher(DISPATCHER_ARGS)
{
    switch (msg->MethodID) {
    case MUIM_AskMinMax: {
        struct MUIP_AskMinMax *m = (struct MUIP_AskMinMax *)msg;
        DoSuperMethodA(cl, obj, msg);
        m->MinMaxInfo->MinWidth  += 100;  /* always ADD to the values */
        m->MinMaxInfo->DefWidth  += 200;
        m->MinMaxInfo->MaxWidth  += MUI_MAXMAX;
        m->MinMaxInfo->MinHeight += 100;
        m->MinMaxInfo->DefHeight += 200;
        m->MinMaxInfo->MaxHeight += MUI_MAXMAX;
        return 0;
    }
    case MUIM_Draw: {
        struct PaintData *data = INST_DATA(cl, obj);
        struct RastPort *rp = _rp(obj);
        LONG x0 = _mleft(obj), y0 = _mtop(obj), w = _mwidth(obj), h = _mheight(obj);
        char buf[32];
        int len;

        DoSuperMethodA(cl, obj, msg);
        if (!(((struct MUIP_Draw *)msg)->flags & (MADF_DRAWOBJECT | MADF_DRAWUPDATE)))
            return 0;
        SetAPen(rp, 0);                   /* clear own area only */
        RectFill(rp, x0, y0, x0 + w - 1, y0 + h - 1);
        SetAPen(rp, 1);
        Move(rp, x0 + w / 2, y0 + h / 2);
        Draw(rp, x0 + (data->count * 7) % w, y0);
        len = sprintf(buf, "%ld", (long)data->count);
        Move(rp, x0 + 4, y0 + h - 4);
        Text(rp, buf, len);
        return 0;
    }
    }
    return DoSuperMethodA(cl, obj, msg);
}

int main(void)
{
    struct MUI_CustomClass *mcc = NULL;
    struct MsgPort *tport = NULL;
    struct timerequest *treq = NULL;
    Object *app = NULL, *win, *paint;
    BOOL timer_open = FALSE;
    ULONG sigs = 0;
    int rc = RETURN_FAIL;

    IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 39);
    GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", 39);
    MUIMasterBase = OpenLibrary(MUIMASTER_NAME, MUIMASTER_VMIN);
    if (!IntuitionBase || !GfxBase || !MUIMasterBase)
        goto cleanup;

    /* own class based on Area; create objects of it with NewObject(mcc->mcc_Class, NULL, ...) */
    if (!(mcc = MUI_CreateCustomClass(NULL, MUIC_Area, NULL, sizeof(struct PaintData), (APTR)paint_dispatcher)))
        goto cleanup;

    app = ApplicationObject,
        MUIA_Application_Title, (ULONG)"Beispiel",
        MUIA_Application_Base, (ULONG)"BEISPIEL",
        SubWindow, win = WindowObject,
            MUIA_Window_Title, (ULONG)"Beispiel",
            MUIA_Window_ID, MAKE_ID('B','S','P','1'),
            WindowContents, VGroup,
                Child, paint = NewObject(mcc->mcc_Class, NULL, TextFrame, MUIA_Background, MUII_BACKGROUND, TAG_DONE),
            End,
        End,
    End;
    if (!app)
        goto cleanup;
    DoMethod(win, MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
             (ULONG)app, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);

    if (!(tport = CreateMsgPort()) ||
        !(treq = (struct timerequest *)CreateIORequest(tport, sizeof(struct timerequest))) ||
        OpenDevice(TIMERNAME, UNIT_VBLANK, (struct IORequest *)treq, 0) != 0)
        goto cleanup;
    timer_open = TRUE;
    treq->tr_node.io_Command = TR_ADDREQUEST;
    treq->tr_time.tv_secs = 1;
    treq->tr_time.tv_micro = 0;
    SendIO((struct IORequest *)treq);

    set(win, MUIA_Window_Open, TRUE);
    while (DoMethod(app, MUIM_Application_NewInput, (ULONG)&sigs) != MUIV_Application_ReturnID_Quit) {
        if (sigs) {
            sigs = Wait(sigs | (1UL << tport->mp_SigBit) | SIGBREAKF_CTRL_C);
            if (sigs & SIGBREAKF_CTRL_C)
                break;
            if (GetMsg(tport)) {              /* one second passed */
                struct PaintData *data = INST_DATA(mcc->mcc_Class, paint);
                data->count++;
                MUI_Redraw(paint, MADF_DRAWUPDATE);   /* calls MUIM_Draw */
                treq->tr_time.tv_secs = 1;
                treq->tr_time.tv_micro = 0;
                SendIO((struct IORequest *)treq);
            }
        }
    }
    set(win, MUIA_Window_Open, FALSE);
    rc = RETURN_OK;

cleanup:
    if (timer_open) {
        if (!CheckIO((struct IORequest *)treq))
            AbortIO((struct IORequest *)treq);
        WaitIO((struct IORequest *)treq);
        CloseDevice((struct IORequest *)treq);
    }
    if (treq) DeleteIORequest((struct IORequest *)treq);
    if (tport) DeleteMsgPort(tport);
    if (app) MUI_DisposeObject(app);          /* disposes all objects first ... */
    if (mcc) MUI_DeleteCustomClass(mcc);      /* ... then the class */
    if (MUIMasterBase) CloseLibrary(MUIMasterBase);
    if (GfxBase) CloseLibrary((struct Library *)GfxBase);
    if (IntuitionBase) CloseLibrary((struct Library *)IntuitionBase);
    return rc;
}
```

## Common mistakes
- MUIMasterBase declared static or not opened -> crash on the first MUI call.
- Dispatcher declared as a plain C function (without DISPATCHER_ARGS registers a0/a2/a1) -> crash as soon as an object of the class is created.
- Objects of an own class: `NewObject(mcc->mcc_Class, NULL, tags..., TAG_DONE)`. MUI_NewObject() only takes a class NAME.
- A main loop that does not call MUIM_Application_NewInput -> window never reacts, not even to the close gadget.
- Wrong number of `End` (too few OR too many) -> vbcc "error 76 identifier expected" / "; expected" at the last `End;`. Count: exactly one `End` per `...Object` / `VGroup` / `HGroup` you opened (ApplicationObject + WindowObject + VGroup = 3 Ends). Objects made with NewObject(...) or SimpleButton() need no End.
- Calling Intuition drawing functions on MUI windows: use MUI objects instead.
- Not disposing the application on error paths -> leaked windows and memory.
