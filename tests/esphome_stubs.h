/*
 * Minimal stubs that satisfy the preprocessor symbols used inside
 * #if !defined(HA_DUMMY_BUILD) in ha-stiebel-control.h, so that
 * pure-logic functions (generate_read_id, processCanMessage, hash, …)
 * can be compiled and tested on the host.
 *
 * Only the symbols actually referenced by the tested functions are defined.
 * Nothing here emulates real ESPHome behaviour.
 */
#pragma once
#include <cstdio>
#include <cstdint>
#include <vector>

// ── Device model default (normally injected via platformio build_flags) ──────
#ifndef HA_DEVICE_MODEL
#define HA_DEVICE_MODEL "test_model"
#endif


// ── Logging ──────────────────────────────────────────────────────────────────
// Swallow all ESP_LOGx calls so functions that log remain compilable.
#define ESP_LOGI(tag, fmt, ...) ((void)0)
#define ESP_LOGD(tag, fmt, ...) ((void)0)
#define ESP_LOGW(tag, fmt, ...) ((void)0)
#define ESP_LOGE(tag, fmt, ...) ((void)0)

// ── Arduino / FreeRTOS / ESP-IDF ─────────────────────────────────────────────
inline void delay(uint32_t) {}
inline unsigned long millis() { return 0; }
inline uint32_t esp_random() { return 42; }

// ── CAN bus stub ─────────────────────────────────────────────────────────────
struct FakeCanBus {
    std::vector<std::vector<uint8_t>> sent;
    void send_data(uint32_t /*id*/, bool /*ext*/, const std::vector<uint8_t>& data) {
        sent.push_back(data);
    }
};
inline FakeCanBus& fake_can() { static FakeCanBus b; return b; }
// id(my_can) → fake_can()
#define my_can fake_can()

// The id() macro used in ESPHome — map every named id to a global accessor
#define id(x) x

// ── MQTT stub ────────────────────────────────────────────────────────────────
struct FakeMqttClient {
    struct Published { std::string topic; std::string payload; };
    std::vector<Published> messages;
    // Overloads for const char* and std::string topics (ha-stiebel-control.h uses both)
    void publish(const char* topic, const char* payload, size_t /*len*/, int /*qos*/, bool /*retain*/) {
        messages.push_back({topic, payload ? payload : ""});
    }
    void publish(const std::string& topic, const char* payload, size_t len, int qos, bool retain) {
        publish(topic.c_str(), payload, len, qos, retain);
    }
    void clear() { messages.clear(); }
};
inline FakeMqttClient& mqtt_client_instance() { static FakeMqttClient c; return c; }
#define mqtt_client mqtt_client_instance()

// ── NVS / Preferences stub ───────────────────────────────────────────────────
// ESPPreferenceObject is only needed by EspHomeSgReadyIO which we don't test here.
// Defined as a no-op struct so the header compiles.
struct ESPPreferenceObject {
    template<typename T> bool save(T*) { return true; }
    template<typename T> bool load(T*) { return false; }
};
struct ESPPreferences {
    template<typename T>
    ESPPreferenceObject make_preference(uint32_t /*key*/, bool /*flash*/) {
        return {};
    }
};
inline ESPPreferences* global_preferences_instance() {
    static ESPPreferences p; return &p;
}
#define global_preferences global_preferences_instance()

// ── ESPHome sensor publish_state stub ────────────────────────────────────────
struct FakeSensor {
    float last_state = 0.0f;
    void publish_state(float v) { last_state = v; }
};
// Named sensor globals referenced from common.yaml lambdas — not used in our tests
// but needed so the header compiles when included alongside HA_DUMMY_BUILD.
inline FakeSensor& sg_ready_boost_state3_sensor_instance() { static FakeSensor s; return s; }
inline FakeSensor& sg_ready_boost_state4_sensor_instance() { static FakeSensor s; return s; }
struct FakeTextSensor {
    std::string last_state;
    void publish_state(const char* v) { last_state = v; }
};
inline FakeTextSensor& SG_READY_CURRENT_STATE_instance() { static FakeTextSensor s; return s; }
#define sg_ready_boost_state3_sensor sg_ready_boost_state3_sensor_instance()
#define sg_ready_boost_state4_sensor sg_ready_boost_state4_sensor_instance()
#define SG_READY_CURRENT_STATE SG_READY_CURRENT_STATE_instance()
