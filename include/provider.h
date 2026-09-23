#ifndef AMICODE_PROVIDER_H
#define AMICODE_PROVIDER_H

#include "config.h"
#include "json.h"

/* Eine Chat-Anfrage. model: NULL = aus der Konfiguration.
   messages: JSON-Array (UTF-8), tools: JSON-Array oder NULL.
   Liefert die ganze geparste Antwort (mit json_free freigeben) oder NULL
   bei Fehler (err beschrieben, ISO-8859-1). */
/* on_text: bei Streaming fuer jedes Textstueck (UTF-8) aufgerufen; NULL = nicht streamen.
   Auch beim Streaming liefert provider_chat am Ende die vollstaendige Antwort
   in derselben Form wie ohne Streaming. */
typedef void (*StreamFn)(const char *utf8, void *user);

JNode *provider_chat(const Config *cfg, const char *model, const char *messages,
                     const char *tools, StreamFn on_text, void *user,
                     char *err, int errlen);

/* Anbieter-Definition (fest eingebaut) */
typedef struct {
    const char *id;             /* "openai", "openrouter", "ollama", "custom" */
    const char *name;
    const char *base;           /* Standard-Basis-URL (.../v1) */
    int needs_key;
    const char *default_model;
} ProviderDef;

/* Wirksame Einstellungen eines Anbieters aus der Konfiguration */
typedef struct {
    const ProviderDef *def;
    char base[256];             /* ohne abschliessendes '/' */
    const char *key;            /* "" wenn keiner */
    const char *model;          /* "" wenn keins */
} ProviderSettings;

const ProviderDef *provider_defs_list(void);        /* endet mit id == NULL */
const char *provider_active(const Config *cfg);     /* [provider] active=, Standard openai */
void provider_settings(const Config *cfg, const char *id, ProviderSettings *ps);  /* id NULL = aktiv */

/* Modell des aktiven Anbieters */
const char *provider_default_model(const Config *cfg);

/* Verfuegbare Modelle abfragen (GET base/models). base/key: NULL = aus der Konfiguration.
   out: ein Modell pro Zeile, sortiert. Liefert die Anzahl oder -1 (err beschrieben). */
int provider_models(const Config *cfg, const char *id, const char *base, const char *key,
                    StrBuf *out, char *err, int errlen);

/* Pfade in der Antwort */
#define RESP_MESSAGE    "choices.0.message"
#define RESP_TOKENS_IN  "usage.prompt_tokens"
#define RESP_TOKENS_OUT "usage.completion_tokens"
#define RESP_TOKENS_CACHED "usage.prompt_tokens_details.cached_tokens"
#define RESP_COST       "usage.cost"    /* OpenRouter: Kosten der Anfrage in Dollar */

/* Denkaufwand ([id] effort=low|medium|high|default) fuer Anfragen; "" = nicht senden.
   OpenRouter: Standard low. id NULL = aktiver Anbieter. */
const char *provider_effort(const Config *cfg, const char *id);

/* Kontextgroesse des aktiven Anbieters in Tokens ([id] context=), 0 = unbegrenzt.
   Ollama: Standard 32768, wird als num_ctx mitgeschickt. */
long provider_context_limit(const Config *cfg);

/* Ollama: Kontextgroesse des geladenen Modells (GET .../api/ps), -1 = unbekannt */
long provider_ollama_context(const Config *cfg, const char *model);

#endif
