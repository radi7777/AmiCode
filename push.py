#!/usr/bin/env python3
"""Schiebt Dateien vom Mac auf den Amiga und ruft optional einen Befehl auf.

Ohne das hier muesste jede Quelltextaenderung als Text durch den
MCP-Aufruf wandern. So liest das Skript die Datei direkt von der Platte
und spricht das amimcp-Protokoll selbst (Client aus dem amimcp-Projekt).

    python3 push.py build/amicode
    python3 push.py --run "Stuff:AmiCode/amicode Hallo"
    python3 push.py --dest RAM:AmiCode build/amicode

Umgebungsvariablen:
    AMIGA_HOST    IP oder Name des Amiga (Pflicht)
    AMIGA_TOKEN   Token des amiagent (Pflicht)
    AMIGA_PORT    Port des amiagent (Standard 7846)
    AMIMCP_PATH   Verzeichnis mit amiga.py aus dem amimcp-Projekt
    AMIGA_DEST    Zielverzeichnis auf dem Amiga (Standard Stuff:AmiCode)
"""

import os
import sys

HOST = os.environ.get("AMIGA_HOST", "")
PORT = int(os.environ.get("AMIGA_PORT", "7846"))
TOKEN = os.environ.get("AMIGA_TOKEN", "")
DEST = os.environ.get("AMIGA_DEST", "Stuff:AmiCode")

if os.environ.get("AMIMCP_PATH"):
    sys.path.insert(0, os.environ["AMIMCP_PATH"])
try:
    from amiga import Amiga  # noqa: E402
except ImportError:
    sys.exit("amiga.py nicht gefunden - AMIMCP_PATH auf das amimcp-Serververzeichnis setzen")


def main(argv):
    files = []
    run = None
    dest_dir = DEST

    i = 0
    while i < len(argv):
        if argv[i] == "--run":
            i += 1
            run = argv[i]
        elif argv[i] == "--dest":
            i += 1
            dest_dir = argv[i]
        else:
            files.append(argv[i])
        i += 1

    if not HOST or not TOKEN:
        sys.exit("AMIGA_HOST und AMIGA_TOKEN setzen (siehe Kopf von push.py)")
    a = Amiga(HOST, PORT, TOKEN)

    if files:
        a.exec_command("MakeDir >NIL: %s" % dest_dir)
        for f in files:
            dest = "%s/%s" % (dest_dir, os.path.basename(f))
            with open(f, "rb") as fh:
                data = fh.read()
            a.write_file(dest, data)
            print("%6d  ->  %s" % (len(data), dest))

    if run:
        rc, out = a.exec_command(run, timeout=300)
        print("rc=%d" % rc)
        print(out)


if __name__ == "__main__":
    main(sys.argv[1:])
