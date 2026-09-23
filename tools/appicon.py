#!/usr/bin/env python3
"""Macht aus art/icon.jpeg (Pixelschrift-Logo "Ami CodeIDE") das Workbench-Icon
build/AmiCodeIDE.info. Nach dem Muster von AmiSubsonic/appicon.py.

Das Logo wird flaechengemittelt verkleinert und in drei Helligkeitsstufen
(Hintergrund, grau, weiss) eingeteilt, damit die Pixelschrift scharf bleibt.

Zwei Fassungen in einer Datei, wie auf dem Amiga ueblich:
  klassisch  4 Workbench-Farben (fuer alte icon.library)
  GlowIcon   eigene Palette, Rand durchsichtig, angewaehlt mit Lichtkranz

    /usr/local/bin/python3 tools/appicon.py
"""

import struct

from PIL import Image

SRC = "art/icon.jpeg"
OUT = "build/AmiCodeIDE.info"
GRID_W, GRID_H = 88, 33        # Groesse der Schrift im Icon (Pixel)
BLOCK = 1
LOW, HIGH = 55, 175             # Helligkeitsstufen: Hintergrund / grau / weiss
PAD = 3                        # dunkler Rand um die Schrift
EDGE = 2                       # durchsichtiger Rand (Platz fuer den Lichtkranz)
W = GRID_W * BLOCK + 2 * PAD + 2 * EDGE
H = GRID_H * BLOCK + 2 * PAD + 2 * EDGE
NOPOS = -2147483648

WB_GREY, WB_BLACK, WB_WHITE, WB_BLUE = 0, 1, 2, 3
WB_PAL = [(0x95, 0x95, 0x95), (0, 0, 0), (255, 255, 255), (0x3B, 0x67, 0xA2)]

# GlowIcon-Palette: 0 durchsichtig, 1 Hintergrund, 2 grau, 3 weiss, 4 Lichtkranz
G_T, G_BG, G_GREY, G_WHITE, G_RING = 0, 1, 2, 3, 4
GLOW_PAL = [(0x95, 0x95, 0x95), (0x12, 0x12, 0x12), (0x8A, 0x8A, 0x8A),
            (0xF4, 0xF4, 0xF4), (0xFF, 0xE0, 0x80)]


def blocks():
    """Logo auf GRID_W x GRID_H Pixel flaechengemittelt verkleinern und in drei
    Stufen einteilen: 0 Hintergrund, 1 grau, 2 weiss. (Die zwei Zeilen und "IDE"
    haben kein gemeinsames Blockraster, deshalb kein Abtasten pro Block.)"""
    im = Image.open(SRC).convert("L")
    px = im.load()
    w, h = im.size
    xs, ys = [], []
    for y in range(0, h, 2):
        for x in range(0, w, 2):
            if px[x, y] > 70:
                xs.append(x)
                ys.append(y)
    crop = im.crop((min(xs), min(ys), max(xs) + 1, max(ys) + 1))
    small = crop.resize((GRID_W, GRID_H), Image.BOX)
    sp = small.load()
    grid = []
    for y in range(GRID_H):
        row = []
        for x in range(GRID_W):
            v = sp[x, y]
            row.append(0 if v < LOW else (2 if v > HIGH else 1))
        grid.append(row)
    return grid


def grids(logo):
    classic = [[WB_GREY] * W for _ in range(H)]
    glow = [[G_T] * W for _ in range(H)]
    for y in range(EDGE, H - EDGE):
        for x in range(EDGE, W - EDGE):
            classic[y][x] = WB_BLACK
            glow[y][x] = G_BG
    for gy in range(GRID_H):
        for gx in range(GRID_W):
            v = logo[gy][gx]
            if not v:
                continue
            for dy in range(BLOCK):
                for dx in range(BLOCK):
                    x = EDGE + PAD + gx * BLOCK + dx
                    y = EDGE + PAD + gy * BLOCK + dy
                    classic[y][x] = WB_WHITE if v == 2 else WB_GREY
                    glow[y][x] = G_WHITE if v == 2 else G_GREY
    return classic, glow


def glow_selected(gn):
    """Angewaehlt: jeder freie Punkt am Rand der Kachel leuchtet."""
    out = [row[:] for row in gn]
    for y in range(H):
        for x in range(W):
            if gn[y][x] != G_T:
                continue
            if any(gn[y + dy][x + dx] != G_T
                   for dy in (-1, 0, 1) for dx in (-1, 0, 1)
                   if 0 <= y + dy < H and 0 <= x + dx < W):
                out[y][x] = G_RING
    return out


def rle_pack(values, depth):
    """ByteRun1 ueber einen Bitstrom: 8-Bit-Steuerbytes, Werte depth Bit."""
    stream = []
    i, n = 0, len(values)
    while i < n:
        run = 1
        while i + run < n and values[i + run] == values[i] and run < 128:
            run += 1
        if run >= 2:
            stream.append((257 - run, 8))
            stream.append((values[i], depth))
            i += run
        else:
            lits = []
            while i < n and len(lits) < 128:
                if i + 1 < n and values[i + 1] == values[i]:
                    break
                lits.append(values[i])
                i += 1
            stream.append((len(lits) - 1, 8))
            stream += [(v, depth) for v in lits]

    buf, acc, nbits = bytearray(), 0, 0
    for v, w in stream:
        acc = (acc << w) | (v & ((1 << w) - 1))
        nbits += w
        while nbits >= 8:
            nbits -= 8
            buf.append((acc >> nbits) & 0xFF)
    if nbits:
        buf.append((acc << (8 - nbits)) & 0xFF)
    return bytes(buf)


def imag_chunk(grid, pal):
    depth = max(1, (len(pal) - 1).bit_length())
    img = rle_pack([p for row in grid for p in row], depth)
    palbytes = bytes(c for rgb in pal for c in rgb)
    body = struct.pack(">BBBBBBHH", G_T, len(pal) - 1, 0x01 | 0x02, 1, 0,
                       depth, len(img) - 1, len(palbytes) - 1)
    body += img + palbytes
    return (b"IMAG" + struct.pack(">I", len(body)) + body
            + (b"\0" if len(body) & 1 else b""))


def glow_form(glow):
    face = struct.pack(">BBBBH", W - 1, H - 1, 1, 0x11, len(GLOW_PAL) * 3 - 1)
    body = (b"ICON"
            + b"FACE" + struct.pack(">I", len(face)) + face
            + imag_chunk(glow, GLOW_PAL)
            + imag_chunk(glow_selected(glow), GLOW_PAL))
    return b"FORM" + struct.pack(">I", len(body)) + body


def planar(g, depth):
    words = (W + 15) // 16
    out = bytearray()
    for plane in range(depth):
        for y in range(H):
            bits = 0
            for x in range(W):
                if (g[y][x] >> plane) & 1:
                    bits |= 1 << (words * 16 - 1 - x)
            out += bits.to_bytes(words * 2, "big")
    return bytes(out)


def image_header(depth):
    return struct.pack(">hhhhhIBBI", 0, 0, W, H, depth, 1, 0x03, 0x00, 0)


def build_info(classic, glow):
    gadget = struct.pack(">IhhhhHHH", 0, 0, 0, W, H, 0x0006, 0x0001, 0x0001)
    gadget += struct.pack(">IIIIIHI", 1, 1, 0, 0, 0, 0, 0)
    gadget = gadget[:44]

    do = struct.pack(">HH", 0xE310, 1) + gadget
    do += bytes([3, 0])                       # WBTOOL
    do += struct.pack(">II", 0, 0)
    do += struct.pack(">ii", NOPOS, NOPOS)
    do += struct.pack(">III", 0, 0, 0)
    assert len(do) == 78, len(do)

    out = bytearray(do)
    out += image_header(2) + planar(classic, 2)
    out += image_header(2) + planar(classic, 2)
    out += glow_form(glow)
    return bytes(out)


def main():
    logo = blocks()
    classic, glow = grids(logo)
    with open(OUT, "wb") as f:
        f.write(build_info(classic, glow))

    # Vorschau: klassisch | GlowIcon | angewaehlt, je vierfach
    z = 4
    sel = glow_selected(glow)
    prev = Image.new("RGB", (W * 3 * z + 20, H * z), WB_PAL[0])
    p = prev.load()
    for n, (g, pl) in enumerate(((classic, WB_PAL), (glow, GLOW_PAL), (sel, GLOW_PAL))):
        for y in range(H):
            for x in range(W):
                c = pl[g[y][x]]
                for dy in range(z):
                    for dx in range(z):
                        p[(n * W + x) * z + dx + n * 10, y * z + dy] = c
    prev.save("build/icon_preview.png")
    print("%s geschrieben (%dx%d), Vorschau build/icon_preview.png" % (OUT, W, H))


if __name__ == "__main__":
    main()
