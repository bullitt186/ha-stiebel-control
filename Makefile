.PHONY: compile compile-s2 check logs upload config test test-all clean-tests

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
                tests/test_can_logic.cpp
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

# Compile and OTA-flash to connected device
upload:
	cd esphome && esphome run heatingpump.yaml

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
