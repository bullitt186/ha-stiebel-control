#!/usr/bin/env python3
"""
Remove stale MQTT discovery entities for this project's heating pump.

MQTT discovery is *retained*: a Home Assistant entity disappears only when its
retained discovery payload is cleared (an empty payload published retained). So
when a user comments signals out of `signal_requests_<model>.h`, the old
entities remain in Home Assistant until their discovery topics are removed.

This script:
  1. collects every retained discovery config for the device node,
  2. prints them,
  3. (with --apply) publishes an empty retained payload to each, which deletes
     the corresponding entity in Home Assistant.

Afterwards the firmware recreates only the entities that still exist in its
current signal table. Trigger that immediately by publishing `go` to
`<node>/republish_discoveries`, or simply wait for the periodic 15-minute
republish.

Examples:
  # dry-run (list only)
  python3 helperscripts/clean_mqtt_discovery.py \\
      --broker 192.168.30.10 --user iotuser --password secret

  # clear everything for the node, then let the device republish
  python3 helperscripts/clean_mqtt_discovery.py \\
      --broker 192.168.30.10 --user iotuser --password secret --apply
"""

import argparse
import re
import time

try:
    import paho.mqtt.client as mqtt
except ImportError:
    raise SystemExit("ERROR: paho-mqtt not installed. Run: pip install paho-mqtt")


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--broker", required=True)
    p.add_argument("--port", type=int, default=1883)
    p.add_argument("--user", default="")
    p.add_argument("--password", default="")
    p.add_argument("--prefix", default="homeassistant",
                   help="HA discovery prefix (default: homeassistant)")
    p.add_argument("--node", default="heatingpump",
                   help="MQTT discovery node id == esphome name/device_name (default: heatingpump)")
    p.add_argument("--window", type=int, default=8,
                   help="seconds to collect retained discovery topics (default: 8)")
    p.add_argument("--filter", default=None,
                   help="only clear topics whose object_id matches this regex")
    p.add_argument("--apply", action="store_true",
                   help="actually clear the topics (default: dry-run)")
    args = p.parse_args()

    pattern = f"{args.prefix}/+/{args.node}/+/config"
    found: "dict[str, str]" = {}

    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
    client.on_message = lambda c, u, m: found.__setitem__(m.topic, m.payload.decode("utf-8", "replace"))
    if args.user:
        client.username_pw_set(args.user, args.password)
    client.connect(args.broker, args.port, keepalive=30)
    client.subscribe(pattern)
    client.loop_start()
    time.sleep(args.window)
    client.loop_stop()

    items = sorted(found)
    if args.filter:
        rx = re.compile(args.filter)
        items = [t for t in items if rx.search(t)]

    print(f"Found {len(items)} discovery topic(s) for node '{args.node}' (pattern {pattern})")
    for t in items:
        print("  " + t)

    if not args.apply:
        print("\nDry-run. Re-run with --apply to clear these (empty retained payload).")
        client.disconnect()
        return 0

    for t in items:
        client.publish(t, "", qos=1, retain=True)
    time.sleep(1.0)
    client.disconnect()
    print(f"\nCleared {len(items)} discovery topic(s).")
    print(f"Trigger a republish now: publish 'go' to {args.node}/republish_discoveries")
    print("(or wait for the periodic 15-minute republish).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
