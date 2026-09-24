/* AmiCode - Coding-Agent fuer AmigaOS 3.2 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <exec/types.h>
#include <dos/dos.h>
#include <dos/rdargs.h>
#include <proto/exec.h>
#include <proto/dos.h>

#include "config.h"
#include "net.h"
#include "tools.h"
#include "agent.h"
#include "provider.h"
#include "amiloc.h"

#define VERSION_STRING "0.30"

static const char vers[] __attribute__((used)) = "$VER: AmiCode " VERSION_STRING " (24.09.2026)";

/* OpenSSL braucht deutlich mehr als die Shell-Vorgabe von 4 KB; libnix tauscht den Stack beim Start */
unsigned long __stack = 65536;

#define TEMPLATE "MODE/K,STEPS/K/N,CONFIG/K,RESUME/S,PROMPT/F"
enum { ARG_MODE, ARG_STEPS, ARG_CONFIG, ARG_RESUME, ARG_PROMPT, ARG_COUNT };

#define CONTINUE_PROMPT GetStr(MSG_CONTINUE_PROMPT)

static void usage(void)
{
    printf(GetStr(MSG_CLI_USAGE), VERSION_STRING);
}

/* Interaktive Schleife: Auftraege nacheinander in derselben Sitzung */
static void repl(Agent *ag, int max_steps)
{
    char line[1024];
    BPTR in = Input();

    printf("%s\n", GetStr(MSG_CLI_INTERACTIVE));
    for (;;) {
        char *s, *e;

        printf("\n%s> ", GetStr(MSG_CLI_PROMPT));
        fflush(stdout);
        SetSignal(0, SIGBREAKF_CTRL_C);
        if (!FGets(in, line, sizeof(line)))
            break;
        for (s = line; *s == ' ' || *s == '\t'; s++)
            ;
        e = s + strlen(s);
        while (e > s && (e[-1] == '\n' || e[-1] == '\r' || e[-1] == ' '))
            *--e = 0;
        if (!*s)
            continue;
        if (stricmp(s, "/exit") == 0 || stricmp(s, "/quit") == 0)
            break;
        if (!agent_command(ag, s))
            agent_ask(ag, s, max_steps);
    }
}

int main(void)
{
    LONG args[ARG_COUNT] = { 0 };
    struct RDArgs *rda;
    Config cfg;
    ToolCtx tools;
    Agent agent;
    const char *cfgpath, *modestr, *prompt;
    char err[256];
    int mode, max_steps, rc = RETURN_FAIL, interactive;

    (void)vers;
    locale_open();
    if (!(rda = ReadArgs(TEMPLATE, args, NULL))) {
        PrintFault(IoErr(), "AmiCode");
        locale_close();
        return RETURN_FAIL;
    }
    prompt = (const char *)args[ARG_PROMPT];
    interactive = !prompt && Input() && IsInteractive(Input());
    if (!prompt && !interactive && !args[ARG_RESUME]) {
        usage();
        FreeArgs(rda);
        locale_close();
        return RETURN_WARN;
    }

    cfgpath = args[ARG_CONFIG] ? (const char *)args[ARG_CONFIG] : CONFIG_DEFAULT_PATH;
    if (!config_load(&cfg, cfgpath) &&
        (args[ARG_CONFIG] || !config_load(&cfg, "ENVARC:AmiCode/amicode.conf"))) {
        printf(GetStr(MSG_CLI_NO_CONFIG), cfgpath);
        FreeArgs(rda);
        locale_close();
        return RETURN_FAIL;
    }

    modestr = args[ARG_MODE] ? (const char *)args[ARG_MODE] : config_get(&cfg, "agent.mode", "project");
    if ((mode = mode_parse(modestr)) < 0) {
        printf(GetStr(MSG_CLI_BAD_MODE), modestr);
        goto out_cfg;
    }
    max_steps = args[ARG_STEPS] ? *(LONG *)args[ARG_STEPS] : atoi(config_get(&cfg, "agent.max_steps", "30"));
    if (max_steps < 1)
        max_steps = 1;

    if (!tools_init(&tools, &cfg, mode)) {
        printf("%s\n", GetStr(MSG_CLI_NO_CWD));
        goto out_cfg;
    }
    if (!net_open(err, sizeof(err))) {
        printf(GetStr(MSG_NETWORK_ERR), err);
        goto out_tools;
    }

    {
        ProviderSettings ps;
        provider_settings(&cfg, NULL, &ps);
        printf(GetStr(MSG_CLI_BANNER), VERSION_STRING,
               ps.def->name, *ps.model ? ps.model : GetStr(MSG_NO_MODEL),
               mode_name(mode), max_steps, tools.root);
    }

    if (!agent_init(&agent, &cfg, &tools)) {
        printf("%s\n", GetStr(MSG_NO_MEMORY));
        goto out_net;
    }
    agent.version = VERSION_STRING;
    if (args[ARG_RESUME]) {
        int n = agent_resume(&agent);
        if (n < 0) {
            printf(GetStr(MSG_CLI_NO_SESSION), agent.session);
            goto out_agent;
        }
        agent_show_history(&agent);
        printf(GetStr(MSG_CLI_RESUMED), n);
        if (!prompt && !interactive)
            prompt = CONTINUE_PROMPT;
    }

    rc = RETURN_OK;
    if (prompt && !agent_command(&agent, prompt) && !agent_ask(&agent, prompt, max_steps))
        rc = RETURN_WARN;
    if (interactive)
        repl(&agent, max_steps);

out_agent:
    agent_free(&agent);
out_net:
    net_close();
out_tools:
    tools_cleanup(&tools);
out_cfg:
    config_free(&cfg);
    FreeArgs(rda);
    locale_close();
    return rc;
}
