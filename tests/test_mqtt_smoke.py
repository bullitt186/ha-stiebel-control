#!/usr/bin/env python3
"""
MQTT smoke test for ha-stiebel-control firmware.

Subscribes to heatingpump/# for a configurable window and checks that all
required topics (common + model-specific) publish at least once.

Model manifests live in tests/models/_common_smoke.json and
tests/models/<model>_smoke.json.  The test requires the union of both.

Usage:
  # Run regression test (exits 0=pass, 1=fail):
  python3 tests/test_mqtt_smoke.py --model wpl13e --broker 192.168.30.10 \\
      --user iotuser --password secret --window 120

  # Capture a new baseline for a model (overwrites <model>_smoke.json):
  python3 tests/test_mqtt_smoke.py --model wpl13e --broker 192.168.30.10 \\
      --user iotuser --password secret --window 300 --capture-baseline

Environment variables (override CLI flags):
  MQTT_BROKER, MQTT_USER, MQTT_PASS, MQTT_WINDOW, DEVICE_MODEL
"""

import argparse
import json
import os
import sys
import time
from datetime import datetime, timezone
from pathlib import Path

try:
    import paho.mqtt.client as mqtt
except ImportError:
    print("ERROR: paho-mqtt not installed. Run: pip install paho-mqtt", file=sys.stderr)
    sys.exit(2)

MODELS_DIR = Path(__file__).parent / "models"
COMMON_FILE = MODELS_DIR / "_common_smoke.json"


def load_required_topics(model: str) -> tuple[list[str], list[str]]:
    """Return (common_topics, model_topics). Raises if files not found."""
    if not COMMON_FILE.exists():
        raise FileNotFoundError(f"Common smoke file not found: {COMMON_FILE}")
    with open(COMMON_FILE) as f:
        common = json.load(f)["required_topics"]

    model_file = MODELS_DIR / f"{model}_smoke.json"
    if not model_file.exists():
        print(f"WARNING: No model smoke file found at {model_file}. "
              f"Testing common topics only.", file=sys.stderr)
        return common, []
    with open(model_file) as f:
        model_topics = json.load(f)["required_topics"]
    return common, model_topics


def run_test(args) -> int:
    """Subscribe, collect topics, compare. Returns 0=pass, 1=fail."""
    common_topics, model_topics = load_required_topics(args.model)
    required = sorted(set(common_topics + model_topics))

    print(f"Smoke test: model={args.model}, broker={args.broker}, window={args.window}s")
    print(f"Required topics: {len(common_topics)} common + {len(model_topics)} model-specific"
          f" = {len(required)} total")

    observed: dict[str, str] = {}
    start_time = time.monotonic()

    def on_connect(client, userdata, flags, rc, props=None):
        if rc == 0:
            client.subscribe("heatingpump/#")
        else:
            print(f"ERROR: MQTT connect failed, rc={rc}", file=sys.stderr)

    def on_message(client, userdata, msg):
        try:
            payload = msg.payload.decode("utf-8", errors="replace")
        except Exception:
            payload = "<binary>"
        observed[msg.topic] = payload

    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)  # type: ignore[attr-defined]
    client.on_connect = on_connect
    client.on_message = on_message
    if args.user:
        client.username_pw_set(args.user, args.password)

    try:
        client.connect(args.broker, args.port, keepalive=60)
    except Exception as e:
        print(f"ERROR: Cannot connect to broker {args.broker}:{args.port}: {e}", file=sys.stderr)
        return 1

    print(f"Connected. Listening for {args.window}s ...")

    deadline = time.monotonic() + args.window
    while time.monotonic() < deadline:
        client.loop(timeout=1.0)
        elapsed = int(time.monotonic() - start_time)
        seen = len(observed)
        # Print progress every 10s (guard against duplicate prints at same second)
        if elapsed > 0 and elapsed % 10 == 0:
            missing_now = [t for t in required if t not in observed]
            marker = f"[{elapsed:3d}s]"
            if not hasattr(run_test, "_last_printed") or run_test._last_printed != marker:  # type: ignore[attr-defined]
                run_test._last_printed = marker  # type: ignore[attr-defined]
                print(f"  {marker} {seen} topics seen, {len(missing_now)} required still missing")

    client.disconnect()

    # Evaluate
    missing = [t for t in required if t not in observed]
    extra = sorted(set(observed) - set(required))

    print(f"\n{'='*60}")
    print(f"Results: {len(observed)} topics observed in {args.window}s")
    print(f"  Required:  {len(required)}")
    print(f"  Present:   {len(required) - len(missing)}")
    print(f"  Missing:   {len(missing)}")
    print(f"  Extra:     {len(extra)} (not required, allowed)")

    if missing:
        print(f"\nFAIL — missing required topics:")
        for t in missing:
            print(f"  ✗ {t}")
    else:
        print(f"\nPASS — all {len(required)} required topics observed")

    if args.verbose and extra:
        print(f"\nExtra topics (informational):")
        for t in sorted(extra):
            print(f"  + {t} = {observed[t][:60]}")

    return 1 if missing else 0


def run_capture(args) -> int:
    """Subscribe for window, write all observed topics to <model>_smoke.json."""
    print(f"Capture mode: model={args.model}, broker={args.broker}, window={args.window}s")
    observed: dict[str, str] = {}

    def on_connect(client, userdata, flags, rc, props=None):
        if rc == 0:
            client.subscribe("heatingpump/#")

    def on_message(client, userdata, msg):
        try:
            payload = msg.payload.decode("utf-8", errors="replace")
        except Exception:
            payload = "<binary>"
        observed[msg.topic] = payload

    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)  # type: ignore[attr-defined]
    client.on_connect = on_connect
    client.on_message = on_message
    if args.user:
        client.username_pw_set(args.user, args.password)

    client.connect(args.broker, args.port, keepalive=60)
    print(f"Connected. Listening for {args.window}s ...")

    deadline = time.monotonic() + args.window
    while time.monotonic() < deadline:
        client.loop(timeout=1.0)
    client.disconnect()

    # State topics only for the baseline (filter out debug, set, etc.)
    state_topics = sorted(
        t for t in observed
        if t.endswith("/state") or t == "heatingpump/status"
    )
    # Exclude debug, ephemeral, or clearly optional topics from the required list
    exclude_patterns = ["/debug", "MISCHERMODUL", "SAMMLERISTTEMP", "/set"]
    required = [
        t for t in state_topics
        if not any(p in t for p in exclude_patterns)
    ]

    # Separate into common-ish (all models) and model-specific
    # For capture mode we just write everything to the model file and let the user
    # split into _common if they want. The union approach means duplicates are harmless.
    model_file = MODELS_DIR / f"{args.model}_smoke.json"
    out = {
        "description": f"Required topics for {args.model} — captured {datetime.now(timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ')}",
        "required_topics": required,
        "observed_topics": {t: observed[t] for t in sorted(observed)},
    }
    MODELS_DIR.mkdir(parents=True, exist_ok=True)
    with open(model_file, "w") as f:
        json.dump(out, f, indent=2)
        f.write("\n")

    print(f"\nCaptured {len(observed)} total topics, {len(required)} written to required_topics.")
    print(f"Baseline saved: {model_file}")
    print("Review the file and move universal topics to tests/models/_common_smoke.json if needed.")
    return 0


def main():
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--broker",   default=os.getenv("MQTT_BROKER", "192.168.30.10"))
    p.add_argument("--port",     type=int, default=1883)
    p.add_argument("--user",     default=os.getenv("MQTT_USER", ""))
    p.add_argument("--password", default=os.getenv("MQTT_PASS", ""))
    p.add_argument("--model",    default=os.getenv("DEVICE_MODEL", "wpl13e"),
                   help="Heat pump model (selects tests/models/<model>_smoke.json)")
    p.add_argument("--window",   type=int, default=int(os.getenv("MQTT_WINDOW", "120")),
                   help="Observation window in seconds (default: 120)")
    p.add_argument("--capture-baseline", action="store_true",
                   help="Write observed topics to model smoke file instead of testing")
    p.add_argument("--verbose",  action="store_true",
                   help="Show extra (non-required) observed topics")
    args = p.parse_args()

    if args.capture_baseline:
        sys.exit(run_capture(args))
    else:
        sys.exit(run_test(args))


if __name__ == "__main__":
    main()
