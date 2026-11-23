.PHONY: compile compile-esp32 compile-s2 check logs upload config test test-all clean-tests smoke-test capture-baseline \
        mutate mutate-all FORCE

# Extract model and MQTT credentials from local config files
DEVICE_MODEL ?= $(shell grep 'device_model:' esphome/heatingpump.yaml | grep -v '^\s*\#' | head -1 | sed 's/.*"\(.*\)".*/\1/')
DEVICE_NAME  ?= $(or $(shell grep '^\s*device_name:' esphome/heatingpump.yaml | head -1 | sed 's/.*device_name: *"\{0,1\}\([a-z0-9-]*\).*/\1/'),heatingpump)
MQTT_BROKER  ?= $(shell grep 'mqtt_broker:' esphome/secrets.yaml | sed 's/mqtt_broker:[[:space:]]*//' | tr -d '"')
MQTT_USER    ?= $(shell grep 'mqtt_username:' esphome/secrets.yaml | sed 's/mqtt_username:[[:space:]]*//' | tr -d '"')
MQTT_PASS    ?= $(shell grep 'mqtt_password:' esphome/secrets.yaml | sed 's/mqtt_password:[[:space:]]*//' | tr -d '"')
# OTA target: static IP from common.yaml wifi.use_address (mDNS does not work reliably in WSL)
OTA_ADDR     ?= heatingpump.local   # override: make upload OTA_ADDR=192.168.x.y

CXX      ?= g++
# Raw CAN frame stream (can_debug.h) and its observers (can_io.h). `make test CAN_DEBUG_TEST_FLAGS=`
# tests without the stream, as in the default firmware build (substitution can_debug: "false").
CAN_DEBUG_TEST_FLAGS ?= -DHA_CAN_DEBUG_true
CXXFLAGS  = $(MODEL_TEST_FLAGS) $(CAN_DEBUG_TEST_FLAGS) -std=c++17 -Wall -Wextra \
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
                tests/test_elster_split.cpp \
                tests/signal_requests_stub.cpp
ELSTER_SRCS   = esphome/ha-stiebel-control/elster/NUtils.cpp \
                esphome/ha-stiebel-control/elster/KElsterTable.cpp
ELSTER_OBJS   = tests/NUtils.o tests/KElsterTable.o
TEST_BIN      = tests/run_tests

# ── ESPHome firmware ─────────────────────────────────────────────────────────

# Compile the ESP32-S3 variant (default user-facing config)
compile:
	cd esphome && esphome compile heatingpump.yaml

# Compile the classic ESP32 (esp32dev, 4 MB) / MCP2515 variant
compile-esp32:
	cd esphome && esphome compile ci_heatingpump_esp32.yaml

# Deprecated name: same board via the old board_esp32s2.yaml alias (it was never an ESP32-S2)
compile-s2:
	cd esphome && esphome compile ci_heatingpump_s2.yaml

# Full firmware quality gate — both board variants must compile cleanly,
# and the deprecated board_esp32s2.yaml alias must still resolve
check: test compile compile-esp32
	cd esphome && esphome config ci_heatingpump_s2.yaml > /dev/null

# Stream live logs from connected device
logs:
	cd esphome && esphome logs heatingpump.yaml

# Compile and OTA-flash to production device (uses static IP from common.yaml — mDNS is unreliable in WSL)
upload:
	cd esphome && esphome run heatingpump.yaml --device $(OTA_ADDR) --no-logs

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
# Every object includes these headers (ElsterTable.h is compiled into KElsterTable.o)
TEST_HDRS = $(wildcard esphome/ha-stiebel-control/*.h esphome/ha-stiebel-control/elster/*.h tests/*.h tests/*.inc tools/*.h)

# Compiler and flags of the last test build. Rewritten only when they change, so a new
# MODEL_TEST_FLAGS (e.g. another HA_DEVICE_MODEL) rebuilds every test object instead of
# silently linking objects built for the previous model.
TEST_FLAGS_STAMP = tests/.build-flags
$(TEST_FLAGS_STAMP): FORCE
	@echo '$(CXX) $(CXXFLAGS)' | cmp -s - $@ || echo '$(CXX) $(CXXFLAGS)' > $@

tests/%.o: tests/%.cpp $(TEST_HDRS) $(TEST_FLAGS_STAMP)
	$(CXX) $(CXXFLAGS) -c $< -o $@

tests/NUtils.o: esphome/ha-stiebel-control/elster/NUtils.cpp $(TEST_HDRS) $(TEST_FLAGS_STAMP)
	$(CXX) $(CXXFLAGS) -c $< -o $@

tests/KElsterTable.o: esphome/ha-stiebel-control/elster/KElsterTable.cpp $(TEST_HDRS) $(TEST_FLAGS_STAMP)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(TEST_BIN): $(TEST_SRCS) $(TEST_EXTRA_OBJS) $(ELSTER_OBJS) $(TEST_HDRS) $(TEST_FLAGS_STAMP)
	$(CXX) $(CXXFLAGS) $(TEST_SRCS) $(TEST_EXTRA_OBJS) $(ELSTER_OBJS) -o $(TEST_BIN)

clean-tests:
	rm -f $(TEST_BIN) $(TEST_EXTRA_OBJS) $(ELSTER_OBJS) $(TEST_FLAGS_STAMP)

# ── Mutation testing (Mull) ──────────────────────────────────────────────────

# The same translation units as $(TEST_BIN), rebuilt with clang and the Mull plugin
MUTATE_SOURCES = $(TEST_SRCS) $(TEST_EXTRA) $(ELSTER_SRCS)

# Mutate only the C++ lines changed against origin/main (override: MUTATE_BASE=<ref>).
# Shows whether the tests notice a wrong result, which line coverage does not.
mutate:
	MUTATE_SOURCES="$(MUTATE_SOURCES)" bash scripts/mutate.sh

# Mutate all C++ logic under esphome/ha-stiebel-control/ (the weekly CI workflow runs this)
mutate-all:
	MUTATE_SOURCES="$(MUTATE_SOURCES)" bash scripts/mutate.sh --all

# ── MQTT smoke test ───────────────────────────────────────────────────────────

# Run MQTT regression test against live device (120s observation window)
smoke-test:
	python3 tests/test_mqtt_smoke.py \
	  --broker "$(MQTT_BROKER)" \
	  --user "$(MQTT_USER)" \
	  --password "$(MQTT_PASS)" \
	  --model "$(DEVICE_MODEL)" \
	  --root "$(DEVICE_NAME)" \
	  --window 120

# Capture a new baseline for the current model (300s window, overwrites model file)
capture-baseline:
	python3 tests/test_mqtt_smoke.py \
	  --broker "$(MQTT_BROKER)" \
	  --user "$(MQTT_USER)" \
	  --password "$(MQTT_PASS)" \
	  --model "$(DEVICE_MODEL)" \
	  --root "$(DEVICE_NAME)" \
	  --window 300 \
	  --capture-baseline
