#ifndef AMICODE_NET_H
#define AMICODE_NET_H

/* HTTPS ueber bsdsocket.library + AmiSSL 5. */

typedef struct {
    int status;             /* HTTP-Statuscode, 0 bei Verbindungsfehler */
    char *body;             /* NUL-terminiert, mit malloc() alloziert */
    unsigned long body_len;
    char location[512];     /* Location-Header (Weiterleitung) */
    char content_type[100]; /* Content-Type-Header */
} HttpResponse;

int net_open(char *err, int errlen);    /* 0 = Fehler, err beschrieben */
void net_close(void);

/* POST an eine http(s)://-URL. extra_headers: "Name: Wert\r\n..." oder NULL.
   Liefert 0 bei Netzwerk-/TLS-Fehler (err beschrieben), sonst 1. */
int https_post(const char *url, const char *extra_headers,
               const char *body, unsigned long body_len,
               HttpResponse *resp, char *err, int errlen);

/* GET mit Weiterleitungen (bis zu 5). final_url: tatsaechlich geladene Adresse (oder NULL) */
int http_fetch(const char *url, const char *extra_headers, HttpResponse *resp,
               char *final_url, int final_len, char *err, int errlen);

/* GET (http:// oder https://), Antwort komplett in resp */
int https_get(const char *url, const char *extra_headers,
              HttpResponse *resp, char *err, int errlen);

/* Wird fuer jede "data:"-Zeile eines Server-Sent-Events-Streams aufgerufen */
typedef void (*SseCallback)(const char *data, void *user);

/* POST mit gestreamter Antwort. Bei Status 200 geht jede data:-Zeile sofort an cb;
   bei anderem Status steht die komplette Antwort in resp->body. 0 = Netzwerkfehler. */
int https_post_sse(const char *url, const char *extra_headers,
                   const char *body, unsigned long body_len,
                   SseCallback cb, void *user,
                   HttpResponse *resp, char *err, int errlen);

/* Wie https_post_sse, aber jede Zeile geht an cb (NDJSON, z. B. Ollama /api/chat) */
int https_post_lines(const char *url, const char *extra_headers,
                     const char *body, unsigned long body_len,
                     SseCallback cb, void *user,
                     HttpResponse *resp, char *err, int errlen);

void http_response_free(HttpResponse *resp);

#endif
