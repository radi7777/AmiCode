/* Befehle in einem Hilfsprozess ausfuehren, damit CTRL-C und Zeitlimits greifen. */

#include <stdio.h>

#include <exec/types.h>
#include <exec/tasks.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/dostags.h>
#include <proto/exec.h>
#include <proto/dos.h>

#include "shell.h"
#include "ui.h"
#include "amiloc.h"

#define POLL_TICKS  10      /* 1/5 Sekunde */

typedef struct {
    const char *cmd;
    BPTR in, out;
    long stack;
    long rc;
    struct Task *parent;
    ULONG sigmask;
} CmdJob;

static CmdJob *cur_job;

static void cmd_entry(void)
{
    CmdJob *job = cur_job;

    job->rc = SystemTags((STRPTR)job->cmd,
                         SYS_Input, job->in,
                         SYS_Output, job->out,
                         NP_Error, job->out,
                         NP_CloseError, FALSE,
                         SYS_UserShell, TRUE,
                         NP_StackSize, job->stack,
                         TAG_DONE);
    /* Forbid bleibt bis zum Prozessende aktiv: der Elternprozess darf
       unseren Code erst entladen, wenn wir ihn nicht mehr ausfuehren. */
    Forbid();
    Signal(job->parent, job->sigmask);
}

/* Break an den Shell-Prozess senden, der unseren Befehl ausfuehrt */
static void send_break(CmdJob *job)
{
    LONG n, max;

    Forbid();
    max = MaxCli();
    for (n = 1; n <= max; n++) {
        struct Process *p = FindCliProc(n);
        if (p && (p->pr_CIS == job->in || p->pr_COS == job->out))
            Signal(&p->pr_Task, SIGBREAKF_CTRL_C);
    }
    Permit();
}

long shell_run(const char *cmd, BPTR in, BPTR out, long stack, long timeout, int *status)
{
    CmdJob job;
    BYTE sig;
    long ticks = 0;
    int breaks = 0;

    *status = SHELL_OK;
    if ((sig = AllocSignal(-1)) < 0)
        return -1;

    job.cmd = cmd;
    job.in = in;
    job.out = out;
    job.stack = stack;
    job.rc = -1;
    job.parent = FindTask(NULL);
    job.sigmask = 1UL << sig;
    cur_job = &job;
    SetSignal(0, job.sigmask | SIGBREAKF_CTRL_C);

    if (!CreateNewProcTags(NP_Entry, (ULONG)cmd_entry,
                           NP_Name, (ULONG)"AmiCode command",
                           NP_StackSize, 16384,
                           TAG_DONE)) {
        FreeSignal(sig);
        return -1;
    }

    while (!(SetSignal(0, 0) & job.sigmask)) {
        Delay(POLL_TICKS);
        ticks += POLL_TICKS;
        if (SetSignal(0, SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C) {
            *status = SHELL_BREAK;
            send_break(&job);
            ui_printf(UI_DETAIL, "%s", GetStr(breaks++ ? MSG_SHELL_NO_BREAK : MSG_SHELL_BREAK_SENT));
        }
        if (timeout > 0 && ticks >= timeout * 50 && ticks - POLL_TICKS < timeout * 50) {
            *status = SHELL_TIMEOUT;
            send_break(&job);
            ui_printf(UI_DETAIL, GetStr(MSG_SHELL_TIMEOUT), timeout);
        }
    }
    SetSignal(0, job.sigmask);
    FreeSignal(sig);
    return job.rc;
}
