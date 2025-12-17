# ha-stiebel-control Refactoring Summary

## Files Created

### config.h
New configuration file containing all system constants and thresholds. This makes the system easily customizable without modifying core logic.

**Key configurations:**
- `BLACKLIST_INVALID_THRESHOLD` - Number of consecutive invalid values before blacklisting (default: 10)
- `BLACKLIST_TIMEOUT_THRESHOLD` - Number of consecutive timeouts before blacklisting (default: 10)
- `CAN_REQUEST_TIMEOUT_MS` - CAN request timeout in milliseconds (default: 5000)
- `STARTUP_DELAY_MS` - Delay after boot before starting requests (default: 30000)
- `MAX_REQUESTS_PER_ITERATION` - Rate limiting for CAN requests (default: 5)
- `MQTT_TOPIC_PREFIX` - MQTT topic prefix (default: "heatingpump")
- `MAIN_DEVICE_ID` / `MAIN_DEVICE_NAME` - Main device identification
- Buffer size constants (`MAX_UNIQUE_ID_SIZE`, `MAX_TOPIC_SIZE`, etc.)
- Frequency definitions (`FREQ_10S`, `FREQ_30S`, `FREQ_1MIN`, etc.)
- COP calculation settings
- Invalid value detection thresholds

### signal_requests_wpl13e.h
Device-specific signal request configuration extracted from main header file. This separates heat pump model-specific code from infrastructure.

**Benefits:**
- Easy to create new configurations for different heat pump models
- Clear separation between "what to request" and "how to request it"
- Self-documented with section headers (Time/Date, Status, Temperatures, Energy, etc.)
- Includes `SIGNAL_REQUEST_COUNT` macro for array size

## Files Modified

### ha-stiebel-control.h
Major refactoring for clarity, maintainability, and scalability:

**Added:**
- Clear section headers dividing the file into logical blocks:
  - INCLUDES
  - CAN BUS MEMBER DEFINITIONS
  - MQTT AUTO-DISCOVERY CONFIGURATION
  - RUNTIME STATE TRACKING
  - SIGNAL REQUEST CONFIGURATION
  - CAN BUS HELPER FUNCTIONS
- Includes for `config.h` and `signal_requests_wpl13e.h`
- Function documentation comments

**Changed:**
- Replaced all magic numbers with named constants from `config.h`:
  - Blacklist thresholds (10 → `BLACKLIST_INVALID_THRESHOLD`)
  - Timeout values (5000 → `CAN_REQUEST_TIMEOUT_MS`)
  - Startup delay (30000 → `STARTUP_DELAY_MS`)
  - Rate limits (5 → `MAX_REQUESTS_PER_ITERATION`)
  - Request count calculation → `SIGNAL_REQUEST_COUNT`
- Moved signal request table to separate file
- Improved code organization with logical grouping

**Benefits:**
- Easier to understand code structure
- Single point of configuration (config.h)
- No hardcoded values scattered throughout code
- Better separation of concerns

### common.yaml
Updated includes section to reference new configuration files:
- Added `config.h` include (first, as it defines constants)
- Added `signal_requests_wpl13e.h` include (device-specific signals)
- Maintained existing elster library includes
- Maintained main header include

## Benefits of Refactoring

### 1. Maintainability
- Configuration changes in one place (`config.h`)
- Device-specific signals isolated in separate files
- Clear section headers make code navigation easier
- Self-documenting code structure

### 2. Scalability
- Easy to support multiple heat pump models:
  - Create `signal_requests_wpl25a.h` for WPL25A model
  - Create `signal_requests_wpm2.h` for WPM2 model
  - Switch between models by changing one include line
- Configuration constants can be overridden per-device if needed
- Clear extension points for new features

### 3. Robustness
- Named constants prevent typos and magic number bugs
- Single source of truth for all thresholds
- Easier to test different configurations
- Better error messages (e.g., shows threshold in logs)

### 4. Clarity
- Section headers clearly divide functionality
- Configuration separated from implementation
- Device-specific code separated from infrastructure
- Constants named according to their purpose

## Migration Guide for Other Heat Pump Models

To support a different Stiebel Eltron heat pump model:

1. **Copy signal request file:**
   ```
   cp signal_requests_wpl13e.h signal_requests_YOURMODEL.h
   ```

2. **Edit the new file:**
   - Update signal list for your model (consult manual)
   - Adjust request frequencies as needed
   - Add/remove sections based on model capabilities

3. **Update your device YAML:**
   ```yaml
   includes:
     - ha-stiebel-control/config.h
     - ha-stiebel-control/signal_requests_YOURMODEL.h  # <-- Change this line
     - ha-stiebel-control/elster/...
   ```

4. **Optionally customize config.h:**
   - Can override constants with build flags if needed
   - Or create model-specific config_YOURMODEL.h

## No Functional Changes

This refactoring maintains 100% functional compatibility:
- All algorithms remain identical
- Same blacklisting logic
- Same MQTT discovery behavior
- Same COP calculations
- Same request scheduling

Only the organization and configurability improved.
