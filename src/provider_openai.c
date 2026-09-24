/* OpenAI-kompatibler Provider (OpenAI, Gemini-OpenAI-Endpoint, OpenRouter, lokal). */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <exec/types.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>

#include "provider.h"
#include "json.h"
#include "net.h"
#include "ui.h"
#include "amiloc.h"

#define RETRIES     3               /* Wiederholungen bei ueberlastetem Server */

/* Bekannte Anbieter; alle sprechen die OpenAI-kompatible Schnittstelle */
static const ProviderDef provider_defs[] = {
    { "openai",     "OpenAI",                      "https://api.openai.com/v1",  1, "gpt-5.4-mini" },
    { "openrouter", "OpenRouter",                  "https://openrouter.ai/api/v1", 1, "openai/gpt-5.4-mini" },
    { "ollama",     "Ollama",                      "http://localhost:11434/v1",  0, "" },
    { "ollamacloud", "Ollama Cloud",               "https://ollama.com/v1",      1, "gpt-oss:120b" },
    { "custom",     "Custom (OpenAI-compatible)", "",                           0, "" },
    { NULL }
};

const ProviderDef *provider_defs_list(void)
{
    return provider_defs;
}

static const ProviderDef *find_def(const char *id)
{
    int i;
    for (i = 0; provider_defs[i].id; i++)
        if (stricmp(provider_defs[i].id, id) == 0)
            return &provider_defs[i];
    return &provider_defs[0];
}

const char *provider_active(const Config *cfg)
{
    return find_def(config_get(cfg, "provider.active", "openai"))->id;
}

static const char *get2(const Config *cfg, const char *id, const char *key, const char *def)
{
    char k[64];
    const char *v;

    snprintf(k, sizeof(k), "%s.%s", id, key);
    if ((v = config_get(cfg, k, NULL)) && *v)
        return v;
    /* alte Konfiguration ohne Anbieter-Abschnitte: [provider] gilt fuer OpenAI */
    if (stricmp(id, "openai") == 0) {
        snprintf(k, sizeof(k), "provider.%s", key);
        if ((v = config_get(cfg, k, NULL)) && *v)
            return v;
    }
    return def;
}

void provider_settings(const Config *cfg, const char *id, ProviderSettings *ps)
{
    const ProviderDef *d = find_def(id ? id : provider_active(cfg));
    const char *legacy = stricmp(d->id, "openai") == 0 ? config_get(cfg, "provider.endpoint", NULL) : NULL;
    int n;

    ps->def = d;
    ps->key = get2(cfg, d->id, "api_key", "");
    ps->model = get2(cfg, d->id, "model", d->default_model);
    if (legacy && !config_get(cfg, "openai.base", NULL)) {
        /* alte Eintraege "endpoint=.../chat/completions" in eine Basis-URL umrechnen */
        strncpy(ps->base, legacy, sizeof(ps->base) - 1);
        ps->base[sizeof(ps->base) - 1] = 0;
        n = strlen(ps->base);
        if (n > 17 && strcmp(ps->base + n - 17, "/chat/completions") == 0)
            ps->base[n - 17] = 0;
    } else {
        strncpy(ps->base, get2(cfg, d->id, "base", d->base), sizeof(ps->base) - 1);
        ps->base[sizeof(ps->base) - 1] = 0;
    }
    n = strlen(ps->base);
    while (n > 0 && ps->base[n - 1] == '/')
        ps->base[--n] = 0;
}

const char *provider_default_model(const Config *cfg)
{
    ProviderSettings ps;
    provider_settings(cfg, NULL, &ps);
    return ps.model;
}

/* Kopfzeilen fuer die Anfrage (Authorization nur mit Key) */
static void add_headers(StrBuf *hdr, const ProviderSettings *ps, const char *key)
{
    if (key && *key) {
        sb_add(hdr, "Authorization: Bearer ");
        sb_add(hdr, key);
        sb_add(hdr, "\r\n");
    }
    if (stricmp(ps->def->id, "openrouter") == 0)
        sb_add(hdr, "X-Title: AmiCode\r\nHTTP-Referer: https://aminet.net\r\n");
}

static int cmp_str(const void *a, const void *b)
{
    return stricmp(*(const char **)a, *(const char **)b);
}

/* Fuer OpenAI nur Chat-Modelle anbieten */
static int openai_chat_model(const char *id)
{
    static const char *skip[] = { "audio", "tts", "image", "transcribe", "realtime", "search",
                                  "embedding", "moderation", "dall-e", "whisper", "davinci",
                                  "babbage", "instruct", NULL };
    int i;

    if (strncmp(id, "gpt-", 4) != 0 && strncmp(id, "o1", 2) != 0 && strncmp(id, "o3", 2) != 0 &&
        strncmp(id, "o4", 2) != 0 && strncmp(id, "chatgpt", 7) != 0)
        return 0;
    for (i = 0; skip[i]; i++)
        if (strstr(id, skip[i]))
            return 0;
    return 1;
}

int provider_models(const Config *cfg, const char *id, const char *base, const char *key,
                    StrBuf *out, char *err, int errlen)
{
    ProviderSettings ps;
    StrBuf url, hdr;
    HttpResponse resp;
    JNode *root = NULL, *data, *m;
    const char **ids = NULL;
    int n = 0, cap = 0, i;

    provider_settings(cfg, id, &ps);
    if (base && *base) {
        strncpy(ps.base, base, sizeof(ps.base) - 1);
        ps.base[sizeof(ps.base) - 1] = 0;
        while (*ps.base && ps.base[strlen(ps.base) - 1] == '/')
            ps.base[strlen(ps.base) - 1] = 0;
    }
    if (!key)
        key = ps.key;
    if (!*ps.base) {
        snprintf(err, errlen, "%s", GetStr(MSG_PROV_NO_BASE));
        return -1;
    }

    sb_init(&url);
    sb_add(&url, ps.base);
    sb_add(&url, "/models");
    if (stricmp(ps.def->id, "openrouter") == 0)
        sb_add(&url, "?supported_parameters=tools");     /* nur Modelle, die Tools koennen */
    sb_init(&hdr);
    add_headers(&hdr, &ps, key);

    if (!https_get(url.buf, hdr.len ? hdr.buf : NULL, &resp, err, errlen)) {
        n = -1;
        goto out;
    }
    root = resp.body ? json_parse(resp.body) : NULL;
    if (resp.status != 200) {
        const char *msg = json_str(json_path(root, "error.message"));
        snprintf(err, errlen, "HTTP %d: %s", resp.status, msg ? msg : GetStr(MSG_PROV_NO_MESSAGE));
        n = -1;
    } else if (!(data = json_get(root, "data")) || data->type != J_ARR) {
        snprintf(err, errlen, "%s", GetStr(MSG_PROV_NO_MODELLIST));
        n = -1;
    } else {
        for (m = data->child; m; m = m->next) {
            const char *mid = json_str(json_get(m, "id"));
            if (!mid || (stricmp(ps.def->id, "openai") == 0 && !openai_chat_model(mid)))
                continue;
            if (strstr(mid, ":batch"))
                continue;       /* OpenRouter-Batch-Varianten taugen nicht fuer den Dialog */
            if (n == cap) {
                const char **ni = realloc(ids, (cap = cap ? cap * 2 : 64) * sizeof(char *));
                if (!ni)
                    break;
                ids = ni;
            }
            ids[n++] = mid;
        }
        qsort(ids, n, sizeof(char *), cmp_str);
        for (i = 0; i < n; i++) {
            sb_add(out, ids[i]);
            sb_add(out, "\n");
        }
    }
    free(ids);
    json_free(root);
    http_response_free(&resp);
out:
    if (hdr.buf)
        memset(hdr.buf, 0, hdr.len);
    sb_free(&hdr);
    sb_free(&url);
    return n;
}

#define MAX_STREAM_CALLS 16

/* Zustand beim Zusammensetzen einer gestreamten Antwort */
typedef struct {
    StrBuf content;
    int has_content;
    struct {
        StrBuf id, name, args;
    } call[MAX_STREAM_CALLS];
    int ncalls;
    long tokens_in, tokens_out, tokens_cached;
    char cost[32];              /* OpenRouter: usage.cost in Dollar, als Text */
    char errmsg[200];           /* Ollama: Fehler im Stream */
    StreamFn on_text;
    void *user;
} Stream;

static void stream_data(const char *data, void *user)
{
    Stream *st = user;
    JNode *j, *delta, *calls, *c, *usage;
    const char *piece;

    if (strcmp(data, "[DONE]") == 0 || !(j = json_parse(data)))
        return;
    if ((usage = json_get(j, "usage")) && usage->type == J_OBJ) {
        JNode *ti = json_get(usage, "prompt_tokens"), *to = json_get(usage, "completion_tokens");
        if (ti && ti->str) st->tokens_in = atol(ti->str);
        if (to && to->str) st->tokens_out = atol(to->str);
        if ((to = json_path(usage, "prompt_tokens_details.cached_tokens")) && to->str)
            st->tokens_cached = atol(to->str);
        if ((to = json_get(usage, "cost")) && to->str) {
            strncpy(st->cost, to->str, sizeof(st->cost) - 1);
            st->cost[sizeof(st->cost) - 1] = 0;
        }
    }
    delta = json_path(j, "choices.0.delta");
    if ((piece = json_str(json_get(delta, "content"))) && *piece) {
        sb_add(&st->content, piece);
        st->has_content = 1;
        if (st->on_text)
            st->on_text(piece, st->user);
    }
    /* Tool-Aufrufe kommen in Stuecken; "index" sagt, zu welchem Aufruf sie gehoeren */
    calls = json_get(delta, "tool_calls");
    if (calls && calls->type == J_ARR) {
        for (c = calls->child; c; c = c->next) {
            JNode *ix = json_get(c, "index");
            int i = ix && ix->str ? atoi(ix->str) : 0;
            const char *v;

            if (i < 0 || i >= MAX_STREAM_CALLS)
                continue;
            if (i >= st->ncalls)
                st->ncalls = i + 1;
            if ((v = json_str(json_get(c, "id"))))
                sb_add(&st->call[i].id, v);
            if ((v = json_str(json_path(c, "function.name"))))
                sb_add(&st->call[i].name, v);
            if ((v = json_str(json_path(c, "function.arguments"))))
                sb_add(&st->call[i].args, v);
        }
    }
    json_free(j);
}

/* Aus den gesammelten Stuecken eine Antwort wie ohne Streaming bauen */
static JNode *stream_result(Stream *st)
{
    StrBuf sb;
    JNode *root;
    char num[40];
    int i;

    sb_init(&sb);
    sb_add(&sb, "{\"choices\":[{\"message\":{\"role\":\"assistant\",\"content\":");
    if (st->has_content)
        sb_add_jstr(&sb, st->content.buf, 0);
    else
        sb_add(&sb, "null");
    if (st->ncalls) {
        sb_add(&sb, ",\"tool_calls\":[");
        for (i = 0; i < st->ncalls; i++) {
            if (i)
                sb_add(&sb, ",");
            sb_add(&sb, "{\"id\":");
            sb_add_jstr(&sb, st->call[i].id.buf ? st->call[i].id.buf : "", 0);
            sb_add(&sb, ",\"type\":\"function\",\"function\":{\"name\":");
            sb_add_jstr(&sb, st->call[i].name.buf ? st->call[i].name.buf : "", 0);
            sb_add(&sb, ",\"arguments\":");
            sb_add_jstr(&sb, st->call[i].args.buf ? st->call[i].args.buf : "{}", 0);
            sb_add(&sb, "}}");
        }
        sb_add(&sb, "]");
    }
    snprintf(num, sizeof(num), "%ld", st->tokens_in);
    sb_add(&sb, "}}],\"usage\":{\"prompt_tokens\":");
    sb_add(&sb, num);
    snprintf(num, sizeof(num), "%ld", st->tokens_out);
    sb_add(&sb, ",\"completion_tokens\":");
    sb_add(&sb, num);
    snprintf(num, sizeof(num), "%ld", st->tokens_cached);
    sb_add(&sb, ",\"prompt_tokens_details\":{\"cached_tokens\":");
    sb_add(&sb, num);
    sb_add(&sb, "}");
    if (st->cost[0] && strspn(st->cost, "0123456789.eE+-") == strlen(st->cost)) {
        sb_add(&sb, ",\"cost\":");
        sb_add(&sb, st->cost);
    }
    sb_add(&sb, "}}");
    root = sb.oom ? NULL : json_parse(sb.buf);
    sb_free(&sb);
    return root;
}

static void stream_free(Stream *st)
{
    int i;
    sb_free(&st->content);
    for (i = 0; i < MAX_STREAM_CALLS; i++) {
        sb_free(&st->call[i].id);
        sb_free(&st->call[i].name);
        sb_free(&st->call[i].args);
    }
}

/* ---------- Ollama: eigene Schnittstelle /api/chat ----------
   Nur sie kennt options.num_ctx (Kontextgroesse); die OpenAI-kompatible
   Schnittstelle ignoriert das. Der Verlauf wird ins Ollama-Format umgesetzt:
   Tool-Argumente als Objekt statt als Text, Tool-Ergebnisse mit tool_name. */

#define OLLAMA_CTX_DEFAULT "32768"
#define MAX_ID_MAP 128

static void ollama_root(const ProviderSettings *ps, char *out, int len)
{
    int n;
    strncpy(out, ps->base, len - 1);
    out[len - 1] = 0;
    n = strlen(out);
    if (n > 3 && strcmp(out + n - 3, "/v1") == 0)
        out[n - 3] = 0;
}

/* Inhalt einer Nachricht als Text (auch aus einem content-Array mit Cache-Marken) */
static const char *msg_text(JNode *m)
{
    JNode *c = json_get(m, "content");
    if (c && c->type == J_STR)
        return c->str;
    if (c && c->type == J_ARR && c->child)
        return json_str(json_get(c->child, "text"));
    return NULL;
}

static void ollama_messages(const char *messages, StrBuf *out)
{
    static char ids[MAX_ID_MAP][48], names[MAX_ID_MAP][48];
    JNode *arr = json_parse(messages), *m;
    int nmap = 0, first = 1;

    if (!arr || arr->type != J_ARR) {
        sb_add(out, messages);
        json_free(arr);
        return;
    }
    sb_add(out, "[");
    for (m = arr->child; m; m = m->next) {
        const char *role = json_str(json_get(m, "role"));
        const char *text = msg_text(m);
        JNode *calls = json_get(m, "tool_calls"), *c;

        if (!role)
            continue;
        if (!first)
            sb_add(out, ",");
        first = 0;
        sb_add(out, "{\"role\":");
        sb_add_jstr(out, role, 0);
        sb_add(out, ",\"content\":");
        sb_add_jstr(out, text ? text : "", 0);
        if (calls && calls->type == J_ARR && calls->child) {
            int cf = 1;
            sb_add(out, ",\"tool_calls\":[");
            for (c = calls->child; c; c = c->next) {
                const char *id = json_str(json_get(c, "id"));
                const char *name = json_str(json_path(c, "function.name"));
                const char *args = json_str(json_path(c, "function.arguments"));
                JNode *ao = args ? json_parse(args) : NULL;

                if (!cf)
                    sb_add(out, ",");
                cf = 0;
                sb_add(out, "{\"function\":{\"name\":");
                sb_add_jstr(out, name ? name : "", 0);
                sb_add(out, ",\"arguments\":");
                if (ao && ao->type == J_OBJ)
                    sb_add_json(out, ao);
                else
                    sb_add(out, "{}");
                sb_add(out, "}}");
                json_free(ao);
                if (id && name && nmap < MAX_ID_MAP) {
                    strncpy(ids[nmap], id, sizeof(ids[0]) - 1);
                    ids[nmap][sizeof(ids[0]) - 1] = 0;
                    strncpy(names[nmap], name, sizeof(names[0]) - 1);
                    names[nmap][sizeof(names[0]) - 1] = 0;
                    nmap++;
                }
            }
            sb_add(out, "]");
        }
        if (strcmp(role, "tool") == 0) {
            const char *id = json_str(json_get(m, "tool_call_id"));
            int i;
            for (i = 0; id && i < nmap; i++)
                if (strcmp(ids[i], id) == 0) {
                    sb_add(out, ",\"tool_name\":");
                    sb_add_jstr(out, names[i], 0);
                    break;
                }
        }
        sb_add(out, "}");
    }
    sb_add(out, "]");
    json_free(arr);
}

/* Eine Zeile (oder die ganze Antwort) von /api/chat auswerten */
static void ollama_line(const char *line, void *user)
{
    Stream *st = user;
    JNode *j = json_parse(line), *msg, *calls, *c, *v;
    const char *piece, *e;

    if (!j)
        return;
    if ((e = json_str(json_get(j, "error")))) {
        strncpy(st->errmsg, e, sizeof(st->errmsg) - 1);
        st->errmsg[sizeof(st->errmsg) - 1] = 0;
    }
    msg = json_get(j, "message");
    if ((piece = json_str(json_get(msg, "content"))) && *piece) {
        sb_add(&st->content, piece);
        st->has_content = 1;
        if (st->on_text)
            st->on_text(piece, st->user);
    }
    calls = json_get(msg, "tool_calls");
    for (c = calls ? calls->child : NULL; c && st->ncalls < MAX_STREAM_CALLS; c = c->next) {
        const char *name = json_str(json_path(c, "function.name"));
        JNode *args = json_path(c, "function.arguments");
        int i = st->ncalls++;
        static unsigned long counter;   /* Ollama vergibt keine IDs: eindeutig ueber die ganze Sitzung */
        char id[24];

        snprintf(id, sizeof(id), "call_o%lu", ++counter);
        sb_add(&st->call[i].id, id);
        sb_add(&st->call[i].name, name ? name : "");
        if (args && args->type == J_OBJ)
            sb_add_json(&st->call[i].args, args);
        else if (args && args->type == J_STR)
            sb_add(&st->call[i].args, args->str);
        else
            sb_add(&st->call[i].args, "{}");
    }
    if ((v = json_get(j, "prompt_eval_count")) && v->str)
        st->tokens_in = atol(v->str);
    if ((v = json_get(j, "eval_count")) && v->str)
        st->tokens_out = atol(v->str);
    json_free(j);
}

static JNode *ollama_chat(const Config *cfg, const ProviderSettings *ps, const char *model,
                          const char *messages, const char *tools, int stream,
                          Stream *st, char *err, int errlen)
{
    char root[256], url[300], num[24];
    const char *think = config_get(cfg, "ollama.think", "");
    StrBuf req;
    HttpResponse resp;
    JNode *out = NULL;
    int ok;

    ollama_root(ps, root, sizeof(root));
    snprintf(url, sizeof(url), "%s/api/chat", root);
    sb_init(&req);
    sb_add(&req, "{\"model\":");
    sb_add_jstr(&req, model, 1);
    sb_add(&req, ",\"messages\":");
    ollama_messages(messages, &req);
    if (tools) {
        sb_add(&req, ",\"tools\":");
        sb_add(&req, tools);
    }
    sb_add(&req, stream ? ",\"stream\":true" : ",\"stream\":false");
    if (!stricmp(think, "yes") || !stricmp(think, "no"))
        sb_add(&req, !stricmp(think, "yes") ? ",\"think\":true" : ",\"think\":false");
    snprintf(num, sizeof(num), "%ld", atol(config_get(cfg, "ollama.context", OLLAMA_CTX_DEFAULT)));
    sb_add(&req, ",\"options\":{\"num_ctx\":");
    sb_add(&req, num);
    sb_add(&req, "}}");
    if (req.oom) {
        snprintf(err, errlen, "%s", GetStr(MSG_NO_MEMORY));
        sb_free(&req);
        return NULL;
    }

    if (stream)
        ok = https_post_lines(url, NULL, req.buf, req.len, ollama_line, st, &resp, err, errlen);
    else
        ok = https_post(url, NULL, req.buf, req.len, &resp, err, errlen);
    sb_free(&req);
    if (!ok)
        return NULL;
    if (resp.status != 200) {
        JNode *e = resp.body ? json_parse(resp.body) : NULL;
        const char *m = json_str(json_get(e, "error"));
        snprintf(err, errlen, "Ollama HTTP %d: %s", resp.status, m ? m : GetStr(MSG_PROV_NO_MESSAGE));
        json_free(e);
    } else {
        if (!stream && resp.body)
            ollama_line(resp.body, st);
        if (st->errmsg[0])
            snprintf(err, errlen, "Ollama: %s", st->errmsg);
        else if (!(out = stream_result(st)))
            snprintf(err, errlen, "%s", GetStr(MSG_NO_MEMORY_ANSWER));
    }
    http_response_free(&resp);
    return out;
}

const char *provider_effort(const Config *cfg, const char *id)
{
    char key[64];
    const char *v;

    if (!id)
        id = provider_active(cfg);
    snprintf(key, sizeof(key), "%s.effort", id);
    v = config_get(cfg, key, stricmp(id, "openrouter") == 0 ? "low" : "");
    if (stricmp(v, "low") == 0 || stricmp(v, "medium") == 0 || stricmp(v, "high") == 0)
        return v;
    return "";     /* "default" oder leer: Voreinstellung des Modells */
}

long provider_context_limit(const Config *cfg)
{
    ProviderSettings ps;
    char key[64];

    provider_settings(cfg, NULL, &ps);
    snprintf(key, sizeof(key), "%s.context", ps.def->id);
    return atol(config_get(cfg, key, stricmp(ps.def->id, "ollama") == 0 ? OLLAMA_CTX_DEFAULT : "0"));
}

/* Chat-Anfrage zusammenbauen. force_no_effort: einige neue OpenAI-Modelle (z. B.
   gpt-5.6-*) lehnen Werkzeuge auf /chat/completions ab, wenn reasoning_effort
   nicht ausdruecklich "none" ist - auch wenn das Feld ganz fehlt (provider_chat
   versucht das automatisch, siehe unten). */
static void build_chat_body(StrBuf *req, const char *model, const char *messages,
                            const char *tools, int stream, const Config *cfg,
                            const ProviderSettings *ps, int force_no_effort)
{
    req->len = 0;
    sb_add(req, "{\"model\":");
    sb_add_jstr(req, model, 1);
    sb_add(req, ",\"messages\":");
    sb_add(req, messages);
    if (tools) {
        sb_add(req, ",\"tools\":");
        sb_add(req, tools);
    }
    if (stream)
        sb_add(req, ",\"stream\":true,\"stream_options\":{\"include_usage\":true}");
    {
        /* Denkaufwand: kostet als Ausgabe-Tokens. OpenRouter vereinheitlicht ihn fuer
           alle Modelle; bei OpenAI nur senden, wenn eingestellt (nicht jedes Modell kennt ihn). */
        const char *effort = force_no_effort ? "none" : provider_effort(cfg, ps->def->id);
        if (*effort) {
            if (!force_no_effort && stricmp(ps->def->id, "openrouter") == 0) {
                sb_add(req, ",\"reasoning\":{\"effort\":");
                sb_add_jstr(req, effort, 0);
                sb_add(req, "}");
            } else {
                sb_add(req, ",\"reasoning_effort\":");
                sb_add_jstr(req, effort, 0);
            }
        }
    }
    if (stricmp(ps->def->id, "openrouter") == 0)
        sb_add(req, ",\"usage\":{\"include\":true}");    /* liefert usage.cost in Dollar */
    sb_add(req, "}");
}

JNode *provider_chat(const Config *cfg, const char *model, const char *messages,
                     const char *tools, StreamFn on_text, void *user,
                     char *err, int errlen)
{
    ProviderSettings ps;
    const char *key;
    const char *sopt = config_get(cfg, "provider.stream", "yes");
    char endpoint[300];
    int stream = on_text && stricmp(sopt, "no") != 0 && stricmp(sopt, "0") != 0;
    StrBuf req, hdr;
    HttpResponse resp;
    JNode *root = NULL;
    Stream st;
    int ok, attempt, effort_retried = 0;

    memset(&st, 0, sizeof(st));
    st.on_text = on_text;
    st.user = user;
    provider_settings(cfg, NULL, &ps);
    key = ps.key;
    if (!model || !*model)
        model = ps.model;
    if (!*ps.base) {
        snprintf(err, errlen, GetStr(MSG_PROV_NO_BASE_FOR), ps.def->name);
        return NULL;
    }
    if (ps.def->needs_key && !*key) {
        snprintf(err, errlen, GetStr(MSG_PROV_NO_KEY), ps.def->name);
        return NULL;
    }
    if (!*model) {
        snprintf(err, errlen, GetStr(MSG_PROV_NO_MODEL), ps.def->name);
        return NULL;
    }
    if (stricmp(ps.def->id, "ollama") == 0 &&
        stricmp(config_get(cfg, "ollama.api", "native"), "openai") != 0) {
        root = ollama_chat(cfg, &ps, model, messages, tools, stream, &st, err, errlen);
        stream_free(&st);
        return root;
    }
    snprintf(endpoint, sizeof(endpoint), "%s/chat/completions", ps.base);

    sb_init(&req);
    build_chat_body(&req, model, messages, tools, stream, cfg, &ps, 0);

    sb_init(&hdr);
    add_headers(&hdr, &ps, key);

    if (req.oom || hdr.oom) {
        snprintf(err, errlen, "%s", GetStr(MSG_NO_MEMORY));
        goto out;
    }
    for (attempt = 0; ; attempt++) {
        if (stream)
            ok = https_post_sse(endpoint, hdr.len ? hdr.buf : NULL, req.buf, req.len, stream_data, &st, &resp, err, errlen);
        else
            ok = https_post(endpoint, hdr.len ? hdr.buf : NULL, req.buf, req.len, &resp, err, errlen);
        if (!ok)
            goto out;
        /* Ueberlastet / Ratenlimit / Gateway: kurz warten und nochmal (Cloud-Anbieter, freie Modelle) */
        /* Manche neuen OpenAI-Modelle lehnen Werkzeuge ab, wenn reasoning_effort nicht
           ausdruecklich "none" ist (auch wenn das Feld fehlt) - einmal mit "none" erneut senden. */
        if (!effort_retried && tools && resp.status == 400 && resp.body &&
            strstr(resp.body, "reasoning_effort") && strstr(resp.body, "none")) {
            effort_retried = 1;
            ui_printf(UI_DETAIL, "%s", GetStr(MSG_PROV_EFFORT_RETRY));
            http_response_free(&resp);
            stream_free(&st);
            memset(&st, 0, sizeof(st));
            st.on_text = on_text;
            st.user = user;
            build_chat_body(&req, model, messages, tools, stream, cfg, &ps, 1);
            continue;
        }
        if (attempt < RETRIES && (resp.status == 429 || resp.status == 502 ||
                                  resp.status == 503 || resp.status == 504)) {
            long wait = 10L << attempt;         /* 10, 20, 40 Sekunden */
            ui_printf(UI_DETAIL, GetStr(MSG_PROV_RETRY), resp.status, wait);
            http_response_free(&resp);
            stream_free(&st);
            memset(&st, 0, sizeof(st));
            st.on_text = on_text;
            st.user = user;
            for (; wait > 0; wait--) {
                if (SetSignal(0, 0) & SIGBREAKF_CTRL_C)
                    break;
                Delay(50);
            }
            if (SetSignal(0, 0) & SIGBREAKF_CTRL_C) {
                snprintf(err, errlen, "%s", GetStr(MSG_ABORTED_CTRLC));
                goto out;
            }
            continue;
        }
        break;
    }

    if (resp.status != 200) {
        root = resp.body ? json_parse(resp.body) : NULL;
        {
            const char *msg = json_str(json_path(root, "error.message"));
            char *m = msg ? strdup(msg) : NULL;
            if (m)
                utf8_to_latin1(m);
            snprintf(err, errlen, "HTTP %d: %s", resp.status,
                     m ? m : (root ? GetStr(MSG_PROV_NO_ERRMSG) : GetStr(MSG_PROV_NO_JSON)));
            free(m);
        }
        json_free(root);
        root = NULL;
    } else if (stream) {
        if (!(root = stream_result(&st)))
            snprintf(err, errlen, "%s", GetStr(MSG_NO_MEMORY_ANSWER));
    } else if (!(root = json_parse(resp.body))) {
        snprintf(err, errlen, "%s", GetStr(MSG_PROV_BAD_JSON));
    } else if (!json_path(root, RESP_MESSAGE)) {
        snprintf(err, errlen, "%s", GetStr(MSG_PROV_NO_MSG));
        json_free(root);
        root = NULL;
    }
    http_response_free(&resp);

out:
    stream_free(&st);
    /* Key nicht im Speicher liegen lassen */
    if (hdr.buf)
        memset(hdr.buf, 0, hdr.len);
    sb_free(&hdr);
    sb_free(&req);
    return root;
}

long provider_ollama_context(const Config *cfg, const char *model)
{
    ProviderSettings ps;
    HttpResponse resp;
    char url[300], err[100];
    JNode *root, *m;
    long ctx = -1;
    int n;

    provider_settings(cfg, "ollama", &ps);
    strncpy(url, ps.base, sizeof(url) - 20);
    url[sizeof(url) - 20] = 0;
    n = strlen(url);
    if (n > 3 && strcmp(url + n - 3, "/v1") == 0)
        url[n - 3] = 0;
    strcat(url, "/api/ps");
    if (!https_get(url, NULL, &resp, err, sizeof(err)))
        return -1;
    if (resp.status == 200 && (root = json_parse(resp.body))) {
        JNode *models = json_get(root, "models");
        for (m = models ? models->child : NULL; m; m = m->next) {
            const char *name = json_str(json_get(m, "name"));
            JNode *cl = json_get(m, "context_length");
            if (name && cl && cl->str && (stricmp(name, model) == 0 ||
                (strncmp(name, model, strlen(model)) == 0 && name[strlen(model)] == ':')))
                ctx = atol(cl->str);
        }
        json_free(root);
    }
    http_response_free(&resp);
    return ctx;
}
