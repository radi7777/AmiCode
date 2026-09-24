/* Ausgabe-Hooks des Agent-Kerns plus die Shell-Fassung fuer die CLI */

#include <stdio.h>
#include <stdarg.h>

#include <exec/types.h>
#include <dos/dos.h>
#include <proto/dos.h>

#include "ui.h"
#include "amiloc.h"

static void cli_print(int kind, const char *text)
{
    static int streaming;

    /* gestreamter Text beginnt in einer neuen Zeile, wie sonst UI_TEXT */
    if (kind == UI_STREAM) {
        if (!streaming)
            printf("\n");
        streaming = 1;
        printf("%s", text);
        fflush(stdout);
        return;
    }
    streaming = 0;
    switch (kind) {
    case UI_STEP:   printf("\n%s\n", text); break;
    case UI_TEXT:   printf("\n%s\n", text); break;
    case UI_TOOL:   printf("  > %s\n", text); break;
    case UI_DETAIL: printf("    %s\n", text); break;
    case UI_ERROR:  printf(GetStr(MSG_CLI_ERROR), text); break;
    case UI_BUSY:   break;
    case UI_OUTPUT: break;      /* geht in der CLI nur an das Modell */
    case UI_PROJECT: printf(GetStr(MSG_CLI_SWITCH_DIR), text); break;
    case UI_MODELS: break;
    case UI_TOKENS: break;      /* nur fuer die Statuszeile der GUI */
    case UI_SESSIONS: break;
    case UI_CWD:    break;    /* die CLI hat die Liste schon als Text bekommen */
    case UI_PROMPT: printf("\n> %s\n", text); break;
    case UI_CHOICES: break;
    case UI_DONE:   printf("\n%s\n", text); break;
    default:        printf("%s\n", text); break;
    }
}

static int cli_ask(const char *question)
{
    char line[16];
    BPTR in = Input();

    printf(GetStr(MSG_CLI_ASK), question);
    fflush(stdout);
    if (!in || !IsInteractive(in) || !FGets(in, line, sizeof(line))) {
        printf("%s\n", GetStr(MSG_CLI_NO_INPUT));
        return ASK_NO;
    }
    /* englische und deutsche Buchstaben: y/j = ja, a/i = immer */
    if (line[0] == 'a' || line[0] == 'A' || line[0] == 'i' || line[0] == 'I')
        return ASK_ALWAYS;
    if (line[0] == 'q' || line[0] == 'Q')
        return ASK_ABORT;
    if (line[0] == 'j' || line[0] == 'J' || line[0] == 'y' || line[0] == 'Y')
        return ASK_YES;
    return ASK_NO;
}

static const UiHooks cli_hooks = { cli_print, cli_ask };

const UiHooks *ui = &cli_hooks;

void ui_printf(int kind, const char *fmt, ...)
{
    char buf[512];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    ui->print(kind, buf);
}

int ui_ask(const char *question)
{
    return ui->ask(question);
}
