#!/usr/bin/env python3
"""
Split ElsterTable rows into a slim main table and a metadata side table (#57).

Old row (13 fields, ~44 bytes on the ESP32):
    { Name, Index, Type, friendlyName, haComponent, haDeviceClass, unit,
      stateClass, icon, payloadOn, payloadOff, isBlacklisted, hasMetadata }
New row (8 bytes):
    { Name, Index, Type, isBlacklisted, hasMetadata }
plus, for every row with hasMetadata = true, one ElsterMetaTable entry:
    { Index, Name, friendlyName, haComponent, haDeviceClass, unit,
      stateClass, icon, payloadOn, payloadOff }

The metadata fields of rows with hasMetadata = false were never read
(publishMqttDiscovery() only looks at them when hasMetadata is set), so they
are dropped. Preprocessor lines (#ifdef HA_MODEL_...) are copied into both
tables, comments stay in the main table.

Usage:
    tools/elster_table_split.py [--old OLD_HEADER] [--write] [--verify]

  --old     header in the old 13-field layout (default: the repo's ElsterTable.h;
            after the conversion use e.g. `git show 636eafd:<path> > old.h`)
  --write   write the converted ElsterTable.h and tests/elster_table_reference.inc
  --verify  compile tools/elster_table_dump.cpp against the old and the new header
            for every model x language and require identical dumps (needs g++)
"""
import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(REPO, "esphome", "ha-stiebel-control")
HEADER = os.path.join(SRC, "elster", "ElsterTable.h")
REFERENCE = os.path.join(REPO, "tests", "elster_table_reference.inc")
DUMP_CPP = os.path.join(REPO, "tools", "elster_table_dump.cpp")

# Model defines that select #ifdef branches in the table (none = generic build)
MODELS = ["", "wpl13e", "wpl17", "wpl23", "wpl10ac", "wpc7", "wpf10"]
LANGUAGES = ["DE", "EN"]

OLD_STRUCT = re.compile(
    r"typedef struct\s*\{.*?\}\s*ElsterIndex;", re.S)
NEW_STRUCT = """typedef struct
{
  const char * Name;
  unsigned short Index;
  unsigned char Type;
  // Flags (bit-fields: the row stays 8 bytes on the ESP32)
  bool isBlacklisted : 1;       // true = skip all processing
  bool hasMetadata : 1;         // true = HA metadata in ElsterMetaTable, false = type defaults
} ElsterIndex;

// Home Assistant metadata of the rows with hasMetadata = true (side table, #57).
// Look it up with GetElsterMeta(ei) (KElsterTable.h); NULL fields = type defaults.
typedef struct
{
  unsigned short Index;
  const char * Name;            // same as the main table row (Index is not unique)
  const char * friendlyName;    // display name, NULL = use Name
  const char * haComponent;     // "sensor" | "binary_sensor" | "text", NULL = derive from Type
  const char * haDeviceClass;   // "temperature" | "energy" | "", NULL = derive from Type
  const char * unit;            // "°C" | "kWh" | "", NULL = derive from Type
  const char * stateClass;      // "measurement" | "total_increasing" | "", NULL = derive from Type
  const char * icon;            // "mdi:thermometer" | "", NULL = derive from Type/Name
  const char * payloadOn;       // "on" | "ein", NULL = use "on" (for binary sensors)
  const char * payloadOff;      // "off" | "aus", NULL = use "off"
} ElsterMeta;

// Marks the split layout (used by tools/elster_table_dump.cpp)
#define ELSTER_TABLE_SPLIT_META 1"""


def split_fields(body):
    """Split the inside of a { ... } initializer at top-level commas."""
    fields, cur, quote, i = [], [], False, 0
    while i < len(body):
        c = body[i]
        if quote:
            cur.append(c)
            if c == "\\":
                cur.append(body[i + 1])
                i += 1
            elif c == '"':
                quote = False
        elif c == '"':
            quote = True
            cur.append(c)
        elif c == ",":
            fields.append("".join(cur).strip())
            cur = []
        else:
            cur.append(c)
        i += 1
    fields.append("".join(cur).strip())
    return fields


def parse_row(line):
    """Return (fields, trailing_comment) for a table row line, or None."""
    m = re.match(r"^\s*\{(.*)\}\s*,?\s*(//.*)?$", line)
    if not m:
        return None
    body = m.group(1)
    # A '}' inside a string would end the match early: make sure quotes balance
    if body.count('"') % 2:
        raise ValueError("unbalanced quotes: " + line)
    return split_fields(body), (m.group(2) or "")


def convert(text):
    start = text.index("const ElsterIndex ElsterTable[] =")
    open_brace = text.index("{", start)
    end = text.index("\n};", open_brace)
    head, table, tail = text[:open_brace + 1], text[open_brace + 1:end], text[end:]
    if not OLD_STRUCT.search(head) or "friendlyName" not in OLD_STRUCT.search(head).group(0):
        raise SystemExit("header is not in the old 13-field layout")

    main_lines, meta_lines, rows, meta_rows = [], [], 0, 0
    for line in table.split("\n"):
        stripped = line.strip()
        if not stripped:
            main_lines.append(line)
            continue
        if stripped.startswith("#"):
            main_lines.append(line)
            meta_lines.append(line)
            continue
        if stripped.startswith("//"):
            main_lines.append(line)
            continue
        parsed = parse_row(line)
        if parsed is None:
            raise SystemExit("cannot parse table line: " + line)
        f, comment = parsed
        if len(f) != 13:
            raise SystemExit("expected 13 fields, got %d: %s" % (len(f), line))
        name, index, typ = f[0], f[1], f[2]
        meta, black, has_meta = f[3:11], f[11], f[12]
        if black not in ("true", "false") or has_meta not in ("true", "false"):
            raise SystemExit("unexpected flags: " + line)
        rows += 1
        out = "  { %-46s, %s, %-16s, %-5s, %-5s }," % (name, index, typ, black, has_meta)
        if comment:
            out += " " + comment
        main_lines.append(out)
        if has_meta == "true":
            meta_rows += 1
            meta_lines.append("  { %s, %s, %s }," % (index, name, ", ".join(meta)))

    head = OLD_STRUCT.sub(lambda _: NEW_STRUCT, head, count=1)
    head = head.replace(
        "//  Name                                                 Index   Type\n"
        "//  Struktur-Definition in KElsterTable.h ",
        "//  Name                                             Index   Type              black  meta\n"
        "//  HA metadata of rows with meta = true: ElsterMetaTable below")
    meta_table = (
        "\n};\nconst unsigned ElsterTableCount = sizeof(ElsterTable) / sizeof(ElsterTable[0]);\n\n"
        "// HA metadata for the rows with hasMetadata = true, same order and #ifdefs as\n"
        "// ElsterTable. Fields: Index, Name, friendlyName, haComponent, haDeviceClass,\n"
        "// unit, stateClass, icon, payloadOn, payloadOff.\n"
        "const ElsterMeta ElsterMetaTable[] =\n{\n"
        + "\n".join(meta_lines) + "\n};\n"
        "const unsigned ElsterMetaCount = sizeof(ElsterMetaTable) / sizeof(ElsterMetaTable[0]);")
    old_count = "\n};\nconst unsigned ElsterTableCount = sizeof(ElsterTable) / sizeof(ElsterTable[0]);"
    if not tail.startswith(old_count):
        raise SystemExit("unexpected text after the table")
    tail = meta_table + tail[len(old_count):]
    # extern declarations for the side table
    head = head.replace(
        "extern const unsigned ElsterTableCount;\n",
        "extern const unsigned ElsterTableCount;\n"
        "extern const ElsterMeta ElsterMetaTable[];\n"
        "extern const unsigned ElsterMetaCount;\n")
    return head + "\n".join(main_lines) + tail, rows, meta_rows


def run(cmd):
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode:
        sys.stderr.write(" ".join(cmd) + "\n" + r.stdout + r.stderr)
        raise SystemExit(1)
    return r.stdout


def dump(header, model, lang, workdir, tag):
    """Compile elster_table_dump.cpp against `header`, return its row dump."""
    exe = os.path.join(workdir, "dump_%s_%s_%s" % (tag, model or "none", lang))
    cmd = ["g++", "-std=c++17", "-O0", "-w",
           "-I", os.path.dirname(header), "-I", os.path.join(SRC, "elster"), "-I", SRC,
           "-I", os.path.join(REPO, "tools"),
           '-DELSTER_HEADER="%s"' % header, "-DHA_LANGUAGE_" + lang]
    if model:
        cmd.append("-DHA_MODEL_" + model)
    srcs = [DUMP_CPP]
    if tag == "new":
        # GetElsterMeta() lives in KElsterTable.cpp, which holds the table definition and
        # includes "ElsterTable.h" from its own directory: compile copies next to `header`.
        hdir = os.path.dirname(header)
        for f in ("KElsterTable.cpp", "KElsterTable.h", "NUtils.cpp", "NUtils.h", "NTypes.h"):
            dst = os.path.join(hdir, f)
            if not os.path.exists(dst):
                shutil.copy(os.path.join(SRC, "elster", f), dst)
        srcs += [os.path.join(hdir, "KElsterTable.cpp"), os.path.join(hdir, "NUtils.cpp")]
        cmd.append("-DELSTER_DUMP_LINKED_TABLE")
    else:
        cmd.append("-DELSTER_TABLE_DEFINITIONS")
    run(cmd + srcs + ["-o", exe])
    return run([exe])


def write_reference(dump_text):
    lines = dump_text.rstrip("\n").split("\n")
    digests = [fnv1a(l) for l in lines]
    out = ["// Generated by tools/elster_table_split.py from the 13-field ElsterTable.h",
           "// (generic build, German names). One FNV-1a digest per row of",
           "// elsterRowText() (tools/elster_row_text.h). Do not edit by hand.",
           "static const unsigned ELSTER_REFERENCE_ROWS = %d;" % len(digests),
           "static const uint32_t ELSTER_REFERENCE_DIGESTS[] = {"]
    for i in range(0, len(digests), 6):
        out.append("    " + ", ".join("0x%08x" % d for d in digests[i:i + 6]) + ",")
    out.append("};")
    with open(REFERENCE, "w") as fh:
        fh.write("\n".join(out) + "\n")


def fnv1a(s):
    h = 0x811C9DC5
    for b in s.encode("utf-8"):
        h ^= b
        h = (h * 0x01000193) & 0xFFFFFFFF
    return h


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--old", default=HEADER)
    ap.add_argument("--write", action="store_true")
    ap.add_argument("--verify", action="store_true")
    args = ap.parse_args()

    old_text = open(args.old, encoding="utf-8").read()
    new_text, rows, meta_rows = convert(old_text)
    print("rows: %d, metadata rows: %d" % (rows, meta_rows))

    with tempfile.TemporaryDirectory() as tmp:
        old_dir, new_dir = os.path.join(tmp, "old"), os.path.join(tmp, "new")
        os.makedirs(old_dir)
        os.makedirs(new_dir)
        old_h, new_h = os.path.join(old_dir, "ElsterTable.h"), os.path.join(new_dir, "ElsterTable.h")
        open(old_h, "w", encoding="utf-8").write(old_text)
        open(new_h, "w", encoding="utf-8").write(new_text)

        if args.write:
            ref = dump(old_h, "", "DE", tmp, "old")
            open(HEADER, "w", encoding="utf-8").write(new_text)
            write_reference(ref)
            print("wrote", HEADER, "and", REFERENCE)

        if args.verify:
            if args.write:
                new_h = HEADER
            for model in MODELS:
                for lang in LANGUAGES:
                    a = dump(old_h, model, lang, tmp, "old")
                    b = dump(new_h, model, lang, tmp, "new")
                    status = "ok" if a == b else "DIFFERENT"
                    print("%-8s %s: %d rows %s" % (model or "generic", lang, a.count("\n"), status))
                    if a != b:
                        raise SystemExit(1)


if __name__ == "__main__":
    main()
