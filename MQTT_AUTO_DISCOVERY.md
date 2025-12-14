# MQTT Auto-Discovery Feature

## Overview
All CAN bus signals are automatically published to Home Assistant via MQTT with proper device classes, units, and friendly names.

## How It Works

### 1. Signal Detection
- Every CAN signal received is automatically detected
- Discovery message sent to Home Assistant (one-time per signal)
- State updates published on every value change

### 2. Device Organization
All sensors appear under one device:
- **Device Name**: Stiebel Eltron Heat Pump
- **Model**: WPL 13 E
- **Manufacturer**: Stiebel Eltron

### 3. Automatic Sensor Configuration

Sensors are automatically configured with:
- **Device Class**: temperature, energy, power, pressure, duration, etc.
- **Unit**: °C, kWh, W, bar, h, min, etc.
- **Icon**: MDI icons (thermometer, lightning-bolt, etc.)
- **State Class**: measurement, total_increasing
- **Friendly Name**: Auto-translated from German signal names

## Customizing Signal Mappings

Edit the `signalMappings[]` table in `esphome/ha-stiebel-control.h`:

```cpp
static const SignalConfig signalMappings[] = {
    // Pattern, DeviceClass, Unit, Icon, StateClass
    {"*TEMP*", "temperature", "°C", "mdi:thermometer", "measurement"},
    {"*ENERGIE*", "energy", "kWh", "mdi:lightning-bolt", "total_increasing"},
    // ... add your custom mappings here ...
};
```

### Pattern Matching Rules
- Use `*` as wildcard (e.g., `*TEMP*` matches anything containing "TEMP")
- Patterns are case-insensitive
- First matching pattern wins
- Last entry (`{"*", ...}`) is the default fallback

### Adding Custom Mappings

Example: Add specific configuration for a signal:
```cpp
{"MEIN_SIGNAL", "temperature", "°C", "mdi:temperature-celsius", "measurement"},
```

## MQTT Topics Structure

### Discovery Topics
```
homeassistant/sensor/heatingpump/stiebel_manager_aussentemp/config
homeassistant/sensor/heatingpump/stiebel_kessel_speicheristtemp/config
```

### State Topics
```
heatingpump/MANAGER/AUSSENTEMP/state
heatingpump/KESSEL/SPEICHERISTTEMP/state
```

## Configuration

### 1. MQTT Broker Setup
Create `esphome/secrets.yaml`:
```yaml
mqtt_broker: "homeassistant.local"
mqtt_username: "your_username"
mqtt_password: "your_password"
```

### 2. Home Assistant MQTT Integration
Ensure MQTT integration is configured in Home Assistant:
- Settings → Devices & Services → MQTT
- Broker should match your configuration

## Benefits

✅ **Automatic Discovery**: No manual sensor definitions needed  
✅ **Complete Coverage**: All 3800+ ElsterTable signals available  
✅ **Proper Types**: Temperature, energy, power sensors with correct units  
✅ **Organized**: Single device with all sensors  
✅ **Flexible**: Easy customization via mapping table  
✅ **Backward Compatible**: Existing ESPHome sensors still work  

## Monitoring

Check ESPHome logs for:
```
[MQTT Discovery] Published: stiebel_manager_aussentemp -> MANAGER Outdoor Temperature
[MQTT Discovery] Published: stiebel_kessel_speicheristtemp -> KESSEL Storage Actual Temperature
```

## Troubleshooting

**Sensors not appearing in HA:**
1. Check MQTT broker connection in ESPHome logs
2. Verify MQTT integration in Home Assistant
3. Check discovery topic: `homeassistant/#` in MQTT Explorer

**Wrong device class or unit:**
1. Edit `signalMappings[]` in `ha-stiebel-control.h`
2. Delete sensor from HA (Settings → Entities)
3. Restart ESP32 - sensor will be rediscovered

**Too many sensors:**
- Only actively received CAN signals are published
- Unused signals won't appear in Home Assistant
