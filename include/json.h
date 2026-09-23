#ifndef AMICODE_JSON_H
#define AMICODE_JSON_H

/* Minimaler JSON-DOM fuer AmiCode. Zahlen bleiben Text (kein FPU-Zwang). */

enum { J_NULL, J_FALSE, J_TRUE, J_NUM, J_STR, J_ARR, J_OBJ };

typedef struct JNode {
    int type;
    char *key;              /* Schluessel, wenn Kind eines Objekts */
    char *str;              /* J_STR: UTF-8-Text, J_NUM: Zahl als Text */
    struct JNode *child;    /* erstes Kind (J_ARR/J_OBJ) */
    struct JNode *next;     /* naechstes Geschwister */
} JNode;

/* Parst text (NUL-terminiert). Liefert NULL bei Fehler. */
JNode *json_parse(const char *text);
void json_free(JNode *n);

JNode *json_get(const JNode *obj, const char *key);
JNode *json_index(const JNode *arr, int idx);
/* Pfad wie "choices.0.message.content" */
JNode *json_path(const JNode *n, const char *path);
const char *json_str(const JNode *n);   /* NULL wenn kein String */

/* Dynamischer String-Puffer fuer das Erzeugen von JSON */
typedef struct {
    char *buf;
    unsigned long len;
    unsigned long cap;
    int oom;
} StrBuf;

void sb_init(StrBuf *sb);
void sb_free(StrBuf *sb);
void sb_add(StrBuf *sb, const char *s);
void sb_addn(StrBuf *sb, const char *s, unsigned long n);
/* Haengt s als JSON-String inkl. Anfuehrungszeichen an.
   latin1 != 0: Eingabe ist ISO-8859-1 und wird nach UTF-8 gewandelt. */
void sb_add_jstr(StrBuf *sb, const char *s, int latin1);

/* Knoten (inkl. Kinder) als JSON anhaengen; Strings sind bereits UTF-8 */
void sb_add_json(StrBuf *sb, const JNode *n);

/* UTF-8 -> ISO-8859-1 fuer die Amiga-Konsole (in place, nicht darstellbar -> '?') */
void utf8_to_latin1(char *s);

#endif
