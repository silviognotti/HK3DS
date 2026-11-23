#!/usr/bin/env python3
"""Hollow Knight's language sheets -> romfs/text/<Sheet>.txt (English) and romfs/text/<lang>/<Sheet>.txt (Spanish:
es) for the 3DS dialogue system.
The game's Resources/languages/<LANG>_<Sheet>.txt are base64 AES-ECB encrypted XML (<entry name="KEY">text</entry>).
   python text_sheets.py <languages dir> <romfs/text> [LANG]
Text is single-byte: 7-bit ASCII plus the Latin-1 letters in LATIN1 below (the bitmap fonts have glyphs for exactly
those, tools/build_ui.py); other letters lose their accents.
Output: one 'KEY<TAB>text' line per entry, text in 7-bit ASCII (the bitmap fonts' range), pages separated by
'\\f' (<page>), line breaks as '\\n' (<br>)."""

import sys, os, re, base64, html, glob

KEY = b"UKu52ePUBwetZ9wNX88o54dnfKRu0T1l"


def decrypt(raw):
    from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes

    dec = Cipher(algorithms.AES(KEY), modes.ECB()).decryptor()
    d = dec.update(base64.b64decode(raw)) + dec.finalize()
    return d[: -d[-1]].decode("utf-8")


ASCII = {
    "\u2018": "'",
    "\u2019": "'",
    "\u201c": '"',
    "\u201d": '"',
    "\u2026": "...",
    "\u2013": "-",
    "\u2014": "-",
    "\u00a0": " ",
    "\u00e9": "e",
    "\u00e8": "e",
    "\u00ea": "e",
    "\u00e0": "a",
    "\u00e2": "a",
    "\u00f4": "o",
    "\u00fc": "u",
    "\u00f6": "o",
    "\u00e4": "a",
    "\u00ef": "i",
    "\u00e7": "c",
    "\u00f1": "n",
    "\u00c9": "E",
}

# the Latin-1 characters the fonts carry: what Spanish needs (and a few the Spanish sheets use besides)
LATIN1 = "\u00a1\u00ab\u00ae\u00bb\u00bf\u00c1\u00c9\u00cd\u00d1\u00d3\u00da\u00dc\u00e1\u00e7\u00e9\u00ea\u00ed\u00f1\u00f3\u00f6\u00fa\u00fc"


def clean(t, keep=""):
    t = html.unescape(t)
    t = t.replace("<page>", "\f").replace("<br>", "\n").replace("<hpage>", "\f")
    t = re.sub(
        r"</?(?:i|b|size[^>]*|color[^>]*|font[^>]*|line-height[^>]*|voffset[^>]*|cspace[^>]*|align[^>]*)>",
        "",
        t,
    )
    t = "".join(c if c in keep else ASCII.get(c, c) for c in t)
    t = "".join(
        c if (32 <= ord(c) < 127 or c in "\f\n" or c in keep) else "?" for c in t
    )
    return t.replace("\r", "").replace("\t", " ").strip()


def convert(src_dir, out_dir, lang="EN"):
    keep = "" if lang == "EN" else LATIN1
    if lang != "EN":
        out_dir = os.path.join(out_dir, lang.lower())
    os.makedirs(out_dir, exist_ok=True)
    n = 0
    for p in sorted(glob.glob(f"{src_dir}/{lang}_*.txt")):
        sheet = os.path.basename(p)[len(lang) + 1 : -4]
        raw = open(p, "rb").read()
        try:
            xml = decrypt(raw)
        except Exception:
            xml = raw.decode("utf-8", "replace")
        lines = []
        for k, v in re.findall(r'<entry name="([^"]+)">(.*?)</entry>', xml, re.S):
            v = (
                clean(v, keep)
                .replace("\\", "/")
                .replace("\n", "\\n")
                .replace("\f", "\\f")
            )
            lines.append(f"{k}\t{v}")
        open(f"{out_dir}/{sheet}.txt", "wb").write(
            ("\n".join(lines) + "\n").encode("latin-1")
        )
        n += len(lines)
    return n


if __name__ == "__main__":
    print(
        convert(sys.argv[1], sys.argv[2], sys.argv[3] if len(sys.argv) > 3 else "EN"),
        "entries",
    )
