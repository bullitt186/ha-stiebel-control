/* Stub for driver/twai.h — host test build only (field set of ESP-IDF 5.5). */
#pragma once
#include <cstdint>

typedef enum { TWAI_STATE_STOPPED, TWAI_STATE_RUNNING, TWAI_STATE_BUS_OFF, TWAI_STATE_RECOVERING } twai_state_t;

typedef struct {
    twai_state_t state;
    uint32_t msgs_to_tx;
    uint32_t msgs_to_rx;
    uint32_t tx_error_counter;
    uint32_t rx_error_counter;
    uint32_t tx_failed_count;
    uint32_t rx_missed_count;
    uint32_t rx_overrun_count;
    uint32_t arb_lost_count;
    uint32_t bus_error_count;
} twai_status_info_t;

typedef int esp_err_t;
#define ESP_OK 0

// Tests may preset values (state stays RUNNING)
inline twai_status_info_t& fake_twai_status() { static twai_status_info_t s{}; return s; }

inline esp_err_t twai_get_status_info(twai_status_info_t* info) {
    *info = fake_twai_status();
    info->state = TWAI_STATE_RUNNING;
    return ESP_OK;
}

inline esp_err_t twai_initiate_recovery() { return ESP_OK; }
inline esp_err_t twai_start() { return ESP_OK; }
