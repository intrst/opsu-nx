#!/usr/bin/env python3
"""Generates core/src/GameImageTable.cpp from itdelatrisu/opsu/GameImage.java (T11).

Usage:  python3 -I gen_gameimage_table.py <GameImage.java> <out GameImageTable.cpp> [<GameImage.h to update>] [--dump]
  --dump  prints every distinct process_sub body found (use it when the Java file changes and the script stops with
          "unknown override"; then add the new body to PROC_KINDS below and implement the kind in GameImage.cpp).

The script is strict on purpose: an unknown constructor shape or an unknown override body aborts the run, so an entry
can never silently lose its processing. Constructor overloads of the Java enum (arguments: S = string, B = boolean):
  (S,S) (S,S,S) (S,S,B)=bgLocked (S,S,S,B)=fmt+bgLocked (S,S,B,B)=skinnable,preload (S,S,S,B,B) (S,S,B,B,B)=+optional
"""
import re, sys

# ---- normalised process_sub body -> (ProcKind enumerator, param1, param2) ------------------------------------------
# Whitespace is collapsed before matching.  Params are C++ float literals (exactly the Java literal).
PROC_KINDS = {
    "return scaleMin1px(img, Options.getMobileUIScale(0.5f));": ("ScaleMin1pxMobileUI", "0.5f", "0"),
    "return img.getScaledCopy(Options.getMobileUIScale(0.5f));": ("ScaleMobileUI", "0.5f", "0"),
    "return img.getScaledCopy(0.8f);": ("ScaleFixed", "0.8f", "0"),
    "return img.getScaledCopy((h / 3f) / img.getHeight());": ("FitHeightDiv", "3.0f", "0"),
    "return COUNTDOWN_READY.process_sub(img, w, h);": ("FitHeightDiv", "3.0f", "0"),  # COUNTDOWN_READY's body, verbatim
    "return img.getScaledCopy((h / 14f) / img.getHeight());": ("FitHeightDiv", "14.0f", "0"),
    "return img.getScaledCopy((h / 17f) / img.getHeight());": ("FitHeightDiv", "17.0f", "0"),
    "return img.getScaledCopy((h * 0.3f) / img.getHeight());": ("FitHeightMul", "0.3f", "0"),
    "return img.getScaledCopy((h * 0.0278f) / img.getHeight());": ("FitHeightMul", "0.0278f", "0"),
    "return img.getScaledCopy((h * 0.45f) / img.getHeight() * Options.getMobileUIScale(0.5f));": ("FitHeightMulMobileUI", "0.45f", "0.5f"),
    "return img.getScaledCopy((float) w / img.getWidth());": ("FitWidth", "0", "0"),
    "return img.getScaledCopy(w, h);": ("StretchWH", "0", "0"),
    "img.setAlpha(0.8f); return img.getScaledCopy(w, h);": ("StretchWHAlpha", "0.8f", "0"),
    "return img.getScaledCopy((MENU_BUTTON_BG.getImage().getHeight() * 0.16f) / img.getHeight());": ("FitButtonBgHeight", "0.16f", "0"),
    "return img.getScaledCopy((MENU_BUTTON_BG.getImage().getHeight() * 0.33f) / img.getHeight());": ("FitButtonBgHeight", "0.33f", "0"),
}
# bodies that are too specific for a parametrised kind: matched by a fragment, mapped to a named kind
SPECIAL = [
    ("float th = h * 0.033f * Options.getMobileUIScale(0.5f);", ("MenuTab", "0", "0")),
    ("return img.getScaledCopy(r, r);", ("MusicNoteFontSquare", "0", "0")),
    ("return img.getScaledCopy(r / 48f);", ("LoaderFontScale", "0", "0")),
    ("img.setAlpha(0.9f);", ("MenuBgCover", "0", "0")),
]

def strip_comments(src):
    out, i, n = [], 0, len(src)
    while i < n:
        c = src[i]
        if c == '"':                       # string literal
            j = i + 1
            while src[j] != '"':
                j += 2 if src[j] == '\\' else 1
            out.append(src[i:j + 1]); i = j + 1
        elif src.startswith("//", i):
            while i < n and src[i] != '\n': i += 1
        elif src.startswith("/*", i):
            j = src.index("*/", i + 2); out.append(" "); i = j + 2
        else:
            out.append(c); i += 1
    return "".join(out)

def split_top(s, sep=','):
    parts, depth, cur, i = [], 0, [], 0
    while i < len(s):
        c = s[i]
        if c == '"':
            j = i + 1
            while s[j] != '"': j += 2 if s[j] == '\\' else 1
            cur.append(s[i:j + 1]); i = j + 1; continue
        if c in '({': depth += 1
        if c in ')}': depth -= 1
        if c == sep and depth == 0:
            parts.append("".join(cur)); cur = []
        else:
            cur.append(c)
        i += 1
    parts.append("".join(cur))
    return parts

def parse(path):
    src = strip_comments(open(path, encoding="utf-8").read())
    m = re.search(r'public\s+enum\s+GameImage\s*\{', src)
    start = m.end()
    # the enum constant list ends at the first ';' at depth 0
    depth, i = 0, start
    while True:
        c = src[i]
        if c == '"':
            i += 1
            while src[i] != '"': i += 2 if src[i] == '\\' else 1
        elif c in '({': depth += 1
        elif c in ')}': depth -= 1
        elif c == ';' and depth == 0: break
        i += 1
    entries = []
    for chunk in split_top(src[start:i]):
        chunk = chunk.strip()
        if not chunk: continue
        mm = re.match(r'([A-Z_0-9]+)\s*\(', chunk)
        if not mm: sys.exit("cannot parse entry: " + chunk[:80])
        name = mm.group(1)
        # matching paren
        d, j = 0, mm.end() - 1
        while True:
            if chunk[j] == '"':
                j += 1
                while chunk[j] != '"': j += 2 if chunk[j] == '\\' else 1
            elif chunk[j] == '(': d += 1
            elif chunk[j] == ')':
                d -= 1
                if d == 0: break
            j += 1
        args = [a.strip() for a in split_top(chunk[mm.end():j])]
        rest = chunk[j + 1:].strip()
        body = rest[1:rest.rindex('}')] if rest.startswith('{') else ""
        entries.append((name, args, body))
    return entries

def ctor(name, args):
    kinds = "".join('S' if a.startswith('"') else 'B' if a in ('true', 'false') else '?' for a in args)
    lit = lambda a: a[1:-1]
    b = lambda a: a == 'true'
    d = dict(filename=None, fmt="", type=None, skinnable=True, preload=True, bg=False, opt=False)
    if kinds == "SS":      d.update(filename=lit(args[0]), type=lit(args[1]))
    elif kinds == "SSS":   d.update(filename=lit(args[0]), fmt=lit(args[1]), type=lit(args[2]))
    elif kinds == "SSB":   d.update(filename=lit(args[0]), type=lit(args[1]), bg=b(args[2]))
    elif kinds == "SSSB":  d.update(filename=lit(args[0]), fmt=lit(args[1]), type=lit(args[2]), bg=b(args[3]))
    elif kinds == "SSBB":  d.update(filename=lit(args[0]), type=lit(args[1]), skinnable=b(args[2]), preload=b(args[3]))
    elif kinds == "SSSBB": d.update(filename=lit(args[0]), fmt=lit(args[1]), type=lit(args[2]), skinnable=b(args[3]), preload=b(args[4]))
    elif kinds == "SSBBB": d.update(filename=lit(args[0]), type=lit(args[1]), skinnable=b(args[2]), preload=b(args[3]), opt=b(args[4]))
    else: sys.exit("%s: unknown constructor shape %s" % (name, kinds))
    return d

def methods(body):
    """Returns {method name: normalised body} for the overrides inside an entry body."""
    res = {}
    for m in re.finditer(r'@Override\s+protected\s+[\w.]+\s+(process_sub|scalesWithUI|allowsSkin)\s*\([^)]*\)\s*\{', body):
        d, j = 1, m.end()
        while d:
            if body[j] == '{': d += 1
            elif body[j] == '}': d -= 1
            j += 1
        res[m.group(1)] = re.sub(r'\s+', ' ', body[m.end():j - 1]).strip()
    return res

def classify(name, text):
    if text in PROC_KINDS: return PROC_KINDS[text]
    for frag, k in SPECIAL:
        if frag in text: return k
    sys.exit("%s: unknown override body: %s" % (name, text))

TYPE = {"png": "IMG_PNG", "jpg": "IMG_JPG"}
def cstr(s): return '"' + s.replace('\\', '\\\\').replace('"', '\\"') + '"'

def main():
    if len(sys.argv) < 3: sys.exit(__doc__)
    ents = parse(sys.argv[1])
    if "--dump" in sys.argv:
        for n, a, b in ents:
            for k, v in methods(b).items():
                if k == "process_sub": print(n, "::", v)
        return
    rows = []
    for name, args, body in ents:
        c = ctor(name, args)
        ms = methods(body)
        unknown = set(ms) - {"process_sub", "scalesWithUI", "allowsSkin"}
        assert not unknown
        kind = classify(name, ms["process_sub"]) if "process_sub" in ms else ("None", "0", "0")
        scales = {"return false;": "false", "return true;": "true"}[ms["scalesWithUI"]] if "scalesWithUI" in ms else "true"
        allows = {"return false;": "false", "return true;": "true"}[ms["allowsSkin"]] if "allowsSkin" in ms else "true"
        tb = " | ".join(TYPE[t] for t in c["type"].split("|"))
        rows.append((name, c, kind, scales, allows, tb))
    out = ['// GENERATED by docs/agents/T11_tools/gen_gameimage_table.py from GameImage.java - do not edit by hand.',
           '// Entry order = Java enum order (ordinal == index). %d entries.' % len(rows),
           '#include "opsu/GameImage.h"', '', 'namespace opsu {', 'namespace gameimage_detail {', '',
           'const GameImageInfo kGameImageTable[] = {',
           '    // name, filename, filenameFormat, type, beatmapSkinnable, preload, bgLocked, optional, scalesWithUI, allowsSkin, ProcKind, p1, p2']
    for name, c, kind, scales, allows, tb in rows:
        out.append('    {%s, %s, %s, %s, %s, %s, %s, %s, %s, %s, ProcKind::%s, %s, %s},' % (
            cstr(name), cstr(c["filename"]), cstr(c["fmt"]), tb, str(c["skinnable"]).lower(), str(c["preload"]).lower(),
            str(c["bg"]).lower(), str(c["opt"]).lower(), scales, allows, kind[0], kind[1], kind[2]))
    out += ['};', '', 'const int kGameImageCount = %d;' % len(rows), '', '}  // namespace gameimage_detail', '}  // namespace opsu', '']
    open(sys.argv[2], "w", encoding="utf-8").write("\n".join(out))
    # regenerate the enum list inside GameImage.h (between the BEGIN/END markers), if a header path is given
    hdr = [a for a in sys.argv[3:] if not a.startswith("--")]
    if hdr:
        t = open(hdr[0], encoding="utf-8").read()
        b, e = "    // BEGIN GENERATED ENUM (gen_gameimage_table.py)\n", "    // END GENERATED ENUM\n"
        i, j = t.index(b) + len(b), t.index(e)
        open(hdr[0], "w", encoding="utf-8").write(t[:i] + "".join("    %s,\n" % r[0] for r in rows) + t[j:])
    print("wrote %d entries to %s" % (len(rows), sys.argv[2]))

main()
