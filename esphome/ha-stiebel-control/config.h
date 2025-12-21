/*
 * Configuration Constants for ha-stiebel-control
 * 
 * This file contains all configurable parameters and thresholds.
 * Edit these values to customize system behavior without touching core logic.
 */

#ifndef CONFIG_H
#define CONFIG_H

// Timeout for CAN request responses (milliseconds)
#define CAN_REQUEST_TIMEOUT_MS 5000

// ============================================================================
// REQUEST MANAGER SETTINGS
// ============================================================================

// Delay after boot before starting signal requests (milliseconds)
#define STARTUP_DELAY_MS 30000

// Maximum number of requests to send per iteration (rate limiting)
#define MAX_REQUESTS_PER_ITERATION 1

// Minimum random delay between requests (milliseconds)
#define MIN_RANDOM_DELAY_MS 200

// Maximum random delay between requests (milliseconds)
#define MAX_RANDOM_DELAY_MS 1000

// ============================================================================
// MQTT SETTINGS
// ============================================================================

// MQTT topic prefix for all messages
#define MQTT_TOPIC_PREFIX "heatingpump"

// MQTT discovery prefix (Home Assistant default)
#define MQTT_DISCOVERY_PREFIX "homeassistant"

// Main device identifier
#define MAIN_DEVICE_ID "stiebel_eltron_wpl13e"

// Main device name
#define MAIN_DEVICE_NAME "Stiebel Eltron Wärmepumpe"

// ============================================================================
// BUFFER SIZES
// ============================================================================

// Maximum size for unique ID strings
#define MAX_UNIQUE_ID_SIZE 128

// Maximum size for MQTT topic strings
#define MAX_TOPIC_SIZE 256

// Maximum size for log messages
#define MAX_LOG_MSG_SIZE 256

// Maximum size for formatted values
#define MAX_VALUE_SIZE 32

// ============================================================================
// TIMING INTERVALS (for use in signalRequests table)
// ============================================================================

// Request frequency definitions (in seconds)
#define FREQ_10S 10
#define FREQ_30S 30
#define FREQ_1MIN 60
#define FREQ_5MIN 300
#define FREQ_10MIN 600
#define FREQ_30MIN 1800
#define FREQ_60MIN 3600

// ============================================================================
// CALCULATED SENSOR SETTINGS
// ============================================================================

// Update frequency for calculated sensors (in seconds)
// These values determine how often calculated values are published
// Setting lower values increases MQTT traffic but provides more responsive updates

// Delta T sensors update frequency (based on temperature sensor frequency)
#define CALC_DELTA_T_FREQUENCY FREQ_30S

// Compressor active sensor update frequency (based on VERDICHTER frequency)
#define CALC_COMPRESSOR_FREQUENCY FREQ_30S

// Date/Time sensors update frequency (based on date/time signal frequency)
#define CALC_DATETIME_FREQUENCY FREQ_1MIN

// Betriebsart sensor update frequency (based on SOMMERBETRIEB frequency)
#define CALC_BETRIEBSART_FREQUENCY FREQ_1MIN

// COP sensors update frequency (based on energy counter frequency)
#define CALC_COP_FREQUENCY FREQ_10MIN

// ============================================================================
// COP CALCULATION SETTINGS
// ============================================================================

// Minimum divisor value to avoid division by zero
#define COP_MIN_DIVISOR 0.001f

// Number of decimal places for COP values
#define COP_DECIMAL_PLACES 2

// ============================================================================
// INVALID VALUE DETECTION
// ============================================================================

// Invalid marker values (exact matches)
#define INVALID_VALUE_NEG_255 -255.0f
#define INVALID_VALUE_NEG_32768 -32768.0f
#define INVALID_VALUE_POS_32767 32767.0f

// Invalid value range limits
#define INVALID_VALUE_MIN -300.0f
#define INVALID_VALUE_MAX 1000.0f

// Epsilon for floating point comparisons
#define INVALID_VALUE_EPSILON 0.01f

// ============================================================================
// NON-BLOCKING DELAY SETTINGS
// ============================================================================

// Minimum random delay between CAN requests (milliseconds)
#define MIN_REQUEST_DELAY_MS 200

// Maximum random delay between CAN requests (milliseconds)
#define MAX_REQUEST_DELAY_MS 1000

#endif // CONFIG_H
