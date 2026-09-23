#include <stdlib.h>
#include <string.h>

#include "json.h"

/* ---------- Parser ---------- */

typedef struct {
    const char *p;
    int err;
} JParser;

static JNode *parse_value(JParser *jp);

static void skip_ws(JParser *jp)
{
    while (*jp->p == ' ' || *jp->p == '\t' || *jp->p == '\n' || *jp->p == '\r')
        jp->p++;
}

static JNode *new_node(int type)
{
    JNode *n = calloc(1, sizeof(JNode));
    if (n)
        n->type = type;
    return n;
}

static int hexval(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int read_hex4(const char *s, unsigned long *out)
{
    unsigned long v = 0;
    int i, h;
    for (i = 0; i < 4; i++) {
        if ((h = hexval(s[i])) < 0)
            return 0;
        v = (v << 4) | h;
    }
    *out = v;
    return 1;
}

static char *put_utf8(char *d, unsigned long cp)
{
    if (cp < 0x80) {
        *d++ = (char)cp;
    } else if (cp < 0x800) {
        *d++ = (char)(0xC0 | (cp >> 6));
        *d++ = (char)(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        *d++ = (char)(0xE0 | (cp >> 12));
        *d++ = (char)(0x80 | ((cp >> 6) & 0x3F));
        *d++ = (char)(0x80 | (cp & 0x3F));
    } else {
        *d++ = (char)(0xF0 | (cp >> 18));
        *d++ = (char)(0x80 | ((cp >> 12) & 0x3F));
        *d++ = (char)(0x80 | ((cp >> 6) & 0x3F));
        *d++ = (char)(0x80 | (cp & 0x3F));
    }
    return d;
}

/* Erwartet jp->p auf '"'. Liefert neu allozierten, unescapten UTF-8-String. */
static char *parse_string(JParser *jp)
{
    const char *s = jp->p + 1;
    const char *e = s;
    char *out, *d;

    /* Ende suchen; das Ergebnis ist nie laenger als die Quelle */
    while (*e && *e != '"') {
        if (*e == '\\' && e[1])
            e++;
        e++;
    }
    if (*e != '"') {
        jp->err = 1;
        return NULL;
    }
    out = malloc((e - s) + 1);
    if (!out) {
        jp->err = 1;
        return NULL;
    }
    d = out;
    while (s < e) {
        if (*s != '\\') {
            *d++ = *s++;
            continue;
        }
        s++;
        switch (*s) {
        case 'n': *d++ = '\n'; s++; break;
        case 't': *d++ = '\t'; s++; break;
        case 'r': *d++ = '\r'; s++; break;
        case 'b': *d++ = '\b'; s++; break;
        case 'f': *d++ = '\f'; s++; break;
        case 'u': {
            unsigned long cp, lo;
            if (!read_hex4(s + 1, &cp)) {
                jp->err = 1;
                free(out);
                return NULL;
            }
            s += 5;
            if (cp >= 0xD800 && cp <= 0xDBFF && s[0] == '\\' && s[1] == 'u' &&
                read_hex4(s + 2, &lo) && lo >= 0xDC00 && lo <= 0xDFFF) {
                cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                s += 6;
            }
            d = put_utf8(d, cp);
            break;
        }
        default: *d++ = *s++; break;   /* \" \\ \/ */
        }
    }
    *d = 0;
    jp->p = e + 1;
    return out;
}

static JNode *parse_container(JParser *jp, int is_obj)
{
    JNode *n = new_node(is_obj ? J_OBJ : J_ARR);
    JNode **tail;
    char close = is_obj ? '}' : ']';

    if (!n) {
        jp->err = 1;
        return NULL;
    }
    tail = &n->child;
    jp->p++;
    skip_ws(jp);
    if (*jp->p == close) {
        jp->p++;
        return n;
    }
    for (;;) {
        char *key = NULL;
        JNode *v;

        skip_ws(jp);
        if (is_obj) {
            if (*jp->p != '"' || !(key = parse_string(jp)))
                break;
            skip_ws(jp);
            if (*jp->p != ':') {
                free(key);
                break;
            }
            jp->p++;
        }
        v = parse_value(jp);
        if (!v) {
            free(key);
            break;
        }
        v->key = key;
        *tail = v;
        tail = &v->next;
        skip_ws(jp);
        if (*jp->p == ',') {
            jp->p++;
            continue;
        }
        if (*jp->p == close) {
            jp->p++;
            return n;
        }
        break;
    }
    jp->err = 1;
    json_free(n);
    return NULL;
}

static JNode *parse_value(JParser *jp)
{
    JNode *n;

    skip_ws(jp);
    switch (*jp->p) {
    case '{': return parse_container(jp, 1);
    case '[': return parse_container(jp, 0);
    case '"':
        n = new_node(J_STR);
        if (!n || !(n->str = parse_string(jp))) {
            free(n);
            jp->err = 1;
            return NULL;
        }
        return n;
    case 't':
        if (strncmp(jp->p, "true", 4) == 0) { jp->p += 4; return new_node(J_TRUE); }
        break;
    case 'f':
        if (strncmp(jp->p, "false", 5) == 0) { jp->p += 5; return new_node(J_FALSE); }
        break;
    case 'n':
        if (strncmp(jp->p, "null", 4) == 0) { jp->p += 4; return new_node(J_NULL); }
        break;
    default:
        if (*jp->p == '-' || (*jp->p >= '0' && *jp->p <= '9')) {
            const char *s = jp->p;
            while (*jp->p && strchr("+-0123456789.eE", *jp->p))
                jp->p++;
            n = new_node(J_NUM);
            if (!n || !(n->str = malloc(jp->p - s + 1))) {
                free(n);
                jp->err = 1;
                return NULL;
            }
            memcpy(n->str, s, jp->p - s);
            n->str[jp->p - s] = 0;
            return n;
        }
        break;
    }
    jp->err = 1;
    return NULL;
}

JNode *json_parse(const char *text)
{
    JParser jp;
    JNode *n;

    jp.p = text;
    jp.err = 0;
    n = parse_value(&jp);
    if (jp.err) {
        json_free(n);
        return NULL;
    }
    return n;
}

void json_free(JNode *n)
{
    while (n) {
        JNode *next = n->next;
        json_free(n->child);
        free(n->key);
        free(n->str);
        free(n);
        n = next;
    }
}

JNode *json_get(const JNode *obj, const char *key)
{
    JNode *c;
    if (!obj || obj->type != J_OBJ)
        return NULL;
    for (c = obj->child; c; c = c->next)
        if (c->key && strcmp(c->key, key) == 0)
            return c;
    return NULL;
}

JNode *json_index(const JNode *arr, int idx)
{
    JNode *c;
    if (!arr || arr->type != J_ARR)
        return NULL;
    for (c = arr->child; c && idx > 0; c = c->next)
        idx--;
    return c;
}

JNode *json_path(const JNode *n, const char *path)
{
    char part[64];

    while (n && *path) {
        int i = 0;
        while (*path && *path != '.' && i < (int)sizeof(part) - 1)
            part[i++] = *path++;
        part[i] = 0;
        if (*path == '.')
            path++;
        if (n->type == J_ARR)
            n = json_index(n, atoi(part));
        else
            n = json_get(n, part);
    }
    return (JNode *)n;
}

const char *json_str(const JNode *n)
{
    return (n && n->type == J_STR) ? n->str : NULL;
}

/* ---------- String-Puffer ---------- */

void sb_init(StrBuf *sb)
{
    sb->buf = NULL;
    sb->len = sb->cap = 0;
    sb->oom = 0;
}

void sb_free(StrBuf *sb)
{
    free(sb->buf);
    sb_init(sb);
}

void sb_addn(StrBuf *sb, const char *s, unsigned long n)
{
    if (sb->oom)
        return;
    if (sb->len + n + 1 > sb->cap) {
        unsigned long ncap = sb->cap ? sb->cap : 1024;
        char *nb;
        while (ncap < sb->len + n + 1)
            ncap *= 2;
        nb = realloc(sb->buf, ncap);
        if (!nb) {
            sb->oom = 1;
            return;
        }
        sb->buf = nb;
        sb->cap = ncap;
    }
    memcpy(sb->buf + sb->len, s, n);
    sb->len += n;
    sb->buf[sb->len] = 0;
}

void sb_add(StrBuf *sb, const char *s)
{
    sb_addn(sb, s, strlen(s));
}

void sb_add_jstr(StrBuf *sb, const char *s, int latin1)
{
    static const char hex[] = "0123456789abcdef";
    const unsigned char *p = (const unsigned char *)s;
    char tmp[8];

    sb_add(sb, "\"");
    for (; *p; p++) {
        switch (*p) {
        case '"':  sb_add(sb, "\\\""); break;
        case '\\': sb_add(sb, "\\\\"); break;
        case '\n': sb_add(sb, "\\n"); break;
        case '\r': sb_add(sb, "\\r"); break;
        case '\t': sb_add(sb, "\\t"); break;
        default:
            if (*p < 0x20) {
                tmp[0] = '\\'; tmp[1] = 'u'; tmp[2] = '0'; tmp[3] = '0';
                tmp[4] = hex[*p >> 4]; tmp[5] = hex[*p & 15];
                sb_addn(sb, tmp, 6);
            } else if (*p >= 0x80 && latin1) {
                tmp[0] = (char)(0xC0 | (*p >> 6));
                tmp[1] = (char)(0x80 | (*p & 0x3F));
                sb_addn(sb, tmp, 2);
            } else {
                sb_addn(sb, (const char *)p, 1);
            }
        }
    }
    sb_add(sb, "\"");
}

void sb_add_json(StrBuf *sb, const JNode *n)
{
    const JNode *c;

    switch (n->type) {
    case J_NULL:  sb_add(sb, "null"); break;
    case J_FALSE: sb_add(sb, "false"); break;
    case J_TRUE:  sb_add(sb, "true"); break;
    case J_NUM:   sb_add(sb, n->str); break;
    case J_STR:   sb_add_jstr(sb, n->str, 0); break;
    case J_ARR:
    case J_OBJ:
        sb_add(sb, n->type == J_ARR ? "[" : "{");
        for (c = n->child; c; c = c->next) {
            if (c != n->child)
                sb_add(sb, ",");
            if (n->type == J_OBJ) {
                sb_add_jstr(sb, c->key ? c->key : "", 0);
                sb_add(sb, ":");
            }
            sb_add_json(sb, c);
        }
        sb_add(sb, n->type == J_ARR ? "]" : "}");
        break;
    }
}

void utf8_to_latin1(char *s)
{
    unsigned char *r = (unsigned char *)s, *w = (unsigned char *)s;

    while (*r) {
        unsigned long cp;
        int extra;

        if (*r < 0x80) {
            *w++ = *r++;
            continue;
        }
        if ((*r & 0xE0) == 0xC0) { cp = *r & 0x1F; extra = 1; }
        else if ((*r & 0xF0) == 0xE0) { cp = *r & 0x0F; extra = 2; }
        else if ((*r & 0xF8) == 0xF0) { cp = *r & 0x07; extra = 3; }
        else { *w++ = '?'; r++; continue; }
        r++;
        while (extra-- > 0 && (*r & 0xC0) == 0x80)
            cp = (cp << 6) | (*r++ & 0x3F);

        /* haeufige Typografie auf ASCII abbilden */
        switch (cp) {
        case 0x2018: case 0x2019: cp = '\''; break;
        case 0x201C: case 0x201D: case 0x201E: cp = '"'; break;
        case 0x2013: case 0x2014: cp = '-'; break;
        case 0x2026: cp = '.'; break;
        case 0x2022: cp = '*'; break;
        case 0x00A0: cp = ' '; break;
        }
        *w++ = cp <= 0xFF ? (unsigned char)cp : '?';
    }
    *w = 0;
}
