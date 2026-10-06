#!/usr/bin/env python3
"""Validate the full Arabic catalog, including pre-existing UI symbols."""
import argparse
import importlib.util
import pathlib
import re

root = pathlib.Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("idiomas", root / "tools/idiomas.py")
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)
parser = argparse.ArgumentParser()
parser.add_argument("--fonts", type=pathlib.Path, default=root / "deploy/app/fonts")
args = parser.parse_args()
master, arabic = m.ler_mestra(), m.ler_irma("ar")
assert len(master) == len(arabic)
characters = set()
for (_, key, english), (line, same_key, value) in zip(master, arabic):
    assert key == same_key, (line, "key/order")
    k, v = m.decodificar(key), m.decodificar(value)
    text = v.decode("utf-8")
    assert text.strip(), (line, "empty")
    assert m.marcadores(k) == m.marcadores(v), (line, "printf")
    assert k.count(b"\n") == v.count(b"\n"), (line, "line breaks")
    assert len(k) - len(k.lstrip(b" ")) == len(v) - len(v.lstrip(b" ")), line
    assert len(k) - len(k.rstrip(b" ")) == len(v) - len(v.rstrip(b" ")), line
    assert not re.search(r"PH\d+|__\w+__", text), (line, "placeholder residue")
    for tag in (b"<b>", b"</b>"):
        assert k.count(tag) == v.count(tag), (line, "markup")
    characters.update(ord(c) for c in text if c not in "\n\r\t")
for weight in ("Regular", "Bold"):
    ar = m.cmap_ttf(root / "deploy/app/fonts" / ("NotoSansArabic-" + weight + ".ttf"))
    # A fresh checkout may not contain the upstream package's Inter fonts.
    # CI also checks the actual packaged Inter fonts after extracting the IPK.
    inter_path = args.fonts / ("InterDisplay-" + weight + ".ttf")
    missing = characters - ar
    if inter_path.exists():
        missing -= m.cmap_ttf(inter_path)
    else:
        missing -= set(map(ord, "←→↑↓✓"))
    assert not missing, (weight, ["U+%04X" % cp for cp in sorted(missing)])
print("Arabic catalog: %d entries; formats, markup and font coverage passed" % len(arabic))
