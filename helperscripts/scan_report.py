#!/usr/bin/env python3
"""Turn bus-scan results into a signal_requests block and a Markdown table.

1. Start a scan (HA button "Bus-Scan <member>", or
   mosquitto_pub -t heatingpump/scan/start -m HEIZMODUL) and wait until
   heatingpump/scan/status reports "done".
2. Dump the retained results:
   mosquitto_sub -h BROKER -u USER -P PASS -t 'heatingpump/scan/#' -v -W 5 > scan.txt
3. python3 helperscripts/scan_report.py scan.txt

Only indices known to ElsterTable get a request line; unknown ones are listed in
the Markdown table so they can be named later.
"""
import json
import sys

MEMBER_ENUM = {"MANAGER": "cm_manager", "HEIZMODUL": "cm_heizmodul", "KESSEL": "cm_kessel"}


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    hits = []
    for line in open(sys.argv[1], encoding="utf-8", errors="replace"):
        topic, _, payload = line.strip().partition(" ")
        parts = topic.split("/")
        if len(parts) != 4 or parts[1] != "scan":
            continue
        try:
            data = json.loads(payload)
        except json.JSONDecodeError:
            continue
        hits.append((parts[2], int(parts[3], 16), data.get("name", ""), data.get("value", ""), data.get("raw")))
    hits.sort()

    print("// signal_requests block (adjust frequencies; FREQ_10MIN is a safe default)")
    for member, idx, name, value, _ in hits:
        if name and member in MEMBER_ENUM:
            print(f'    {{"{name}", FREQ_10MIN, {MEMBER_ENUM[member]}}},  // 0x{idx:04x} = {value}')
    print("\n| Member | Index | Name | Value | Raw |\n|---|---|---|---|---|")
    for member, idx, name, value, raw in hits:
        print(f"| {member} | 0x{idx:04x} | {name or '(unknown)'} | {value} | {raw} |")
    print(f"\n{len(hits)} answering indices")


if __name__ == "__main__":
    main()
