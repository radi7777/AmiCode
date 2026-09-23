#ifndef AMICODE_SHELL_H
#define AMICODE_SHELL_H

#include <dos/dos.h>

/* Ergebnis eines Befehls */
#define SHELL_OK         0
#define SHELL_BREAK      1  /* Benutzer hat CTRL-C gedrueckt */
#define SHELL_TIMEOUT    2  /* Zeitlimit ueberschritten */

/* Fuehrt cmd in einem eigenen Prozess aus (Ein-/Ausgabe auf in/out).
   Waehrend der Befehl laeuft, werden CTRL-C und das Zeitlimit (Sekunden, 0 = keins)
   ueberwacht und als Break an den Befehl weitergegeben.
   Liefert den Returncode (-1 = nicht startbar), *status = SHELL_*. */
long shell_run(const char *cmd, BPTR in, BPTR out, long stack, long timeout, int *status);

#endif
