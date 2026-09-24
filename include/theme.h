#ifndef AMICODE_THEME_H
#define AMICODE_THEME_H

/* Farben und Schriften der IDE (Light/Dark).

   Farben sind MUI-Pen-Specs, wie sie Poppen.mui liefert:
   "rRRRRRRRR,GGGGGGGG,BBBBBBBB" (RGB), "m5" (MUI-Pen), "p3" (Farbregister).

   MUI 3.8 kann Schriftfarben nicht pro Objekt setzen. Deshalb:
   - TextEditor und BetterString: Unterklassen beantworten MUIM_GetConfigItem
     mit den Theme-Farben (die Klassen lesen ihre Einstellungen in MUIM_Setup,
     also greift ein Wechsel beim naechsten Oeffnen des Fensters)
   - NList/NListtree: ebenso (MUIA_NList_ListPen gilt nur bis zum naechsten Setup)
   - Text-Objekte: Unterklasse setzt "\33P[pen]" vor den Inhalt (in PreParse
     wirkt \33P unter MUI 3.8 nicht)
   - Knoepfe: nur der Hintergrund, ihre Schrift bleibt MUI-schwarz
   Rahmentitel und Registerreiter bleiben im MUI-Stil.
   Ein leerer Hintergrund bedeutet: MUI-Standard, nichts setzen. */

#include <exec/types.h>
#include <intuition/classusr.h>

#include "config.h"

enum { TA_EDITOR, TA_OUTPUT, TA_LOG, TA_LIST, TA_INPUT, TA_STATUS, TA_WINDOW, TA_BUTTON, TA_COUNT };
enum { TS_KEYWORD, TS_COMMENT, TS_STRING, TS_PREPROC, TS_CURSOR, TS_MARK, TS_COUNT };

#define THEME_SPEC  32      /* sizeof(struct MUI_PenSpec) */
#define THEME_FONT  48

typedef struct {
    char bg[TA_COUNT][THEME_SPEC];
    char fg[TA_COUNT][THEME_SPEC];
    char syn[TS_COUNT][THEME_SPEC];
} ThemeSet;

typedef struct {
    ThemeSet set[2];                /* 0 = Light, 1 = Dark */
    int dark;
    char font_fixed[THEME_FONT];    /* "xen/8"; leer = MUI-Standard */
    char font_text[THEME_FONT];
} Theme;

extern Theme theme;

void theme_defaults(ThemeSet *s, int dark);
void theme_load(const Config *cfg);             /* Farben/Schriften aus [theme], Modus aus ENV: */
void theme_store(Config *cfg);                  /* Farben/Schriften nach [theme] (malloc!) */
void theme_save_mode(void);                     /* nur Light/Dark nach ENV:/ENVARC: (ohne malloc) */
const ThemeSet *theme_cur(void);
const char *theme_image(int area);              /* Hintergrund als Image-Spec "2:...", NULL = MUI-Standard */
/* Hintergrund setzen; ohne Theme-Farbe den MUI-Standard def (MUII_...) */
void theme_background(Object *obj, int area, ULONG def);

/* Eigene Klassen: vor dem Fensterbau anlegen, nach MUI_DisposeObject(app) freigeben */
int theme_classes_init(void);
void theme_classes_free(void);
/* TextEditor fuer TA_EDITOR/TA_OUTPUT, BetterString fuer TA_INPUT; NULL wenn die
   Klasse fehlt. Tags wie bei MUI_NewObject, mit TAG_DONE abschliessen. */
Object *theme_texteditor(int area, ULONG tag1, ...);
Object *theme_betterstring(ULONG tag1, ...);
Object *theme_nlist(int area, ULONG tag1, ...);
Object *theme_nlisttree(int area, ULONG tag1, ...);
/* Text mit Schriftfarbe des Bereichs; MUIA_Text_Contents wie gewohnt setzen */
Object *theme_text(int area, ULONG tag1, ...);
#define MUIM_ThemeText_Refresh  0xAC0DE001      /* Inhalt mit aktuellem Pen neu setzen */

/* Listen in anderen Fenstern (normale NList) einfaerben: nach dem Oeffnen aufrufen */
void theme_list(Object *list, int area);

/* Pens fuer Text-Objekte: nach dem Oeffnen des Fensters holen, vor dem
   Schliessen freigeben. obj: irgendein Objekt im offenen Fenster. */
void theme_pens_obtain(Object *obj);
void theme_pens_release(Object *obj);

/* Schriften fuer Objekte mit MUIA_Font (NULL = MUI-Standard); bleiben offen bis theme_fonts_close */
struct TextFont *theme_font(int fixed);
void theme_fonts_close(void);

#endif
