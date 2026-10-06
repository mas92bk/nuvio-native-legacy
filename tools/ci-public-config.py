#!/usr/bin/env python3
"""Reuse application configuration from the pinned official public binary.

Only allowlisted compile-time application fields are read, never user data.
Values stay in a temporary compiler header; do not print/upload that header.
"""
import base64
import json
import pathlib
import re
import struct
import sys
from elftools.elf.elffile import ELFFile

source, destination = map(pathlib.Path, sys.argv[1:])
fields = {
    "anon": (1200, "NV_SUPABASE_ANON_KEY"),
    "url": (300, "NV_SUPABASE_URL"),
    "baseLogin": (300, "NV_TV_LOGIN_BASE"),
    "traktCliente": (128, "NV_TRAKT_CLIENT_ID"),
    "traktSegredo": (128, "NV_TRAKT_CLIENT_SECRET"),
    "simklCliente": (200, "NV_SIMKL_CLIENT_ID"),
    "simklApp": (80, "NV_SIMKL_APP"),
}
values = {}
with source.open("rb") as stream:
    elf = ELFFile(stream)
    if elf["e_machine"] != "EM_ARM":
        raise SystemExit("Expected the pinned official ARM binary")
    symbols = elf.get_section_by_name(".symtab")
    if symbols is None:
        raise SystemExit("Official binary has no configuration symbol table")
    for symbol in symbols.iter_symbols():
        spec = fields.get(symbol.name)
        section_id = symbol["st_shndx"]
        if not spec or not isinstance(section_id, int) or symbol["st_size"] != spec[0]:
            continue
        section = elf.get_section(section_id)
        if section.name != ".data":
            continue
        offset = symbol["st_value"] - section["sh_addr"]
        text = section.data()[offset:offset + spec[0]].split(b"\0", 1)[0].decode("utf-8")
        if spec[1] in values:
            raise SystemExit("Ambiguous application configuration symbol")
        values[spec[1]] = text
    literals = elf.get_section_by_name(".rodata").data().split(b"\0")
    # The pinned binary contains exactly one 32-hex literal: its compiled
    # TMDB fallback. Refuse ambiguity rather than selecting another key.
    tmdb = {s.decode("ascii") for s in literals if re.fullmatch(rb"[0-9a-f]{32}", s)}
    if len(tmdb) != 1:
        raise SystemExit("Cannot unambiguously preserve the TMDB fallback")
    values["NV_TMDB_API_KEY"] = tmdb.pop()
    discord = {s.decode("ascii") for s in literals if re.fullmatch(rb"[0-9]{18,20}", s)}
    if len(discord) == 1:
        values["NV_DISCORD_CLIENT_ID"] = discord.pop()

    rec = {s.decode("ascii") for s in literals if re.fullmatch(rb"https://[^\s/]+(?:/v1)?", s) and b"rec" in s.lower()}
    if len(rec) != 1:
        raise SystemExit("Cannot unambiguously preserve the recommendation endpoint")
    values["NV_REC_URL"] = rec.pop()

    # Preserve the compiled Seekr default without guessing token-shaped
    # strings elsewhere in the binary. The pinned ARM executable has two
    # direct calls selecting the same .rodata literal into r0 (MOVW/MOVT).
    text_section = elf.get_section_by_name(".text")
    code = text_section.data()
    base = text_section["sh_addr"]
    callees = [s["st_value"] for s in symbols.iter_symbols()
               if s.name == "seekr_definir_chave" and s["st_info"]["type"] == "STT_FUNC"]
    if len(callees) != 1:
        raise SystemExit("Missing Seekr configuration call target")
    addresses = set()
    for offset in range(36, len(code) - 4, 4):
        word = struct.unpack_from("<I", code, offset)[0]
        if word & 0x0f000000 != 0x0b000000:
            continue
        displacement = word & 0xffffff
        if displacement & 0x800000:
            displacement -= 0x1000000
        if base + offset + 8 + displacement * 4 != callees[0]:
            continue
        low = high = None
        for at in range(offset - 36, offset, 4):
            instruction = struct.unpack_from("<I", code, at)[0]
            opcode = instruction & 0xfff0f000
            immediate = ((instruction >> 4) & 0xf000) | (instruction & 0xfff)
            if opcode == 0xe3000000:  # MOVW r0
                low = immediate
            elif opcode == 0xe3400000:  # MOVT r0
                high = immediate
        if low is not None and high is not None:
            addresses.add((high << 16) | low)
    rodata = elf.get_section_by_name(".rodata")
    if len(addresses) != 1:
        raise SystemExit("Ambiguous compiled Seekr default")
    address = addresses.pop() - rodata["sh_addr"]
    if not 0 <= address < rodata["sh_size"]:
        raise SystemExit("Seekr default is outside immutable application data")
    token = rodata.data()[address:address + 96].split(b"\0", 1)[0]
    if not re.fullmatch(rb"[A-Za-z0-9_-]{1,95}", token):
        raise SystemExit("Unexpected compiled Seekr default format")
    values["NV_SEEKR_API_KEY"] = token.decode("ascii")

for name in ("NV_SUPABASE_URL", "NV_SUPABASE_ANON_KEY", "NV_TV_LOGIN_BASE"):
    if not values.get(name):
        raise SystemExit("Missing required application configuration: " + name)
for name in ("NV_SUPABASE_URL", "NV_TV_LOGIN_BASE"):
    if not values[name].startswith("https://"):
        raise SystemExit("Expected HTTPS application endpoint")
key = values["NV_SUPABASE_ANON_KEY"]
if key.startswith("eyJ"):
    payload = key.split(".")[1]
    role = json.loads(base64.urlsafe_b64decode(payload + "=" * (-len(payload) % 4))).get("role")
    if role != "anon":
        raise SystemExit("Refusing a non-anonymous Supabase key")
elif not key.startswith("sb_publishable_"):
    raise SystemExit("Unrecognized public client key format")
values["NV_VERSAO"] = "2.0.0-arabic.1"
destination.write_text("\n".join("#define " + k + " " + json.dumps(v) for k, v in values.items()) + "\n")
destination.chmod(0o600)
print("Preserved public application configuration; no personal account data read")
