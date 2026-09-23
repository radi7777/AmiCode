/* Internetzugriff fuer den Agenten: Websuche (DuckDuckGo lite, ohne Key) und
   Seiten als Text laden. Laeuft direkt auf dem Amiga ueber bsdsocket/AmiSSL,
   funktioniert daher mit jedem Modell-Anbieter. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "web.h"
#include "net.h"

#define SEARCH_URL      "https://lite.duckduckgo.com/lite/?q="
#define MAX_RESULTS     8

static int hexval(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static void url_encode(StrBuf *out, const char *s)
{
    static const char hex[] = "0123456789ABCDEF";
    char buf[4];

    for (; *s; s++) {
        unsigned char c = *s;
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~') {
            sb_addn(out, (const char *)&c, 1);
        } else if (c == ' ') {
            sb_add(out, "+");
        } else {
            buf[0] = '%';
            buf[1] = hex[c >> 4];
            buf[2] = hex[c & 15];
            sb_addn(out, buf, 3);
        }
    }
}

/* %XX dekodieren (in place), bis end oder '&' */
static void url_decode(char *s)
{
    char *r = s, *w = s;

    while (*r && *r != '&') {
        if (*r == '%' && hexval(r[1]) >= 0 && hexval(r[2]) >= 0) {
            *w++ = (char)(hexval(r[1]) * 16 + hexval(r[2]));
            r += 3;
        } else if (*r == '+') {
            *w++ = ' ';
            r++;
        } else {
            *w++ = *r++;
        }
    }
    *w = 0;
}

/* HTML-Entity bei p ("&...;") auswerten; liefert Zeichen (Latin-1) und Laenge */
static int entity(const char *p, int *len)
{
    static const struct { const char *name; int ch; } named[] = {
        { "amp", '&' }, { "lt", '<' }, { "gt", '>' }, { "quot", '"' }, { "apos", '\'' },
        { "nbsp", ' ' }, { "auml", 0xE4 }, { "ouml", 0xF6 }, { "uuml", 0xFC }, { "Auml", 0xC4 },
        { "Ouml", 0xD6 }, { "Uuml", 0xDC }, { "szlig", 0xDF }, { "copy", 0xA9 }, { "reg", 0xAE },
        { "eacute", 0xE9 }, { "egrave", 0xE8 }, { "agrave", 0xE0 }, { "ndash", '-' },
        { "mdash", '-' }, { "hellip", '.' }, { "laquo", 0xAB }, { "raquo", 0xBB },
        { "lsquo", '\'' }, { "rsquo", '\'' }, { "ldquo", '"' }, { "rdquo", '"' }, { NULL, 0 }
    };
    const char *e = strchr(p, ';');
    int i, n;

    if (!e || e - p > 10) {
        *len = 1;
        return '&';
    }
    *len = e - p + 1;
    if (p[1] == '#') {
        long v = (p[2] == 'x' || p[2] == 'X') ? strtol(p + 3, NULL, 16) : strtol(p + 2, NULL, 10);
        if (v == 0x2019 || v == 0x2018) return '\'';
        if (v == 0x201C || v == 0x201D) return '"';
        if (v == 0x2013 || v == 0x2014) return '-';
        return v > 0 && v < 256 ? (int)v : '?';
    }
    n = e - p - 1;
    for (i = 0; named[i].name; i++)
        if ((int)strlen(named[i].name) == n && strncmp(p + 1, named[i].name, n) == 0)
            return named[i].ch;
    *len = 1;
    return '&';
}

/* Text zwischen p und end ohne Tags, Entities aufgeloest, Leerraum zusammengefasst */
static void plain_text(StrBuf *out, const char *p, const char *end)
{
    int space = 0;

    while (p < end) {
        if (*p == '<') {
            const char *gt = strchr(p, '>');
            if (!gt || gt >= end)
                break;
            p = gt + 1;
            continue;
        }
        if (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t') {
            space = 1;
            p++;
            continue;
        }
        if (space && out->len)
            sb_add(out, " ");
        space = 0;
        if (*p == '&') {
            int len;
            char c = (char)entity(p, &len);
            sb_addn(out, &c, 1);
            p += len;
        } else {
            sb_addn(out, p, 1);
            p++;
        }
    }
}

static int is_utf8_page(const HttpResponse *r)
{
    return !strstr(r->content_type, "8859") && !strstr(r->content_type, "latin");
}

int web_search(const char *query, StrBuf *out, char *err, int errlen)
{
    StrBuf url;
    HttpResponse resp;
    char *p;
    int n = 0;

    sb_init(&url);
    sb_add(&url, SEARCH_URL);
    url_encode(&url, query);
    if (url.oom || !http_fetch(url.buf, NULL, &resp, NULL, 0, err, errlen)) {
        sb_free(&url);
        return -1;
    }
    sb_free(&url);
    if (resp.status != 200 || !resp.body) {
        snprintf(err, errlen, "Suchdienst antwortet mit HTTP %d", resp.status);
        http_response_free(&resp);
        return -1;
    }
    utf8_to_latin1(resp.body);

    /* Treffer: <a ... href="//duckduckgo.com/l/?uddg=URL&..." class='result-link'>Titel</a>
       danach <td class='result-snippet'>Kurztext</td> */
    p = resp.body;
    while (n < MAX_RESULTS && (p = strstr(p, "class='result-link'"))) {
        char *href = p, *u, *gt, *aend, *snip, *send;
        StrBuf target;

        /* href steht vor dem class-Attribut im selben Tag */
        while (href > resp.body && *href != '<')
            href--;
        u = strstr(href, "uddg=");
        gt = strchr(p, '>');
        aend = gt ? strstr(gt, "</a>") : NULL;
        if (!gt || !aend) {
            p += 19;
            continue;
        }
        sb_init(&target);
        if (u && u < p) {
            char tmp[600];
            const char *q = u + 5;
            int k = 0;
            while (*q && *q != '&' && *q != '"' && *q != '\'' && k < (int)sizeof(tmp) - 1)
                tmp[k++] = *q++;
            tmp[k] = 0;
            url_decode(tmp);
            sb_add(&target, tmp);
        }
        n++;
        {
            char num[12];
            snprintf(num, sizeof(num), "%d. ", n);
            sb_add(out, num);
        }
        plain_text(out, gt + 1, aend);
        sb_add(out, "\n   ");
        sb_add(out, target.buf ? target.buf : "(ohne Adresse)");
        sb_add(out, "\n");
        sb_free(&target);

        snip = strstr(aend, "result-snippet");
        send = snip ? strstr(snip, "</td>") : NULL;
        if (snip && send && (!strstr(aend, "class='result-link'") || snip < strstr(aend, "class='result-link'"))) {
            char *sgt = strchr(snip, '>');
            if (sgt && sgt < send) {
                sb_add(out, "   ");
                plain_text(out, sgt + 1, send);
                sb_add(out, "\n");
            }
        }
        sb_add(out, "\n");
        p = aend;
    }
    http_response_free(&resp);
    if (n == 0)
        sb_add(out, "(keine Treffer)");
    return n;
}

/* HTML in lesbaren Text: Bloecke als Zeilen, Links mit Adresse, Skripte weg */
static void html_to_text(const char *html, StrBuf *out)
{
    const char *p = html;
    int space = 0, newlines = 0;

    while (*p) {
        if (*p == '<') {
            const char *gt = strchr(p, '>');
            char tag[16];
            int k = 0, closing = 0;
            const char *t = p + 1;

            if (!gt)
                break;
            if (*t == '/') {
                closing = 1;
                t++;
            }
            while (k < (int)sizeof(tag) - 1 && t < gt &&
                   ((*t >= 'a' && *t <= 'z') || (*t >= 'A' && *t <= 'Z') || (*t >= '0' && *t <= '9')))
                tag[k++] = (*t >= 'A' && *t <= 'Z') ? *t + 32 : *t, t++;
            tag[k] = 0;

            /* Inhalt von script/style/noscript/svg ueberspringen */
            if (!closing && (!strcmp(tag, "script") || !strcmp(tag, "style") ||
                             !strcmp(tag, "noscript") || !strcmp(tag, "svg"))) {
                char endtag[20];
                const char *e;
                snprintf(endtag, sizeof(endtag), "</%s", tag);
                e = strstr(gt, endtag);
                if (!e) {
                    /* Grossschreibung */
                    int j;
                    for (j = 2; endtag[j]; j++)
                        endtag[j] -= 32;
                    e = strstr(gt, endtag);
                }
                p = e ? strchr(e, '>') : NULL;
                if (!p)
                    break;
                p++;
                continue;
            }
            if (!strncmp(p, "<!--", 4)) {
                const char *e = strstr(p, "-->");
                p = e ? e + 3 : p + 4;
                continue;
            }
            /* Blockelemente -> Zeilenumbruch */
            if (!strcmp(tag, "br") || !strcmp(tag, "p") || !strcmp(tag, "div") ||
                !strcmp(tag, "li") || !strcmp(tag, "tr") || !strcmp(tag, "pre") ||
                !strcmp(tag, "table") || !strcmp(tag, "ul") || !strcmp(tag, "ol") ||
                (tag[0] == 'h' && tag[1] >= '1' && tag[1] <= '6' && !tag[2])) {
                if (newlines < 2) {
                    sb_add(out, "\n");
                    newlines++;
                }
                space = 0;
                if (!closing && !strcmp(tag, "li"))
                    sb_add(out, "- ");
                if (!closing && tag[0] == 'h' && tag[1] >= '1' && tag[1] <= '6')
                    sb_add(out, "## ");
            }
            /* Links: Adresse hinter dem Text merken */
            if (!closing && !strcmp(tag, "a")) {
                const char *h = strstr(p, "href=");
                if (h && h < gt) {
                    char q = h[5];
                    const char *s = h + 6, *e;
                    if (q == '"' || q == '\'') {
                        e = strchr(s, q);
                        if (e && e < gt && (!strncmp(s, "http://", 7) || !strncmp(s, "https://", 8))) {
                            /* nach dem Linktext anhaengen */
                            const char *aend = strstr(gt, "</a>");
                            if (!aend)
                                aend = strstr(gt, "</A>");
                            if (aend) {
                                plain_text(out, gt + 1, aend);
                                sb_add(out, " <");
                                sb_addn(out, s, e - s);
                                sb_add(out, ">");
                                p = aend + 4;
                                space = 0;
                                newlines = 0;
                                continue;
                            }
                        }
                    }
                }
            }
            p = gt + 1;
            continue;
        }
        if (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
            space = 1;
            p++;
            continue;
        }
        if (space && newlines == 0 && out->len)
            sb_add(out, " ");
        space = 0;
        newlines = 0;
        if (*p == '&') {
            int len;
            char c = (char)entity(p, &len);
            sb_addn(out, &c, 1);
            p += len;
        } else {
            sb_addn(out, p, 1);
            p++;
        }
    }
}

int web_fetch(const char *url, long offset, long maxlen, StrBuf *out, char *err, int errlen)
{
    HttpResponse resp;
    StrBuf text;
    char final[600];
    const char *body;
    long len;

    if (strncmp(url, "http://", 7) != 0 && strncmp(url, "https://", 8) != 0) {
        snprintf(err, errlen, "Nur http:// und https:// werden unterstuetzt");
        return 0;
    }
    if (!http_fetch(url, "Accept: text/html,text/plain,application/json;q=0.9,*/*;q=0.5\r\n",
                    &resp, final, sizeof(final), err, errlen))
        return 0;
    if (resp.status != 200 || !resp.body) {
        snprintf(err, errlen, "HTTP %d", resp.status);
        http_response_free(&resp);
        return 0;
    }
    if (resp.content_type[0] && !strstr(resp.content_type, "text") &&
        !strstr(resp.content_type, "json") && !strstr(resp.content_type, "xml")) {
        snprintf(err, errlen, "Keine Textseite (%s, %lu Bytes)", resp.content_type, resp.body_len);
        http_response_free(&resp);
        return 0;
    }
    if (memchr(resp.body, 0, resp.body_len)) {
        snprintf(err, errlen, "Binaerdaten (%lu Bytes)", resp.body_len);
        http_response_free(&resp);
        return 0;
    }
    if (is_utf8_page(&resp))
        utf8_to_latin1(resp.body);

    sb_init(&text);
    if (strstr(resp.content_type, "html") || (!resp.content_type[0] && strstr(resp.body, "<html")))
        html_to_text(resp.body, &text);
    else
        sb_add(&text, resp.body);
    http_response_free(&resp);

    body = text.buf ? text.buf : "";
    len = text.len;
    {
        char head[700];
        snprintf(head, sizeof(head), "Adresse: %s (%ld Zeichen Text)\n\n", final, len);
        sb_add(out, head);
    }
    if (offset > len)
        offset = len;
    if (len - offset > maxlen) {
        char tail[100];
        sb_addn(out, body + offset, maxlen);
        snprintf(tail, sizeof(tail), "\n[... gekuerzt; mit offset=%ld weiterlesen]", offset + maxlen);
        sb_add(out, tail);
    } else {
        sb_add(out, body + offset);
    }
    sb_free(&text);
    return 1;
}
