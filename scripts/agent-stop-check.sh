#!/usr/bin/env bash
# Claude Code Stop hook: run the fast local checks before an agent stops.
#
# Reads the hook input (JSON) from stdin. Exits 0 when there is nothing to
# check or every check passes. Exits 2 with a short summary on stderr when a
# check fails, which makes Claude Code continue working on the findings.
#
# Local and offline only: no firmware compile (`make check` does that, and it
# takes minutes), no upload, no MQTT, no Home Assistant. Mutation testing is
# `make mutate`.
set -uo pipefail

input=$(cat)

# A Stop hook that already sent the agent back must not loop forever.
if printf '%s' "$input" | grep -Eq '"stop_hook_active"[[:space:]]*:[[:space:]]*true'; then
    exit 0
fi

cd "${CLAUDE_PROJECT_DIR:-$(git rev-parse --show-toplevel 2>/dev/null || pwd)}" || exit 0

if [ -z "$(git status --porcelain 2>/dev/null)" ]; then
    exit 0
fi

failed=()
notes=()
log=$(mktemp "${TMPDIR:-/tmp}/ha-stiebel-stop-check.XXXXXX")
cfgdir=""
trap 'rm -f "$log"; [ -n "$cfgdir" ] && rm -rf "$cfgdir"' EXIT

run_check() {
    local name=$1
    shift
    if ! "$@" >"$log" 2>&1; then
        failed+=("$name")
        {
            echo "--- $name failed:"
            tail -n 25 "$log"
        } >&2
    fi
}

# C++ unit tests (Catch2), default model as in `make test`.
run_check "unit tests (make test)" make --no-print-directory test

# MQTT manifest tests, as in CI. They import paho-mqtt but use no broker.
if python3 -c "import paho.mqtt.client" >/dev/null 2>&1; then
    run_check "MQTT manifest tests" \
        python3 -m unittest discover -s tests -p 'test_mqtt_manifest.py'
else
    notes+=("MQTT manifest tests skipped (python3 without paho-mqtt)")
fi

# Whitespace errors and conflict markers in the changed lines only; existing
# files still contain old whitespace that a full pre-commit run would rewrite.
run_check "git diff --check" git diff --check HEAD

# YAML lint with the CI rules.
if command -v yamllint >/dev/null 2>&1; then
    run_check "yamllint" yamllint -d '{extends: relaxed, rules: {line-length: {max: 120}}}' \
        esphome/ha-stiebel-control/*.yaml packages/*.yaml
else
    notes+=("yamllint skipped (not installed)")
fi

# ESPHome config validation (no compile) of the CI configs, only when the
# firmware files changed. Runs on a copy with the credential-free CI secrets,
# so the real esphome/secrets.yaml is never read.
esphome_bin=${ESPHOME:-esphome}
changed=$( { git diff --name-only HEAD; git ls-files --others --exclude-standard; } 2>/dev/null)
if printf '%s\n' "$changed" | grep -q '^esphome/'; then
    if command -v "$esphome_bin" >/dev/null 2>&1; then
        cfgdir=$(mktemp -d "${TMPDIR:-/tmp}/ha-stiebel-esphome-config.XXXXXX")
        cp -R esphome/ha-stiebel-control "$cfgdir/"
        cp esphome/ci_heatingpump_*.yaml "$cfgdir/"
        cp esphome/ci_secrets.yaml "$cfgdir/secrets.yaml"
        for cfg in "$cfgdir"/ci_heatingpump_*.yaml; do
            name="esphome config $(basename "$cfg")"
            if ! "$esphome_bin" config "$cfg" >"$log" 2>&1; then
                if grep -q "Your ESPHome version is too old" "$log"; then
                    version=$("$esphome_bin" version 2>/dev/null | sed 's/^Version: //')
                    notes+=("esphome config skipped (ESPHome $version is older than min_version)")
                    break
                fi
                failed+=("$name")
                {
                    echo "--- $name failed:"
                    grep -v -e '^INFO' -e '^WARNING' "$log" | head -n 25
                } >&2
                break  # the configs share most files; one report is enough
            fi
        done
    else
        notes+=("esphome config skipped (esphome not installed)")
    fi
fi

for note in ${notes[@]+"${notes[@]}"}; do
    echo "Note: $note." >&2
done

if [ ${#failed[@]} -gt 0 ]; then
    echo "Fast check failed: ${failed[*]}. Fix the findings before stopping." >&2
    exit 2
fi
exit 0
