.PHONY: compile compile-s2 check logs upload config

# Compile the ESP32-S3 variant (default user-facing config)
compile:
	cd esphome && esphome compile heatingpump.yaml

# Compile the ESP32-S2 / MCP2515 variant (uses ci_heatingpump_s2.yaml)
compile-s2:
	cd esphome && esphome compile ci_heatingpump_s2.yaml

# Full quality gate — both board variants must compile cleanly
check: compile compile-s2

# Stream live logs from connected device (uses S3 config)
logs:
	cd esphome && esphome logs heatingpump.yaml

# Compile and OTA-flash to connected device
upload:
	cd esphome && esphome run heatingpump.yaml

# Dump merged YAML for debugging package includes
config:
	cd esphome && esphome config heatingpump.yaml
