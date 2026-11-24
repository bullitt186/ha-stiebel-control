# Scope

What this project is for, so contributions go in the same direction. Changing this file is a
maintainer decision, recorded as an ADR in the same pull request (PR hygiene checks this).

## Purpose

Owners of Stiebel Eltron and Tecalor heat pumps want to see and control their heat pump in Home
Assistant without the manufacturer's gateway. This project is ESPHome firmware for an ESP32 that
joins the heat pump's internal CAN bus (Elster protocol) as a bus member, polls the signals of the
configured model, publishes them to Home Assistant through MQTT discovery and writes selected
settings back.

## In scope

- Heat pump models with a Stiebel Eltron / Tecalor manager on the Elster CAN bus (see the model
  table in the [README](../README.md#supported-models)); a new model is a request table, a profile
  and a smoke manifest ([ADR-0002](adr/0002-model-differences-in-one-compile-time-profile.md)).
- The two supported hardware variants: ESP32-S3 with the built-in CAN controller and a classic
  ESP32 with an MCP2515, each with WiFi or W5500 Ethernet.
- Reading signals, calculated values (COP, Delta T, compressor statistics, fault text) and
  writable settings with read-back, including SG Ready and WPMW II time programs.
- Home Assistant helpers, dashboards and diagnostic tools for the bus (bus scan, raw CAN stream).
- Entity names in German and English.

## Out of scope

- Pre-built firmware binaries: the firmware is compiled with the user's own credentials, so
  binaries built in CI would carry placeholders (see the release job in `.github/workflows/ci.yml`).
- Writes that are not verified on a real unit of the model: they stay behind a model flag that is
  off until checked ([ADR-0002](adr/0002-model-differences-in-one-compile-time-profile.md)).
- Other heat pump brands, other bus protocols or a cloud service: Open: to be decided by the
  maintainer.

## Direction

Open: to be decided by the maintainer. Current work is in the
[open issues](https://github.com/bullitt186/ha-stiebel-control/issues): more models and
verification of writes on further models, counter and COP accuracy, bus analysis tools, and the
second stage of the OTA encryption migration ([docs/INSTALLATION.md](INSTALLATION.md#ota-encryption-migration)).

## Principles

- Protocol values come from a source (signal table, manuals, recorded frames), never from
  guessing; unverified model behaviour stays off.
- One firmware for all models: model differences live only in the compile-time profile, and a
  user switches models with one line in `heatingpump.yaml`.
- Local only: the device talks to the heat pump bus and to Home Assistant (MQTT and the ESPHome
  API); `read_only: "true"` guarantees it never writes to the heat pump.
- Flash and RAM use stay below 50 % on both board variants.
- Signal protocol names and option values written to the bus stay German in every language.
