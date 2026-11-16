#!/usr/bin/env bash
# Mutation testing of the C++ logic with Mull (https://github.com/mull-project/mull).
#
#   make mutate        mutate only the lines changed against origin/main
#                      (override the base with MUTATE_BASE=<ref>)
#   make mutate-all    mutate all C++ code under esphome/ha-stiebel-control/
#
# The Makefile passes the test sources in MUTATE_SOURCES, so both builds use
# the same translation units.
#
# Mull is a clang compiler plugin: the unit tests are rebuilt with clang and the
# plugin into .mull/, then mull-runner runs the Catch2 binary once per mutant.
# A mutant that no test notices "survives" and points to a missing or weak
# assertion. Which code is mutated is set in mull.yml; tests/ is never mutated.
#
# Tool locations (defaults match the Ubuntu packages the CI workflow installs):
#   MULL_CXX          clang++ of the LLVM version the plugin was built for (clang++-18)
#   MULL_IR_FRONTEND  the Mull compiler plugin (/usr/lib/mull-ir-frontend-18)
#   MULL_RUNNER       mull-runner (mull-runner)
#   MODEL_TEST_FLAGS  same as for `make test`, e.g. -DHA_DEVICE_MODEL=wpl9
set -euo pipefail

cd "$(git rev-parse --show-toplevel)"

mode="diff"
case "${1:-}" in
    --all) mode=all ;;
    "") ;;
    *) echo "usage: $0 [--all]" >&2; exit 64 ;;
esac

if [ -z "${MUTATE_SOURCES:-}" ]; then
    echo "mutate: MUTATE_SOURCES is empty; run 'make mutate' or 'make mutate-all'." >&2
    exit 64
fi
read -r -a sources <<<"$MUTATE_SOURCES"

base=${MUTATE_BASE:-origin/main}
cxx=${MULL_CXX:-clang++-18}
plugin=${MULL_IR_FRONTEND:-/usr/lib/mull-ir-frontend-18}
runner=${MULL_RUNNER:-mull-runner}
out=.mull

for tool in "$cxx" "$runner"; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "mutate: '$tool' not found. Install Mull 0.38.0 and a matching clang," >&2
        echo "or set MULL_CXX / MULL_IR_FRONTEND / MULL_RUNNER (see CLAUDE.md)." >&2
        exit 1
    fi
done
if [ ! -f "$plugin" ]; then
    echo "mutate: Mull plugin '$plugin' not found (set MULL_IR_FRONTEND)." >&2
    exit 1
fi

if [ "$mode" = diff ]; then
    changed=$(git diff --name-only "$base" -- 'esphome/ha-stiebel-control/*.h' \
        'esphome/ha-stiebel-control/*.cpp' 'esphome/ha-stiebel-control/*.inc')
    if [ -z "$changed" ]; then
        echo "mutate: no C++ changes under esphome/ha-stiebel-control/ against $base."
        exit 0
    fi
    echo "mutate: changed against $base:"
    printf '%s\n' "$changed" | sed 's/^/  /'
fi

rm -rf "$out"
mkdir -p "$out/obj"

# The plugin reads its config at compile time. The incremental run adds the
# git diff filter to the shared mull.yml.
cp mull.yml "$out/mull.yml"
if [ "$mode" = diff ]; then
    {
        echo "gitDiffRef: $base"
        echo "gitProjectRoot: $PWD"
        echo "gitDiffPaths:"
        echo "  - esphome/ha-stiebel-control/"
    } >>"$out/mull.yml"
fi
export MULL_CONFIG="$PWD/$out/mull.yml"

# Same flags as the `test` target in the Makefile, at -O0 with
# debug info so that every mutant maps to a source line.
flags=(-std=c++17 -O0 -g -grecord-command-line
    -I. -Itests -Iesphome/ha-stiebel-control -Iesphome/ha-stiebel-control/elster
    -Wno-deprecated-declarations -Wno-sign-compare)
if [ -n "${MODEL_TEST_FLAGS:-}" ]; then
    read -r -a model_flags <<<"$MODEL_TEST_FLAGS"
    flags=("${model_flags[@]}" "${flags[@]}")
fi

echo "mutate: building instrumented tests with $cxx"
pids=()
for src in "${sources[@]}"; do
    # Catch2 itself is never mutated, so it is built without the plugin.
    case "$src" in
        */catch2/*) plugin_flag=() ;;
        *) plugin_flag=(-fpass-plugin="$plugin") ;;
    esac
    "$cxx" "${flags[@]}" ${plugin_flag[@]+"${plugin_flag[@]}"} -c "$src" \
        -o "$out/obj/$(basename "${src%.cpp}").o" &
    pids+=($!)
done
for pid in "${pids[@]}"; do
    wait "$pid"
done
"$cxx" "$out"/obj/*.o -o "$out/run_tests"

# Surviving mutants are reported, not treated as a failure: the score is a
# signal for review, not a gate.
"$runner" --allow-surviving --reporters IDE --reporters SQLite \
    --report-dir "$out" --report-name mutation "$out/run_tests"

# Score summary as Markdown (the CI workflow appends it to the job summary).
# Status codes: Mull's ExecutionStatus (1 failed = killed, 2 passed = survived,
# 3 timed out, 4 crashed, 5 abnormal exit; the last three also count as killed).
python3 - "$out/mutation.sqlite" <<'PYEOF' | tee "$out/summary.md"
import sqlite3, sys
names = {1: "killed", 2: "survived", 3: "timeout", 4: "crashed", 5: "abnormal exit", 8: "not covered"}
rows = sqlite3.connect(sys.argv[1]).execute(
    "SELECT filename, execution_status, count(*) FROM mutant GROUP BY 1, 2").fetchall()
files, total = {}, {}
for filename, status, count in rows:
    name = names.get(status, "other")
    short = filename.split("esphome/ha-stiebel-control/")[-1]
    files.setdefault(short, {})[name] = files.get(short, {}).get(name, 0) + count
    total[name] = total.get(name, 0) + count

def score(c):
    detected = c.get("killed", 0) + c.get("timeout", 0) + c.get("crashed", 0) + c.get("abnormal exit", 0)
    tested = detected + c.get("survived", 0)
    return f"{100 * detected / tested:.1f}%" if tested else "n/a"

cols = ["killed", "timeout", "crashed", "abnormal exit", "survived"]
print("| file | " + " | ".join(cols) + " | score |")
print("|" + "---|" * (len(cols) + 2))
for name, c in [*sorted(files.items()), ("**total**", total)]:
    print(f"| {name} | " + " | ".join(str(c.get(k, 0)) for k in cols) + f" | {score(c)} |")
PYEOF
