/* Stub for driver/twai.h — host test build only. */
#pragma once
#include <cstdint>

typedef enum { TWAI_STATE_STOPPED, TWAI_STATE_RUNNING, TWAI_STATE_BUS_OFF, TWAI_STATE_RECOVERING } twai_state_t;

typedef struct {
    twai_state_t state;
    uint32_t tx_error_counter;
    uint32_t rx_error_counter;
    uint32_t arb_lost_count;
    uint32_t bus_error_count;
} twai_status_info_t;

typedef int esp_err_t;
#define ESP_OK 0

inline esp_err_t twai_get_status_info(twai_status_info_t* info) {
    info->state = TWAI_STATE_RUNNING;
    info->tx_error_counter = 0;
    info->rx_error_counter = 0;
    info->arb_lost_count   = 0;
    info->bus_error_count  = 0;
    return ESP_OK;
}
