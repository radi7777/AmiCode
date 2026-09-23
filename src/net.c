#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <errno.h>

#include <exec/types.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <proto/bsdsocket.h>

#include <libraries/amisslmaster.h>
#include <libraries/amissl.h>
#include <proto/amisslmaster.h>
#include <proto/amissl.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/x509v3.h>

#include "net.h"
#include "json.h"

struct Library *SocketBase = NULL;
struct Library *AmiSSLMasterBase = NULL;
struct Library *AmiSSLBase = NULL;
struct Library *AmiSSLExtBase = NULL;

static SSL_CTX *ssl_ctx = NULL;

#define USER_AGENT "AmiCode/0.1 (AmigaOS 3.2; m68k)"

static void set_err(char *err, int errlen, const char *msg)
{
    strncpy(err, msg, errlen - 1);
    err[errlen - 1] = 0;
}

static void set_ssl_err(char *err, int errlen, const char *prefix)
{
    char buf[200];
    unsigned long e = ERR_get_error();

    if (e) {
        ERR_error_string_n(e, buf, sizeof(buf));
        snprintf(err, errlen, "%s: %s", prefix, buf);
    } else {
        snprintf(err, errlen, "%s", prefix);
    }
}

int net_open(char *err, int errlen)
{
    if (!(SocketBase = OpenLibrary("bsdsocket.library", 4))) {
        set_err(err, errlen, "bsdsocket.library nicht gefunden - TCP/IP-Stack gestartet?");
        return 0;
    }
    if (!(AmiSSLMasterBase = OpenLibrary("amisslmaster.library", AMISSLMASTER_MIN_VERSION))) {
        set_err(err, errlen, "amisslmaster.library (AmiSSL 5) nicht gefunden");
        return 0;
    }
    if (OpenAmiSSLTags(AMISSL_CURRENT_VERSION,
                       AmiSSL_UsesOpenSSLStructs, TRUE,
                       AmiSSL_InitAmiSSL, TRUE,
                       AmiSSL_GetAmiSSLBase, (ULONG)&AmiSSLBase,
                       AmiSSL_GetAmiSSLExtBase, (ULONG)&AmiSSLExtBase,
                       AmiSSL_SocketBase, (ULONG)SocketBase,
                       AmiSSL_ErrNoPtr, (ULONG)&errno,
                       TAG_DONE) != 0) {
        set_err(err, errlen, "AmiSSL konnte nicht initialisiert werden (Version zu alt?)");
        return 0;
    }

    if (!(ssl_ctx = SSL_CTX_new(TLS_client_method()))) {
        set_ssl_err(err, errlen, "SSL_CTX_new");
        return 0;
    }
    SSL_CTX_set_min_proto_version(ssl_ctx, TLS1_2_VERSION);
    SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_PEER, NULL);
    /* nutzt AmiSSL:Certs */
    if (!SSL_CTX_set_default_verify_paths(ssl_ctx)) {
        set_ssl_err(err, errlen, "Zertifikatsspeicher nicht ladbar");
        return 0;
    }
    return 1;
}

void net_close(void)
{
    if (ssl_ctx) {
        SSL_CTX_free(ssl_ctx);
        ssl_ctx = NULL;
    }
    if (AmiSSLBase) {
        CloseAmiSSL();
        AmiSSLBase = NULL;
    }
    if (AmiSSLMasterBase) {
        CloseLibrary(AmiSSLMasterBase);
        AmiSSLMasterBase = NULL;
    }
    if (SocketBase) {
        CloseLibrary(SocketBase);
        SocketBase = NULL;
    }
}

/* http(s)://host[:port]/pfad zerlegen. *tls = 1 bei https */
static int parse_url(const char *url, char *host, int hostlen, int *port, const char **path, int *tls)
{
    const char *h, *e;
    int n;

    if (strncmp(url, "https://", 8) == 0) {
        h = url + 8;
        *tls = 1;
    } else if (strncmp(url, "http://", 7) == 0) {
        h = url + 7;
        *tls = 0;
    } else {
        return 0;
    }
    for (e = h; *e && *e != '/' && *e != ':'; e++)
        ;
    n = e - h;
    if (n <= 0 || n >= hostlen)
        return 0;
    memcpy(host, h, n);
    host[n] = 0;
    *port = *tls ? 443 : 80;
    if (*e == ':') {
        *port = atoi(e + 1);
        while (*e && *e != '/')
            e++;
    }
    *path = *e ? e : "/";
    return 1;
}

static int tcp_connect(const char *host, int port, char *err, int errlen)
{
    struct hostent *he;
    struct sockaddr_in sa;
    int s;

    if (!(he = gethostbyname((STRPTR)host))) {
        snprintf(err, errlen, "DNS: '%s' nicht aufloesbar", host);
        return -1;
    }
    if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        set_err(err, errlen, "socket() fehlgeschlagen");
        return -1;
    }
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port = htons(port);
    memcpy(&sa.sin_addr, he->h_addr, he->h_length);
    if (connect(s, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
        snprintf(err, errlen, "Verbindung zu %s:%d fehlgeschlagen (errno %d)", host, port, errno);
        CloseSocket(s);
        return -1;
    }
    return s;
}

static int ssl_write_all(SSL *ssl, const char *buf, unsigned long len)
{
    while (len > 0) {
        int n = SSL_write(ssl, buf, len > 16384 ? 16384 : (int)len);
        if (n <= 0)
            return 0;
        buf += n;
        len -= n;
    }
    return 1;
}

/* Prueft, ob der Header name (klein geschrieben) den Wert value enthaelt. */
static int header_has(const char *headers, const char *name, const char *value)
{
    const char *line = headers;
    int nlen = strlen(name), vlen = strlen(value);

    while ((line = strstr(line, "\r\n")) != NULL) {
        const char *p;
        int i;

        line += 2;
        for (i = 0; i < nlen; i++)
            if (tolower((unsigned char)line[i]) != name[i])
                break;
        if (i < nlen || line[nlen] != ':')
            continue;
        for (p = line + nlen + 1; *p && *p != '\r'; p++)
            if (strncasecmp(p, value, vlen) == 0)
                return 1;
    }
    return 0;
}

/* Wert eines Headers (name klein geschrieben) nach out kopieren. 1 = gefunden */
static int header_value(const char *headers, const char *name, char *out, int len)
{
    const char *line = headers;
    int nlen = strlen(name);

    while ((line = strstr(line, "\r\n")) != NULL) {
        int i;
        const char *v, *e;

        line += 2;
        for (i = 0; i < nlen; i++)
            if (tolower((unsigned char)line[i]) != name[i])
                break;
        if (i < nlen || line[nlen] != ':')
            continue;
        for (v = line + nlen + 1; *v == ' '; v++)
            ;
        for (e = v; *e && *e != '\r'; e++)
            ;
        if (e - v >= len)
            e = v + len - 1;
        memcpy(out, v, e - v);
        out[e - v] = 0;
        return 1;
    }
    return 0;
}

/* Chunked-Body in place dekodieren. Liefert neue Laenge oder -1. */
static long dechunk(char *data, unsigned long len)
{
    char *r = data, *w = data, *end = data + len;

    for (;;) {
        unsigned long sz;
        char *nl;

        sz = strtoul(r, NULL, 16);
        if (!(nl = strstr(r, "\r\n")) || nl >= end)
            return -1;
        r = nl + 2;
        if (sz == 0)
            break;
        if (r + sz > end)
            return -1;
        memmove(w, r, sz);
        w += sz;
        r += sz + 2;
        if (r > end)
            return -1;
    }
    *w = 0;
    return w - data;
}

/* Verbindung: mit TLS (ssl gesetzt) oder unverschluesselt (z. B. Ollama im LAN) */
typedef struct {
    SSL *ssl;
    int sock;
} Conn;

static int conn_write(Conn *c, const char *buf, unsigned long len)
{
    if (c->ssl)
        return ssl_write_all(c->ssl, buf, len);
    while (len > 0) {
        long n = send(c->sock, (APTR)buf, len > 16384 ? 16384 : len, 0);
        if (n <= 0)
            return 0;
        buf += n;
        len -= n;
    }
    return 1;
}

static int conn_read(Conn *c, char *buf, int len)
{
    if (c->ssl)
        return SSL_read(c->ssl, buf, len);
    return recv(c->sock, (APTR)buf, len, 0);
}

static void conn_close(Conn *c)
{
    if (c->ssl) {
        SSL_shutdown(c->ssl);
        SSL_free(c->ssl);
        c->ssl = NULL;
    }
    if (c->sock >= 0)
        CloseSocket(c->sock);
    c->sock = -1;
}

/* Verbinden, ggf. TLS aushandeln und die Anfrage senden. body == NULL: GET */
static int conn_request(Conn *c, const char *url, const char *extra_headers,
                        const char *body, unsigned long body_len, char *err, int errlen)
{
    char host[256], hdr[1024];
    const char *path;
    int port, tls;

    c->ssl = NULL;
    c->sock = -1;
    if (!parse_url(url, host, sizeof(host), &port, &path, &tls)) {
        snprintf(err, errlen, "Ungueltige URL: %s", url);
        return 0;
    }
    if ((c->sock = tcp_connect(host, port, err, errlen)) < 0)
        return 0;
    if (!tls)
        goto send_request;

    if (!(c->ssl = SSL_new(ssl_ctx))) {
        set_ssl_err(err, errlen, "SSL_new");
        conn_close(c);
        return 0;
    }
    SSL_set_fd(c->ssl, c->sock);
    SSL_set_tlsext_host_name(c->ssl, host);    /* SNI */
    SSL_set1_host(c->ssl, host);               /* Hostname im Zertifikat pruefen */

    if (SSL_connect(c->ssl) != 1) {
        long vr = SSL_get_verify_result(c->ssl);
        if (vr != X509_V_OK) {
            snprintf(err, errlen, "TLS: Zertifikat abgelehnt: %s%s",
                     X509_verify_cert_error_string(vr),
                     (vr == X509_V_ERR_CERT_NOT_YET_VALID || vr == X509_V_ERR_CERT_HAS_EXPIRED)
                         ? " - Systemuhr pruefen!" : "");
        } else {
            set_ssl_err(err, errlen, "TLS-Handshake fehlgeschlagen");
        }
        conn_close(c);
        return 0;
    }

send_request:
    if (body)
        snprintf(hdr, sizeof(hdr),
                 "POST %s HTTP/1.1\r\n"
                 "Host: %s\r\n"
                 "User-Agent: " USER_AGENT "\r\n"
                 "Content-Type: application/json\r\n"
                 "Content-Length: %lu\r\n"
                 "Connection: close\r\n",
                 path, host, body_len);
    else
        snprintf(hdr, sizeof(hdr),
                 "GET %s HTTP/1.1\r\n"
                 "Host: %s\r\n"
                 "User-Agent: " USER_AGENT "\r\n"
                 "Accept: application/json\r\n"
                 "Connection: close\r\n",
                 path, host);
    if (!conn_write(c, hdr, strlen(hdr)) ||
        (extra_headers && !conn_write(c, extra_headers, strlen(extra_headers))) ||
        !conn_write(c, "\r\n", 2) ||
        (body && !conn_write(c, body, body_len))) {
        set_ssl_err(err, errlen, "Senden fehlgeschlagen");
        conn_close(c);
        return 0;
    }
    return 1;
}

/* Rest der Antwort komplett lesen und an buf (len/cap) anhaengen */
static int read_all(Conn *c, char **buf, unsigned long *len, unsigned long *cap, char *err, int errlen)
{
    for (;;) {
        int n;
        if (SetSignal(0, 0) & SIGBREAKF_CTRL_C) {
            set_err(err, errlen, "Abgebrochen (CTRL-C)");
            return 0;
        }
        if (*len + 16385 > *cap) {
            char *nb = realloc(*buf, *cap * 2);
            if (!nb) {
                set_err(err, errlen, "Kein Speicher fuer Antwort");
                return 0;
            }
            *buf = nb;
            *cap *= 2;
        }
        n = conn_read(c, *buf + *len, 16384);
        if (n <= 0)
            break;
        *len += n;
    }
    (*buf)[*len] = 0;
    return 1;
}

/* Header in buf abtrennen: Status setzen, Body an den Anfang schieben (dekodiert) */
static int finish_body(char *buf, unsigned long len, HttpResponse *resp, char *err, int errlen)
{
    char *hend;
    long r;
    int chunked;

    if (strncmp(buf, "HTTP/1.", 7) != 0 || !(hend = strstr(buf, "\r\n\r\n"))) {
        set_err(err, errlen, "Ungueltige HTTP-Antwort");
        return 0;
    }
    resp->status = atoi(buf + 9);
    *hend = 0;
    chunked = header_has(buf, "transfer-encoding", "chunked");
    header_value(buf, "location", resp->location, sizeof(resp->location));
    header_value(buf, "content-type", resp->content_type, sizeof(resp->content_type));
    hend += 4;
    r = len - (hend - buf);
    memmove(buf, hend, r + 1);
    if (chunked && (r = dechunk(buf, r)) < 0) {
        set_err(err, errlen, "Fehlerhafte Chunked-Antwort");
        return 0;
    }
    resp->body = buf;
    resp->body_len = r;
    return 1;
}

int https_post(const char *url, const char *extra_headers,
               const char *body, unsigned long body_len,
               HttpResponse *resp, char *err, int errlen)
{
    Conn c;
    char *buf;
    unsigned long len = 0, cap = 65536;
    int ok = 0;

    memset(resp, 0, sizeof(*resp));

    if (!conn_request(&c, url, extra_headers, body, body_len, err, errlen))
        return 0;
    if (!(buf = malloc(cap))) {
        set_err(err, errlen, "Kein Speicher");
    } else if (read_all(&c, &buf, &len, &cap, err, errlen) &&
               finish_body(buf, len, resp, err, errlen)) {
        ok = 1;
        buf = NULL;     /* gehoert jetzt resp->body */
    }
    free(buf);
    conn_close(&c);
    return ok;
}

int https_get(const char *url, const char *extra_headers,
              HttpResponse *resp, char *err, int errlen)
{
    Conn c;
    char *buf;
    unsigned long len = 0, cap = 65536;
    int ok = 0;

    memset(resp, 0, sizeof(*resp));

    if (!conn_request(&c, url, extra_headers, NULL, 0, err, errlen))
        return 0;
    if (!(buf = malloc(cap))) {
        set_err(err, errlen, "Kein Speicher");
    } else if (read_all(&c, &buf, &len, &cap, err, errlen) &&
               finish_body(buf, len, resp, err, errlen)) {
        ok = 1;
        buf = NULL;
    }
    free(buf);
    conn_close(&c);
    return ok;
}

int http_fetch(const char *url, const char *extra_headers, HttpResponse *resp,
               char *final_url, int final_len, char *err, int errlen)
{
    char cur[600];
    int hops;

    strncpy(cur, url, sizeof(cur) - 1);
    cur[sizeof(cur) - 1] = 0;
    for (hops = 0; hops <= 5; hops++) {
        if (!https_get(cur, extra_headers, resp, err, errlen))
            return 0;
        if ((resp->status == 301 || resp->status == 302 || resp->status == 303 ||
             resp->status == 307 || resp->status == 308) && resp->location[0]) {
            char next[600];
            if (strncmp(resp->location, "http://", 7) == 0 || strncmp(resp->location, "https://", 8) == 0) {
                strncpy(next, resp->location, sizeof(next) - 1);
                next[sizeof(next) - 1] = 0;
            } else if (resp->location[0] == '/' && resp->location[1] == '/') {
                /* protokollrelativ */
                snprintf(next, sizeof(next), "%.*s%s", (int)(strchr(cur, ':') - cur + 1), cur, resp->location);
            } else {
                /* relativ zum Host */
                const char *h = strstr(cur, "://");
                const char *slash = h ? strchr(h + 3, '/') : NULL;
                int base = slash ? (int)(slash - cur) : (int)strlen(cur);
                snprintf(next, sizeof(next), "%.*s%s%s", base, cur,
                         resp->location[0] == '/' ? "" : "/", resp->location);
            }
            http_response_free(resp);
            strcpy(cur, next);
            continue;
        }
        if (final_url) {
            strncpy(final_url, cur, final_len - 1);
            final_url[final_len - 1] = 0;
        }
        return 1;
    }
    snprintf(err, errlen, "Zu viele Weiterleitungen");
    return 0;
}

/* ---------- Server-Sent Events ---------- */

enum { CH_SIZE, CH_DATA, CH_CRLF, CH_DONE };

typedef struct {
    int chunked, state;
    unsigned long remain;
    char size[20];
    int sizelen;
    StrBuf line;
    SseCallback cb;
    void *user;
    int raw;            /* 1: jede Zeile melden (NDJSON, Ollama), 0: nur "data:" (SSE) */
} Sse;

/* Nutzdaten zeilenweise zerlegen; "data:"-Zeilen gehen an den Callback */
static void sse_payload(Sse *s, const char *p, long n)
{
    while (n > 0) {
        const char *nl = memchr(p, '\n', n);
        long k = nl ? nl - p : n;

        sb_addn(&s->line, p, k);
        p += k;
        n -= k;
        if (!nl)
            break;
        p++;
        n--;
        if (s->line.buf) {
            char *l = s->line.buf;
            if (s->line.len && l[s->line.len - 1] == '\r')
                l[--s->line.len] = 0;
            if (s->raw) {
                if (*l)
                    s->cb(l, s->user);
            } else if (strncmp(l, "data:", 5) == 0) {
                s->cb(l[5] == ' ' ? l + 6 : l + 5, s->user);
            }
        }
        s->line.len = 0;
        if (s->line.buf)
            s->line.buf[0] = 0;
    }
}

/* Empfangene Bytes einspeisen, Chunked-Kodierung schrittweise aufloesen */
static void sse_feed(Sse *s, const char *p, long n)
{
    if (!s->chunked) {
        sse_payload(s, p, n);
        return;
    }
    while (n > 0 && s->state != CH_DONE) {
        switch (s->state) {
        case CH_SIZE:
            if (*p == '\n') {
                s->size[s->sizelen] = 0;
                s->remain = strtoul(s->size, NULL, 16);
                s->sizelen = 0;
                s->state = s->remain ? CH_DATA : CH_DONE;
            } else if (*p != '\r' && s->sizelen < (int)sizeof(s->size) - 1) {
                s->size[s->sizelen++] = *p;
            }
            p++;
            n--;
            break;
        case CH_DATA: {
            long k = (long)s->remain < n ? (long)s->remain : n;
            sse_payload(s, p, k);
            p += k;
            n -= k;
            s->remain -= k;
            if (!s->remain)
                s->state = CH_CRLF;
            break;
        }
        case CH_CRLF:
            if (*p == '\n')
                s->state = CH_SIZE;
            p++;
            n--;
            break;
        }
    }
}

static int post_stream(const char *url, const char *extra_headers,
                       const char *body, unsigned long body_len,
                       SseCallback cb, void *user, int raw,
                       HttpResponse *resp, char *err, int errlen);

int https_post_sse(const char *url, const char *extra_headers,
                   const char *body, unsigned long body_len,
                   SseCallback cb, void *user,
                   HttpResponse *resp, char *err, int errlen)
{
    return post_stream(url, extra_headers, body, body_len, cb, user, 0, resp, err, errlen);
}

int https_post_lines(const char *url, const char *extra_headers,
                     const char *body, unsigned long body_len,
                     SseCallback cb, void *user,
                     HttpResponse *resp, char *err, int errlen)
{
    return post_stream(url, extra_headers, body, body_len, cb, user, 1, resp, err, errlen);
}

static int post_stream(const char *url, const char *extra_headers,
                       const char *body, unsigned long body_len,
                       SseCallback cb, void *user, int raw,
                       HttpResponse *resp, char *err, int errlen)
{
    Conn c;
    char *buf, *hend = NULL;
    unsigned long len = 0, cap = 65536;
    int ok = 0;
    Sse s;

    memset(resp, 0, sizeof(*resp));
    memset(&s, 0, sizeof(s));
    sb_init(&s.line);
    s.cb = cb;
    s.user = user;
    s.raw = raw;

    if (!conn_request(&c, url, extra_headers, body, body_len, err, errlen))
        return 0;
    if (!(buf = malloc(cap))) {
        set_err(err, errlen, "Kein Speicher");
        goto out;
    }

    /* Bis zum Ende der Header lesen */
    while (!hend) {
        int n;
        if (SetSignal(0, 0) & SIGBREAKF_CTRL_C) {
            set_err(err, errlen, "Abgebrochen (CTRL-C)");
            goto out;
        }
        if (len + 4097 > cap) {
            set_err(err, errlen, "HTTP-Header zu gross");
            goto out;
        }
        n = conn_read(&c, buf + len, 4096);
        if (n <= 0) {
            set_err(err, errlen, "Verbindung vor Ende der Header geschlossen");
            goto out;
        }
        len += n;
        buf[len] = 0;
        hend = strstr(buf, "\r\n\r\n");
    }
    if (strncmp(buf, "HTTP/1.", 7) != 0) {
        set_err(err, errlen, "Ungueltige HTTP-Antwort");
        goto out;
    }
    resp->status = atoi(buf + 9);

    if (resp->status != 200) {
        /* Fehler: ganze Antwort lesen, damit der Aufrufer die Meldung auswerten kann */
        if (read_all(&c, &buf, &len, &cap, err, errlen) &&
            finish_body(buf, len, resp, err, errlen)) {
            ok = 1;
            buf = NULL;
        }
        goto out;
    }

    *hend = 0;
    s.chunked = header_has(buf, "transfer-encoding", "chunked");
    hend += 4;
    sse_feed(&s, hend, len - (hend - buf));

    for (;;) {
        int n;
        if (SetSignal(0, 0) & SIGBREAKF_CTRL_C) {
            set_err(err, errlen, "Abgebrochen (CTRL-C)");
            goto out;
        }
        n = conn_read(&c, buf, 4096);
        if (n <= 0)
            break;
        sse_feed(&s, buf, n);
        if (s.state == CH_DONE)
            break;
    }
    if (raw && s.line.len && s.line.buf)
        cb(s.line.buf, user);           /* letzte Zeile ohne Zeilenumbruch */
    ok = 1;

out:
    sb_free(&s.line);
    free(buf);
    conn_close(&c);
    return ok;
}

void http_response_free(HttpResponse *resp)
{
    free(resp->body);
    resp->body = NULL;
    resp->body_len = 0;
}
