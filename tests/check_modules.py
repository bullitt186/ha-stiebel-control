#!/usr/bin/env python3
"""Firmware module layout of esphome/ha-stiebel-control/ (#109).

- The quoted #includes between the firmware headers form no cycle.
- ha-stiebel-control.h stays below MAX_MAIN_LINES lines (CAN entry points and includes only).
- Every header that a header in common.yaml `includes:` pulls in is listed there too:
  ESPHome copies only listed files, so a missing module breaks the firmware build while
  `make test` still passes. The model headers (signal_requests_<model>.h) come from the
  model packages and are not checked here.
Exit code 0 = ok, 1 = problems found.
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "esphome" / "ha-stiebel-control"
MAIN = "ha-stiebel-control.h"
MAX_MAIN_LINES = 800
INCLUDE_RE = re.compile(r'^\s*#\s*include\s+"([^"]+)"', re.M)


def header_graph() -> dict:
    """header file name -> quoted includes that are firmware headers (same folder or elster/)."""
    headers = {p.name: p for p in list(SRC.glob("*.h")) + list((SRC / "elster").glob("*.h"))}
    graph = {}
    for name, path in headers.items():
        deps = [Path(inc).name for inc in INCLUDE_RE.findall(path.read_text(encoding="utf-8"))]
        graph[name] = [d for d in deps if d in headers]
    return graph


def find_cycle(graph: dict):
    """A list of headers forming a cycle, or None."""
    state = {}   # 1 = on the current path, 2 = done
    path = []

    def visit(node):
        state[node] = 1
        path.append(node)
        for dep in graph.get(node, []):
            if state.get(dep) == 1:
                return path[path.index(dep):] + [dep]
            if dep not in state:
                cycle = visit(dep)
                if cycle:
                    return cycle
        path.pop()
        state[node] = 2
        return None

    for node in sorted(graph):
        if node not in state:
            cycle = visit(node)
            if cycle:
                return cycle
    return None


def listed_includes() -> set:
    text = (SRC / "common.yaml").read_text(encoding="utf-8")
    block = re.search(r"^  includes:\n((?:    (?:- .*|#.*)\n)+)", text, re.M)
    return {Path(m).name for m in re.findall(r"^    - (\S+)", block.group(1), re.M)} if block else set()


def main() -> int:
    problems = []
    graph = header_graph()

    cycle = find_cycle(graph)
    if cycle:
        problems.append("include cycle: " + " -> ".join(cycle))

    lines = len((SRC / MAIN).read_text(encoding="utf-8").splitlines())
    if lines >= MAX_MAIN_LINES:
        problems.append(f"{MAIN} has {lines} lines (limit {MAX_MAIN_LINES}); move logic into a module")

    listed = listed_includes()
    for name in sorted(listed):
        for dep in graph.get(name, []):
            if dep not in listed:
                problems.append(f"common.yaml includes {name}, which includes {dep}: list {dep} as well")

    for p in problems:
        print(p)
    print(f"{len(problems)} problem(s); {len(graph)} headers, {MAIN}: {lines} lines")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
