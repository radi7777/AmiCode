/* Agent-Prozess fuer die MUI-Oberflaeche */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/dostags.h>
#include <proto/exec.h>
#include <proto/dos.h>

#include "gui_agent.h"
#include "agent.h"
#include "net.h"
#include "tools.h"
#include "ui.h"
#include "provider.h"
#include "toolchain.h"
#include "amiloc.h"

enum { AC_START, AC_PROMPT, AC_RESET, AC_RESUME, AC_MODE, AC_SHELL, AC_UNDO, AC_DIFF, AC_MODELS,
       AC_CHOICES, AC_NEWPROJECT, AC_SESSIONS, AC_CMDLINE, AC_QUIT };

typedef struct {
    struct Message msg;
    LONG cmd;
    LONG value;
    struct MsgPort *port;       /* AC_START: Befehlsport des Agenten (Antwort) */
    char text[1];
} AgentCmd;

/* Daten fuer den Prozessstart, nur waehrend gui_agent_start gueltig */
static const Config *start_cfg;
static int start_mode;
static const char *start_version = "?";

static struct MsgPort *events_port;     /* GUI: empfaengt GuiEvents */
static struct MsgPort *reply_port;      /* GUI: Antworten auf AgentCmds */
static struct MsgPort *agent_port;      /* Agent: empfaengt AgentCmds */
static struct Process *agent_proc;
static int agent_max_steps;

/* ---------- im Agent-Prozess ---------- */

static struct MsgPort *ask_port;

static void send_event(LONG kind, const char *text, int wait)
{
    ULONG len = strlen(text);
    GuiEvent *ev = AllocVec(sizeof(GuiEvent) + len, MEMF_PUBLIC | MEMF_CLEAR);

    if (!ev)
        return;
    ev->msg.mn_Length = sizeof(GuiEvent) + len;
    ev->msg.mn_ReplyPort = wait ? ask_port : NULL;
    ev->kind = kind;
    memcpy(ev->text, text, len + 1);
    PutMsg(events_port, &ev->msg);
    if (wait) {
        WaitPort(ask_port);
        GetMsg(ask_port);
    }
}

static void gui_print(int kind, const char *text)
{
    send_event(kind, text, 0);
}

static int gui_ask(const char *question)
{
    ULONG len = strlen(question);
    GuiEvent *ev = AllocVec(sizeof(GuiEvent) + len, MEMF_PUBLIC | MEMF_CLEAR);
    int answer;

    if (!ev)
        return ASK_NO;
    ev->msg.mn_Length = sizeof(GuiEvent) + len;
    ev->msg.mn_ReplyPort = ask_port;
    ev->kind = EV_ASK;
    memcpy(ev->text, question, len + 1);
    PutMsg(events_port, &ev->msg);
    WaitPort(ask_port);
    GetMsg(ask_port);
    answer = ev->answer;
    FreeVec(ev);
    return answer;
}

static const UiHooks gui_hooks = { gui_print, gui_ask };

/* Verzeichnis der 1>-Zeile; 0 = Projektverzeichnis. Nur im Agent-Prozess benutzt. */
static BPTR shell_dir;

static void report_cwd(BPTR lock)
{
    char name[512];

    if (NameFromLock(lock, name, sizeof(name)))
        send_event(UI_CWD, name, 0);
}

/* Befehl aus der 1>-Zeile: CD merkt sich das Verzeichnis fuer die naechsten Befehle,
   alles andere laeuft dort. Der Agent selbst arbeitet weiter im Projektverzeichnis. */
static void cmdline(ToolCtx *tools, char *cmd)
{
    char *arg, *end;
    BPTR prev, lock;

    while (*cmd == ' ')
        cmd++;
    if (strnicmp(cmd, "cd", 2) != 0 || (cmd[2] && cmd[2] != ' ')) {
        StrBuf out;
        sb_init(&out);
        prev = shell_dir ? CurrentDir(shell_dir) : 0;
        tools_exec(tools, cmd, &out);
        if (shell_dir)
            CurrentDir(prev);
        sb_free(&out);
        return;
    }
    for (arg = cmd + 2; *arg == ' '; arg++)
        ;
    end = arg + strlen(arg);
    while (end > arg && end[-1] == ' ')
        *--end = 0;
    if (*arg == '"' && end > arg + 1 && end[-1] == '"') {
        end[-1] = 0;
        arg++;
    }
    if (!*arg) {
        /* CD ohne Argument: Verzeichnis anzeigen */
        char name[512];
        if (NameFromLock(shell_dir ? shell_dir : tools->root_lock, name, sizeof(name)))
            send_event(UI_OUTPUT, name, 0);
        return;
    }
    prev = shell_dir ? CurrentDir(shell_dir) : 0;
    lock = Lock(arg, SHARED_LOCK);
    if (shell_dir)
        CurrentDir(prev);
    if (lock) {
        struct FileInfoBlock *fib = AllocDosObject(DOS_FIB, NULL);
        int isdir = fib && Examine(lock, fib) && fib->fib_DirEntryType > 0;
        if (fib)
            FreeDosObject(DOS_FIB, fib);
        if (!isdir) {
            UnLock(lock);
            lock = 0;
        }
    }
    if (!lock) {
        char msg[300];
        snprintf(msg, sizeof(msg), GetStr(MSG_GA_CD_FAIL), arg);
        send_event(UI_OUTPUT, msg, 0);
        return;
    }
    if (shell_dir)
        UnLock(shell_dir);
    shell_dir = lock;
    report_cwd(lock);
}

static void agent_entry(void)
{
    struct Process *me = (struct Process *)FindTask(NULL);
    AgentCmd *start, *cmd, *quit = NULL;
    const Config *cfg;
    struct MsgPort *port;
    ToolCtx tools;
    Agent ag;
    char err[256];
    int net_ok, ready;

    WaitPort(&me->pr_MsgPort);
    start = (AgentCmd *)GetMsg(&me->pr_MsgPort);
    cfg = start_cfg;

    port = CreateMsgPort();
    ask_port = CreateMsgPort();
    ready = port && ask_port && tools_init(&tools, cfg, start_mode);
    if (ready && !agent_init(&ag, cfg, &tools)) {
        tools_cleanup(&tools);
        ready = 0;
    }
    if (!ready) {
        if (port) DeleteMsgPort(port);
        if (ask_port) DeleteMsgPort(ask_port);
        start->port = NULL;
        Forbid();
        ReplyMsg(&start->msg);
        return;
    }
    ag.version = start_version;
    start->port = port;
    ReplyMsg(&start->msg);

    /* bsdsocket und AmiSSL gelten pro Prozess: hier oeffnen, nicht in der GUI */
    net_ok = net_open(err, sizeof(err));
    if (!net_ok)
        ui_printf(UI_ERROR, GetStr(MSG_GA_NETWORK), err);
    report_cwd(tools.root_lock);
    send_event(EV_IDLE, "", 0);

    while (!quit) {
        WaitPort(port);
        while (!quit && (cmd = (AgentCmd *)GetMsg(port))) {
            char *text = NULL;
            LONG c = cmd->cmd, value = cmd->value;

            if (c == AC_QUIT) {
                quit = cmd;         /* Antwort erst nach dem Aufraeumen */
                break;
            }
            if (c == AC_PROMPT || c == AC_SHELL || c == AC_MODELS || c == AC_NEWPROJECT ||
                c == AC_SESSIONS || c == AC_CMDLINE)
                text = strdup(cmd->text);
            ReplyMsg(&cmd->msg);

            switch (c) {
            case AC_PROMPT:
                if (text && !agent_command(&ag, text)) {
                    if (!net_ok)
                        ui_printf(UI_ERROR, "%s", GetStr(MSG_GA_NO_NET_ABOVE));
                    else
                        agent_ask(&ag, text, agent_max_steps);
                }
                break;
            case AC_RESET:
                agent_reset(&ag);
                ui_printf(UI_INFO, "%s", GetStr(MSG_NEW_SESSION));
                break;
            case AC_RESUME: {
                int n = agent_resume(&ag);
                if (n < 0)
                    ui_printf(UI_ERROR, "%s", GetStr(MSG_SESSION_NOT_FOUND));
                else
                    ui_printf(UI_INFO, GetStr(MSG_GA_SESSION_LOADED), n);
                break;
            }
            case AC_SHELL:
                if (text) {
                    StrBuf out;
                    sb_init(&out);
                    tools_exec(&tools, text, &out);
                    sb_free(&out);
                }
                break;
            case AC_UNDO:
            case AC_DIFF: {
                StrBuf out;
                sb_init(&out);
                if (c == AC_UNDO)
                    tools_undo(&tools, &out);
                else
                    tools_diff(&tools, &out);
                if (out.buf)
                    ui->print(UI_TEXT, out.buf);
                sb_free(&out);
                break;
            }
            case AC_MODELS:
                /* text: "anbieter\nbasis-url\nkey" (fuers Einstellungsfenster, noch nicht gespeichert) */
                if (text) {
                    char *base = strchr(text, '\n'), *key = NULL, err[200];
                    StrBuf out;
                    int n;
                    if (base) {
                        *base++ = 0;
                        if ((key = strchr(base, '\n')))
                            *key++ = 0;
                    }
                    sb_init(&out);
                    sb_add(&out, "prefs\n");
                    if (!net_ok)
                        ui_printf(UI_ERROR, "%s", GetStr(MSG_GA_NO_NET));
                    else if ((n = provider_models(start_cfg, text, base, key ? key : "", &out,
                                                  err, sizeof(err))) < 0)
                        ui_printf(UI_ERROR, GetStr(MSG_GA_MODELS_ERR), err);
                    else {
                        ui->print(UI_MODELS, out.buf);
                        ui_printf(UI_INFO, GetStr(MSG_GA_MODELS_LOADED), n);
                    }
                    sb_free(&out);
                }
                break;
            case AC_CHOICES: {
                StrBuf out;
                sb_init(&out);
                toolchains_choices(&out);
                if (out.buf)
                    ui->print(UI_CHOICES, out.buf);
                sb_free(&out);
                break;
            }
            case AC_NEWPROJECT:
                /* text: id \n verzeichnis \n programmart \n cpu \n os \n beschreibung (Rest) */
                if (text) {
                    char *f[6];
                    int k;
                    ProjectOptions opt;
                    StrBuf out;

                    f[0] = text;
                    for (k = 1; k < 6; k++) {
                        char *nl = f[k - 1] ? strchr(f[k - 1], '\n') : NULL;
                        if (nl)
                            *nl = 0;
                        f[k] = nl ? nl + 1 : NULL;
                    }
                    opt.kind = f[2];
                    opt.cpu = f[3];
                    opt.os = f[4];
                    opt.description = f[5];
                    sb_init(&out);
                    if (f[1] && toolchains_new_project2(f[0], f[1], &opt, &out)) {
                        if (out.buf)
                            ui->print(UI_TEXT, out.buf);
                        ui->print(UI_PROJECT, f[1]);
                    } else if (out.buf) {
                        ui_printf(UI_ERROR, "%s", out.buf);
                    }
                    sb_free(&out);
                }
                break;
            case AC_CMDLINE:
                if (text)
                    cmdline(&tools, text);
                break;
            case AC_SESSIONS:
                /* text: "" = nur Liste, sonst Name der zu loeschenden Sitzung */
                if (text && *text)
                    agent_delete_session(&ag, text);
                agent_list_sessions(&ag, 0);
                break;
            case AC_MODE:
                tools.mode = value;
                ui_printf(UI_INFO, GetStr(MSG_GA_MODE), mode_name(value));
                break;
            }
            free(text);
            send_event(EV_IDLE, "", 0);
        }
    }

    if (shell_dir) {
        UnLock(shell_dir);
        shell_dir = 0;
    }
    agent_free(&ag);
    tools_cleanup(&tools);
    net_close();
    DeleteMsgPort(ask_port);
    DeleteMsgPort(port);
    /* Forbid bis zum Prozessende: die GUI darf erst danach beenden */
    Forbid();
    ReplyMsg(&quit->msg);
}

/* ---------- im GUI-Prozess ---------- */

static AgentCmd *new_cmd(LONG c, const char *text)
{
    ULONG len = text ? strlen(text) : 0;
    AgentCmd *cmd = AllocVec(sizeof(AgentCmd) + len, MEMF_PUBLIC | MEMF_CLEAR);

    if (cmd) {
        cmd->msg.mn_Length = sizeof(AgentCmd) + len;
        cmd->msg.mn_ReplyPort = reply_port;
        cmd->cmd = c;
        if (text)
            memcpy(cmd->text, text, len + 1);
    }
    return cmd;
}

static void send_cmd(LONG c, const char *text, LONG value)
{
    AgentCmd *cmd;

    if (!agent_port || !(cmd = new_cmd(c, text)))
        return;
    cmd->value = value;
    PutMsg(agent_port, &cmd->msg);
}

void gui_agent_version(const char *version)
{
    start_version = version;
}

int gui_agent_start(const Config *cfg, BPTR dir, int mode, struct MsgPort *events)
{
    AgentCmd *start;
    BPTR lock;

    events_port = events;
    agent_max_steps = atoi(config_get(cfg, "agent.max_steps", "30"));
    if (agent_max_steps < 1)
        agent_max_steps = 1;
    if (!reply_port && !(reply_port = CreateMsgPort()))
        return 0;
    if (!(start = new_cmd(AC_START, NULL)))
        return 0;
    if (!(lock = DupLock(dir))) {
        FreeVec(start);
        return 0;
    }
    ui = &gui_hooks;
    start_cfg = cfg;
    start_mode = mode;

    agent_proc = CreateNewProcTags(NP_Entry, (ULONG)agent_entry,
                                   NP_Name, (ULONG)"AmiCode Agent",
                                   NP_StackSize, 65536,
                                   NP_CurrentDir, lock,
                                   TAG_DONE);
    if (!agent_proc) {
        UnLock(lock);
        FreeVec(start);
        return 0;
    }
    PutMsg(&agent_proc->pr_MsgPort, &start->msg);
    WaitPort(reply_port);
    GetMsg(reply_port);
    agent_port = start->port;
    FreeVec(start);
    if (!agent_port) {
        agent_proc = NULL;
        return 0;
    }
    return 1;
}

void gui_agent_prompt(const char *text)  { send_cmd(AC_PROMPT, text, 0); }
void gui_agent_reset(void)               { send_cmd(AC_RESET, NULL, 0); }
void gui_agent_resume(void)              { send_cmd(AC_RESUME, NULL, 0); }
void gui_agent_mode(int mode)            { send_cmd(AC_MODE, NULL, mode); }
void gui_agent_shell(const char *cmd)    { send_cmd(AC_SHELL, cmd, 0); }
void gui_agent_undo(void)                { send_cmd(AC_UNDO, NULL, 0); }
void gui_agent_diff(void)                { send_cmd(AC_DIFF, NULL, 0); }
void gui_agent_models(const char *spec)  { send_cmd(AC_MODELS, spec, 0); }
void gui_agent_choices(void)             { send_cmd(AC_CHOICES, NULL, 0); }
void gui_agent_newproject(const char *spec) { send_cmd(AC_NEWPROJECT, spec, 0); }
void gui_agent_sessions(const char *del)    { send_cmd(AC_SESSIONS, del ? del : "", 0); }
void gui_agent_cmdline(const char *cmd)     { send_cmd(AC_CMDLINE, cmd, 0); }

void gui_agent_break(void)
{
    if (agent_proc)
        Signal(&agent_proc->pr_Task, SIGBREAKF_CTRL_C);
}

void gui_agent_collect(void)
{
    struct Message *m;

    if (!reply_port)
        return;
    while ((m = GetMsg(reply_port)))
        FreeVec(m);
}

ULONG gui_agent_replysig(void)
{
    return reply_port ? 1UL << reply_port->mp_SigBit : 0;
}

void gui_agent_stop(void)
{
    AgentCmd *quit;
    int done = 0;

    if (agent_port && (quit = new_cmd(AC_QUIT, NULL))) {
        gui_agent_break();
        PutMsg(agent_port, &quit->msg);
        while (!done) {
            struct Message *m;
            GuiEvent *ev;

            Wait((1UL << reply_port->mp_SigBit) | (1UL << events_port->mp_SigBit));
            while ((ev = (GuiEvent *)GetMsg(events_port))) {
                if (ev->msg.mn_ReplyPort) {
                    ev->answer = ASK_NO;
                    ReplyMsg(&ev->msg);
                } else {
                    FreeVec(ev);
                }
            }
            while ((m = GetMsg(reply_port))) {
                if (m == &quit->msg)
                    done = 1;
                FreeVec(m);
            }
        }
    }
    agent_port = NULL;
    agent_proc = NULL;
    if (reply_port) {
        gui_agent_collect();
        DeleteMsgPort(reply_port);
        reply_port = NULL;
    }
}
