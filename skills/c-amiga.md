# C on AmigaOS 3.x
Description: Writing C for AmigaOS 3.x on 68k (libraries, types, files, windows, signals, compilers). Read before writing or changing Amiga C code.

## Basics
- Big-endian 68k, 32-bit int and pointers. 68000/010 fault on word/long access at odd addresses; keep structs naturally aligned.
- Types from <exec/types.h>: BYTE/UBYTE, WORD/UWORD, LONG/ULONG, BOOL (TRUE/FALSE), STRPTR, APTR, BPTR (DOS pointer, never dereference).
- Include `<proto/xxx.h>` for each library you call (proto/exec.h, proto/dos.h, proto/intuition.h, proto/graphics.h). Structures come from <exec/...>, <dos/...>, <intuition/...>.
- Exit codes: RETURN_OK 0, RETURN_WARN 5, RETURN_ERROR 10, RETURN_FAIL 20.
- Version string for the `Version` command: `static const char ver[] = "$VER: Name 1.0 (23.9.2026)";`
- No fork, no pthreads, no mmap, no POSIX signals, no sockets without bsdsocket.library. stdio (printf, fopen) works.

## Libraries
- exec and dos are opened by the startup code. Every other library you open yourself:
```c
struct IntuitionBase *IntuitionBase;   /* global, NOT static: the protos use this name */
struct GfxBase *GfxBase;
IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 39);
if (!IntuitionBase) { PutStr("intuition.library fehlt\n"); return RETURN_FAIL; }
...
CloseLibrary((struct Library *)IntuitionBase);
```
- Version 39 = OS 3.0, 40 = 3.1, 47 = 3.2. Close everything you opened, in reverse order, on every exit path.
- Functions ending in `Tags` (OpenWindowTags, AllocDosObjectTags...) take a tag list ending with TAG_DONE and need amiga.lib (`-lamiga` with vbcc) in some compilers.

## Memory
- `AllocVec(size, MEMF_ANY | MEMF_CLEAR)` / `FreeVec(p)`. Chip memory (MEMF_CHIP) only for data the custom chips read (sprites, audio, planar bitmaps). malloc/free also work.
- There is no memory protection: a stray pointer crashes the whole machine. Check every allocation.

## Files (dos.library)
```c
BPTR fh = Open("T:test.txt", MODE_NEWFILE);   /* MODE_OLDFILE = read, MODE_READWRITE */
if (fh) { FPuts(fh, "Hallo\n"); Close(fh); }
BPTR lock = Lock("Work:", SHARED_LOCK);        /* UnLock(lock) when done */
```
- Directory scan: `AllocDosObject(DOS_FIB, NULL)`, `Examine(lock, fib)`, then `while (ExNext(lock, fib))` - fib_FileName, fib_DirEntryType > 0 means directory.
- Output to the shell: `PutStr("text\n")` or `Printf("%ld\n", value)` (dos Printf wants LONG for %ld; use %s for strings) or stdio printf.
- Wait: `Delay(50)` = 1 second (ticks of 1/50 s).
- Current time: `struct DateStamp ds; DateStamp(&ds);` It has ONLY three fields: `ds_Days` (days since 1.1.1978), `ds_Minute` (minutes since midnight), `ds_Tick` (1/50 s ticks within the current minute). There is no ds_Hour/ds_Second/ds_Year. Clock time: `hour = ds.ds_Minute / 60; min = ds.ds_Minute % 60; sec = ds.ds_Tick / TICKS_PER_SECOND;` The system clock is local time. Date as text: `DateToStr()` with `struct DateTime` (dos/datetime.h).

## Signals, CTRL-C, events
- Check for CTRL-C in long loops: `if (SetSignal(0, SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C) break;`
- Never busy-wait and never DoIO() a timer inside an event loop: send requests with SendIO() and Wait() on all signal bits at once.
- Drawing: SetAPen(rp, pen) selects the colour (pen 0 background, 1 text/black, 2 white, 3 blue). SetRast(rp, pen) CLEARS THE WHOLE window - do not use it to pick a colour. Coordinates start at win->BorderLeft / win->BorderTop.
- sin()/cos() from <math.h> need a math library: with vbcc add `-lmieee` before `-lamiga` in the vc line of the build script (tested: `vc +aos68k -O1 -lmieee -lamiga -o prog prog.c`). M_PI is not defined by vbcc: `#ifndef M_PI` / `#define M_PI 3.14159265358979` / `#endif`.

## Complete example: window + 1 second timer + close gadget (tested, copy this structure)
```c
#include <stdio.h>
#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <intuition/intuition.h>
#include <devices/timer.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/graphics.h>

struct IntuitionBase *IntuitionBase;   /* global, not static */
struct GfxBase *GfxBase;

static void draw(struct Window *win, LONG count)
{
    struct RastPort *rp = win->RPort;
    char buf[32];
    int x0 = win->BorderLeft, y0 = win->BorderTop;
    int w = win->Width - win->BorderLeft - win->BorderRight;
    int h = win->Height - win->BorderTop - win->BorderBottom;
    int len = sprintf(buf, "Sekunden: %ld", (long)count);

    SetAPen(rp, 0);                         /* background */
    RectFill(rp, x0, y0, x0 + w - 1, y0 + h - 1);
    SetAPen(rp, 1);                         /* pen 1 = black/text */
    Move(rp, x0 + 10, y0 + 20);
    Text(rp, buf, len);
    SetAPen(rp, 2);
    Move(rp, x0 + w / 2, y0 + h / 2);       /* a line from the centre */
    Draw(rp, x0 + w / 2 + (count % 20) * 3, y0 + 10);
}

int main(void)
{
    struct Window *win = NULL;
    struct MsgPort *tport = NULL;
    struct timerequest *treq = NULL;
    BOOL timer_open = FALSE, done = FALSE;
    LONG count = 0;
    int rc = RETURN_FAIL;

    IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 39);
    GfxBase = (struct GfxBase *)OpenLibrary("graphics.library", 39);
    if (!IntuitionBase || !GfxBase)
        goto cleanup;

    win = OpenWindowTags(NULL,
        WA_Title, (ULONG)"Beispiel",
        WA_InnerWidth, 200, WA_InnerHeight, 120,
        WA_IDCMP, IDCMP_CLOSEWINDOW,
        WA_Flags, WFLG_CLOSEGADGET | WFLG_DRAGBAR | WFLG_DEPTHGADGET | WFLG_ACTIVATE,
        TAG_DONE);
    if (!win)
        goto cleanup;

    /* timer.device: one request per second */
    if (!(tport = CreateMsgPort()))
        goto cleanup;
    if (!(treq = (struct timerequest *)CreateIORequest(tport, sizeof(struct timerequest))))
        goto cleanup;
    if (OpenDevice(TIMERNAME, UNIT_VBLANK, (struct IORequest *)treq, 0) != 0)
        goto cleanup;
    timer_open = TRUE;

    draw(win, count);
    treq->tr_node.io_Command = TR_ADDREQUEST;
    treq->tr_time.tv_secs = 1;
    treq->tr_time.tv_micro = 0;
    SendIO((struct IORequest *)treq);       /* asynchronous, never DoIO in an event loop */

    while (!done) {
        ULONG winsig = 1UL << win->UserPort->mp_SigBit;
        ULONG timsig = 1UL << tport->mp_SigBit;
        ULONG sigs = Wait(winsig | timsig | SIGBREAKF_CTRL_C);

        if (sigs & SIGBREAKF_CTRL_C)
            done = TRUE;
        if (sigs & winsig) {
            struct IntuiMessage *im;
            while ((im = (struct IntuiMessage *)GetMsg(win->UserPort))) {
                ULONG class = im->Class;
                ReplyMsg((struct Message *)im);     /* reply every message */
                if (class == IDCMP_CLOSEWINDOW)
                    done = TRUE;
            }
        }
        if ((sigs & timsig) && GetMsg(tport)) {   /* timer request came back */
            draw(win, ++count);
            treq->tr_time.tv_secs = 1;             /* send it again */
            treq->tr_time.tv_micro = 0;
            SendIO((struct IORequest *)treq);
        }
    }
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
    if (win) CloseWindow(win);
    if (GfxBase) CloseLibrary((struct Library *)GfxBase);
    if (IntuitionBase) CloseLibrary((struct Library *)IntuitionBase);
    return rc;
}
```

## Compilers
- VBCC: `vc +aos68k -O1 -lamiga -o prog prog.c` (the project's build script sets assigns). C99 mostly supported.
- SAS/C 6: C89 only, declare variables at the start of blocks. gcc 2.95 (ADE): `-noixemul`, mostly C89.
- If a function is "undefined" at link time, it is usually in amiga.lib (-lamiga) or its library base is missing/static.
