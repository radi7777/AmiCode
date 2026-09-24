#ifndef AMICODE_AMILOC_H
#define AMICODE_AMILOC_H

/* Sprachumschaltung ueber locale.library.

   Die eingebauten Texte sind ENGLISCH. Uebersetzungen kommen aus
   Catalogs/<sprache>/AmiCode.catalog (neben dem Programm oder in LOCALE:).
   Passt kein Katalog, bleibt alles englisch - ohne Fehlermeldung.

   "SetEnv AmiCodeLanguage deutsch" erzwingt eine Sprache unabhaengig von
   den Locale-Voreinstellungen, "SetEnv AmiCodeLanguage english" die
   eingebauten Texte.

   Die MSG_-Nummern stehen in build/locale_strings.h, das tools/locale.py
   aus catalogs/AmiCode.cd erzeugt. Nie von Hand aendern. */

#include "locale_strings.h"

void        locale_open(void);
void        locale_close(void);
const char *GetStr(long id);
/* Sprache des geoeffneten Katalogs ("deutsch") oder NULL = eingebautes Englisch */
const char *locale_language(void);

#endif
