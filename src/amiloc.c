/* Sprachumschaltung - siehe amiloc.h.

   Bewusst anspruchslos: ein OpenCatalog beim Start, ein CloseCatalog am
   Ende, dazwischen nur Zeigerarithmetik. Der Katalog wird nur gelesen,
   darum darf auch der Agent-Prozess der IDE GetStr() aufrufen. */

#include <string.h>

#include <exec/types.h>
#include <libraries/locale.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/locale.h>

#define AMILOC_TABLE
#include "amiloc.h"

struct LocaleBase *LocaleBase = NULL;
static struct Catalog *g_catalog = NULL;

void locale_open(void)
{
    char forced[32];

    if (LocaleBase)
        return;
    if (!(LocaleBase = (struct LocaleBase *)OpenLibrary("locale.library", 38)))
        return;

    /* SetEnv AmiCodeLanguage schlaegt die Systemsprache - fuer alle, die ein
       englisches Workbench fahren, AmiCode aber in ihrer Sprache wollen,
       und zum Pruefen der Kataloge. */
    if (GetVar((STRPTR)"AmiCodeLanguage", (STRPTR)forced, sizeof(forced) - 1, GVF_GLOBAL_ONLY) > 0) {
        if (stricmp(forced, "english") == 0)
            return;
        g_catalog = OpenCatalog(NULL, (STRPTR)"AmiCode.catalog",
                                OC_BuiltInLanguage, (ULONG)"english",
                                OC_Language, (ULONG)forced,
                                TAG_DONE);
    }
    if (!g_catalog)
        g_catalog = OpenCatalog(NULL, (STRPTR)"AmiCode.catalog",
                                OC_BuiltInLanguage, (ULONG)"english",
                                TAG_DONE);
}

void locale_close(void)
{
    if (!LocaleBase)
        return;
    if (g_catalog) {
        CloseCatalog(g_catalog);
        g_catalog = NULL;
    }
    CloseLibrary((struct Library *)LocaleBase);
    LocaleBase = NULL;
}

const char *GetStr(long id)
{
    const char *builtin = AMILOC_BUILTIN[id];

    if (g_catalog)
        return (const char *)GetCatalogStr(g_catalog, id, (STRPTR)builtin);
    return builtin;
}

const char *locale_language(void)
{
    return g_catalog ? (const char *)g_catalog->cat_Language : NULL;
}
