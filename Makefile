.PHONY: compile compile-s2 check logs upload config test test-all clean-tests smoke-test capture-baseline

# Extract model and MQTT credentials from local config files
DEVICE_MODEL ?= $(shell grep 'device_model:' esphome/heatingpump.yaml | grep -v '^\s*\#' | head -1 | sed 's/.*"\(.*\)".*/\1/')
MQTT_BROKER  ?= $(shell grep 'mqtt_broker:' esphome/secrets.yaml | sed 's/mqtt_broker:[[:space:]]*//' | tr -d '"')
MQTT_USER    ?= $(shell grep 'mqtt_username:' esphome/secrets.yaml | sed 's/mqtt_username:[[:space:]]*//' | tr -d '"')
MQTT_PASS    ?= $(shell grep 'mqtt_password:' esphome/secrets.yaml | sed 's/mqtt_password:[[:space:]]*//' | tr -d '"')

CXX      ?= g++
CXXFLAGS  = -std=c++17 -Wall -Wextra \
            -I. -Itests \
            -Iesphome/ha-stiebel-control \
            -Iesphome/ha-stiebel-control/elster \
            -Wno-deprecated-declarations \
            -Wno-sign-compare
# test_*.cpp files are each their own TU. The elster library .cpp files are
# compiled once as separate objects to avoid duplicate symbols.
TEST_SRCS     = tests/catch2/catch_amalgamated.cpp \
                tests/test_sg_ready.cpp
TEST_EXTRA    = tests/test_nutils.cpp \
                tests/test_kelster.cpp \
                tests/test_can_logic.cpp \
                tests/signal_requests_stub.cpp
ELSTER_SRCS   = esphome/ha-stiebel-control/elster/NUtils.cpp \
                esphome/ha-stiebel-control/elster/KElsterTable.cpp
ELSTER_OBJS   = tests/NUtils.o tests/KElsterTable.o
TEST_BIN      = tests/run_tests

# ── ESPHome firmware ─────────────────────────────────────────────────────────

# Compile the ESP32-S3 variant (default user-facing config)
compile:
	cd esphome && esphome compile heatingpump.yaml

# Compile the ESP32-S2 / MCP2515 variant
compile-s2:
	cd esphome && esphome compile ci_heatingpump_s2.yaml

# Full firmware quality gate — both board variants must compile cleanly
check: test compile compile-s2

# Stream live logs from connected device
logs:
	cd esphome && esphome logs heatingpump.yaml

# Compile and OTA-flash to production device (192.168.30.107)
upload:
	cd esphome && esphome run heatingpump.yaml --device 192.168.30.107 --no-logs

# Dump merged YAML for debugging package includes
config:
	cd esphome && esphome config heatingpump.yaml

# ── Native unit tests ─────────────────────────────────────────────────────────

# Build and run unit tests (fast, no hardware needed)
test: $(TEST_BIN)
	./$(TEST_BIN)

# Build and run with verbose reporter
test-all: $(TEST_BIN)
	./$(TEST_BIN) --reporter console --success

TEST_EXTRA_OBJS = $(patsubst tests/%.cpp,tests/%.o,$(TEST_EXTRA))

tests/%.o: tests/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

tests/NUtils.o: esphome/ha-stiebel-control/elster/NUtils.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

tests/KElsterTable.o: esphome/ha-stiebel-control/elster/KElsterTable.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(TEST_BIN): $(TEST_SRCS) $(TEST_EXTRA_OBJS) $(ELSTER_OBJS) esphome/ha-stiebel-control/sg_ready_controller.h
	$(CXX) $(CXXFLAGS) $(TEST_SRCS) $(TEST_EXTRA_OBJS) $(ELSTER_OBJS) -o $(TEST_BIN)

clean-tests:
	rm -f $(TEST_BIN) $(TEST_EXTRA_OBJS) $(ELSTER_OBJS)

# ── MQTT smoke test ───────────────────────────────────────────────────────────

# Run MQTT regression test against live device (120s observation window)
smoke-test:
	python3 tests/test_mqtt_smoke.py \
	  --broker "$(MQTT_BROKER)" \
	  --user "$(MQTT_USER)" \
	  --password "$(MQTT_PASS)" \
	  --model "$(DEVICE_MODEL)" \
	  --window 120

# Capture a new baseline for the current model (300s window, overwrites model file)
capture-baseline:
	python3 tests/test_mqtt_smoke.py \
	  --broker "$(MQTT_BROKER)" \
	  --user "$(MQTT_USER)" \
	  --password "$(MQTT_PASS)" \
	  --model "$(DEVICE_MODEL)" \
	  --window 300 \
	  --capture-baseline
