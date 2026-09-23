#ifndef AMICODE_WEB_H
#define AMICODE_WEB_H

#include "json.h"

/* Websuche ueber DuckDuckGo (ohne API-Key). query in UTF-8.
   out erhaelt die Treffer als ISO-8859-1-Text. Liefert Anzahl oder -1 (err). */
int web_search(const char *query, StrBuf *out, char *err, int errlen);

/* Seite laden und als lesbaren Text (ISO-8859-1) liefern, ab Zeichen offset,
   hoechstens maxlen Zeichen. 1 = ok, 0 = Fehler (err). */
int web_fetch(const char *url, long offset, long maxlen, StrBuf *out, char *err, int errlen);

#endif
