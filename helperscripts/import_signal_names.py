#!/usr/bin/env python3
"""Import Elster index facts (name, index, type) from kr0ner/OneESP32ToRuleThemAll.

Usage: import_signal_names.py <path/to/kr0ner/components/wp_core/property.h> [--write]

Only facts are taken (no code): the upstream repository has no license.
  1. ElsterTable rows still called TEST_OBJEKT_* whose index kr0ner names
     unambiguously are renamed (and get kr0ner's type if ours is untyped),
     unless the TEST_OBJEKT_* name is used by a signal_requests_*.h.
  2. Indices missing from our table are appended in a block that only compiles
     for the WPM3-generation models, to keep the ESP32-S2 flash budget.
Only entries valid for WPL/TTF/WPE models are considered (THZ-only blocks are
skipped). Without --write the planned changes are printed (dry run).
"""
import glob
import os
import re
import sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "esphome", "ha-stiebel-control")
TABLE = os.path.join(ROOT, "elster", "ElsterTable.h")
OUR_TYPES = {"et_default", "et_dec_val", "et_cent_val", "et_mil_val", "et_byte", "et_bool",
             "et_little_bool", "et_double_val", "et_triple_val", "et_little_endian", "et_betriebsart"}
MODEL_GUARD = ("#if defined(HA_MODEL_wpl17) || defined(HA_MODEL_wpl23) || "
               "defined(HA_MODEL_wpl10ac) || defined(HA_MODEL_wpc7)")
BLOCK_START = "  // kr0ner/OneESP32ToRuleThemAll index facts (WPM3 generation)"


def applies(cond):
    return cond.startswith("!defined") or any(k in cond for k in ("WPL_", "TTF_07_C", "WPE_I"))


def parse_kr0ner(path):
    """index -> set of (name, type) for entries valid on WPL/TTF/WPE models."""
    facts, stack = {}, []
    for line in open(path, encoding="utf-8", errors="ignore"):
        s = line.strip()
        if s.startswith("#if"):
            stack.append(s[3:].strip().lstrip("def").strip())
        elif s.startswith("#endif") and stack:
            stack.pop()
        m = re.match(r"PROPERTY\((\w+),\s*(0x[0-9a-fA-F]+)(?:,\s*Type::(\w+))?\)", s)
        if m and all(applies(c) for c in stack[1:]):  # stack[0] is the include guard
            t = m.group(3) if m.group(3) in OUR_TYPES else "0"
            facts.setdefault(int(m.group(2), 16), set()).add((m.group(1), t))
    return facts


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    write = "--write" in sys.argv
    facts = parse_kr0ner(sys.argv[1])
    table = open(TABLE, encoding="utf-8").read()
    rows = {int(i, 16): n for n, i in re.findall(r'\{\s*"(\w+)"\s*,\s*(0x[0-9a-fA-F]+)', table)}
    names = set(rows.values())
    used = set()
    for f in glob.glob(os.path.join(ROOT, "signal_requests_*.h")):
        used |= set(re.findall(r'"(\w+)"', open(f, encoding="utf-8").read()))

    renamed = 0
    for idx, cands in sorted(facts.items()):
        ours = rows.get(idx)
        if not ours or not ours.startswith("TEST_OBJEKT_") or ours in used or len({c[0] for c in cands}) != 1:
            continue
        name, typ = next(iter(cands))
        if name in names:
            continue
        pat = re.compile(r'\{ "%s", (0x[0-9a-fA-F]+), (\w+), "[^"]*",' % ours)
        def sub(m, name=name, typ=typ):
            t = typ if m.group(2) == "0" else m.group(2)
            return '{ "%s", %s, %s, "%s",' % (name, m.group(1), t, name.replace("_", " ").title())
        table, n = pat.subn(sub, table, count=1)
        if n:
            renamed += 1
            names.add(name)
            print(f"rename {ours:18s} 0x{idx:04x} -> {name}")

    added = []
    if BLOCK_START not in table:
        for idx, cands in sorted(facts.items()):
            if idx in rows or len({c[0] for c in cands}) != 1:
                continue
            name, typ = next(iter(cands))
            if name in names:
                name = f"{name}_{idx:04X}"
            added.append(f'  {{ "{name}", 0x{idx:04x}, {typ}, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, false, false }},')
            names.add(name)
        if added:
            block = "\n".join([BLOCK_START, MODEL_GUARD] + added + ["#endif", ""])
            end = table.index("\n};", table.index("static const ElsterIndex ElsterTable[]"))
            table = table[:end + 1] + block + table[end + 1:]
    print(f"{renamed} renamed, {len(added)} added" + ("" if write else " (dry run)"))
    if write:
        open(TABLE, "w", encoding="utf-8").write(table)


if __name__ == "__main__":
    main()
