#!/usr/bin/env python3
"""
Script to migrate ElsterTable entries to new format with metadata fields.
Reads existing ElsterTable.h and outputs updated version with new struct fields.
"""

import re
import sys

# Blacklisted signals (from signal_requests_wpl13e.h)
BLACKLISTED = {
    "ACCESS_EEPROM", "ANFAHRENT", "GERAETE_ID", "GERAETEKONFIGURATION",
    "HARDWARE_NUMMER", "INDEX_NOT_FOUND", "INITIALISIERUNG", "PARAMETERSATZ",
    "QUELLENPUMPEN_STATUS", "ZWISCHENEINSPRITZUNG_ISTTEMP", "BUSKONFIGURATION",
    "BUSKONTROLLE", "SOFTWARE_NUMMER", "SOFTWARE_VERSION", "SPEICHERBEDARF",
    "FATAL_ERROR", "FEHLER_PARAMETERSATZ_IWS", "FEHLERMELDUNG",
    "K_OS_RMX_RESERVE_INFO3", "SCHALTFKT_IWS", "SOLAR_KOLLEKTOR_3_I_ANTEIL",
    "STUETZSTELLE_ND1", "STUETZSTELLE_ND2", "STUETZSTELLE_HD1", "STUETZSTELLE_HD2",
    "LZ_VERD_1_HEIZBETRIEB", "LZ_VERD_2_HEIZBETRIEB", "LZ_VERD_1_2_HEIZBETRIEB",
    "LZ_VERD_1_WW_BETRIEB", "LZ_VERD_2_WW_BETRIEB", "LZ_VERD_1_2_WW_BETRIEB",
    "LZ_VERD_1_KUEHLBETRIEB", "LZ_VERD_2_KUEHLBETRIEB", "LZ_VERD_1_2_KUEHLBETRIEB",
    "LAUFZEIT_VERD_BEI_SPEICHERBEDARF", "SAMMEL_RELAISSTATUS",
    "LUEFT_PASSIVKUEHLUNG_UEBER_FORTLUEFTER", "TEMPORALE_LUEFTUNGSSTUFE_DAUER",
    "TEILVORRANG_WW"
}

# Active signals with custom metadata (45 signals from signal_requests)
ACTIVE_SIGNALS_METADATA = {
    # Date/Time signals
    "JAHR": {"name": "Jahr", "component": "sensor", "class": "", "unit": "", "state": "", "icon": "mdi:calendar"},
    "MONAT": {"name": "Monat", "component": "sensor", "class": "", "unit": "", "state": "", "icon": "mdi:calendar-month"},
    "TAG": {"name": "Tag", "component": "sensor", "class": "", "unit": "", "state": "", "icon": "mdi:calendar-today"},
    "STUNDE": {"name": "Stunde", "component": "sensor", "class": "", "unit": "h", "state": "", "icon": "mdi:clock-outline"},
    "MINUTE": {"name": "Minute", "component": "sensor", "class": "", "unit": "min", "state": "", "icon": "mdi:clock-outline"},
    "SEKUNDE": {"name": "Sekunde", "component": "sensor", "class": "", "unit": "s", "state": "", "icon": "mdi:clock-outline"},
    
    # Status signals
    "WP_STATUS": {"name": "WP Status", "component": "sensor", "class": "", "unit": "", "state": "", "icon": "mdi:heat-pump"},
    "EVU_SPERRE_AKTIV": {"name": "EVU Sperre Aktiv", "component": "binary_sensor", "class": "lock", "unit": "", "state": "", "icon": "mdi:lock"},
    "ABTAUUNGAKTIV": {"name": "Abtauung Aktiv", "component": "binary_sensor", "class": "", "unit": "", "state": "", "icon": "mdi:snowflake-melt"},
    "BETRIEBSART_WP": {"name": "Betriebsart WP", "component": "sensor", "class": "", "unit": "", "state": "", "icon": "mdi:hvac"},
    "PROGRAMMSCHALTER": {"name": "Programmschalter", "component": "sensor", "class": "", "unit": "", "state": "", "icon": "mdi:dip-switch"},
    "SOMMERBETRIEB": {"name": "Sommerbetrieb", "component": "binary_sensor", "class": "", "unit": "", "state": "", "icon": "mdi:weather-sunny"},
    
    # Temperature setpoints
    "KESSELSOLLTEMP": {"name": "Kessel Soll Temp", "component": "sensor", "class": "temperature", "unit": "°C", "state": "measurement", "icon": "mdi:thermometer"},
    "SPEICHERSOLLTEMP": {"name": "Speicher Soll Temp", "component": "sensor", "class": "temperature", "unit": "°C", "state": "measurement", "icon": "mdi:thermometer"},
    "RAUMSOLLTEMP_I": {"name": "Raum Soll Temp I", "component": "sensor", "class": "temperature", "unit": "°C", "state": "measurement", "icon": "mdi:home-thermometer"},
    "RAUMSOLLTEMP_II": {"name": "Raum Soll Temp II", "component": "sensor", "class": "temperature", "unit": "°C", "state": "measurement", "icon": "mdi:home-thermometer"},
    "RAUMSOLLTEMP_III": {"name": "Raum Soll Temp III", "component": "sensor", "class": "temperature", "unit": "°C", "state": "measurement", "icon": "mdi:home-thermometer"},
    "RAUMSOLLTEMP_NACHT": {"name": "Raum Soll Temp Nacht", "component": "sensor", "class": "temperature", "unit": "°C", "state": "measurement", "icon": "mdi:home-thermometer"},
    "EINSTELL_SPEICHERSOLLTEMP": {"name": "Einst. Speicher Soll Temp", "component": "sensor", "class": "temperature", "unit": "°C", "state": "measurement", "icon": "mdi:thermometer"},
    
    # Temperature measurements
    "AUSSENTEMP": {"name": "Außen Temp", "component": "sensor", "class": "temperature", "unit": "°C", "state": "measurement", "icon": "mdi:thermometer"},
    "SAMMLERISTTEMP": {"name": "Sammler Ist Temp", "component": "sensor", "class": "temperature", "unit": "°C", "state": "measurement", "icon": "mdi:thermometer"},
    "SPEICHERISTTEMP": {"name": "Speicher Ist Temp", "component": "sensor", "class": "temperature", "unit": "°C", "state": "measurement", "icon": "mdi:thermometer"},
    "VORLAUFISTTEMP": {"name": "Vorlauf Ist Temp", "component": "sensor", "class": "temperature", "unit": "°C", "state": "measurement", "icon": "mdi:thermometer-lines"},
    "RUECKLAUFISTTEMP": {"name": "Rücklauf Ist Temp", "component": "sensor", "class": "temperature", "unit": "°C", "state": "measurement", "icon": "mdi:thermometer-lines"},
    "WPVORLAUFIST": {"name": "WP Vorlauf Ist", "component": "sensor", "class": "temperature", "unit": "°C", "state": "measurement", "icon": "mdi:thermometer-lines"},
    "VERDAMPFERTEMP": {"name": "Verdampfer Temp", "component": "sensor", "class": "temperature", "unit": "°C", "state": "measurement", "icon": "mdi:thermometer"},
    "KONDENSATORTEMP": {"name": "Kondensator Temp", "component": "sensor", "class": "temperature", "unit": "°C", "state": "measurement", "icon": "mdi:thermometer"},
    
    # Power/Energy
    "VERDICHTER": {"name": "Verdichter", "component": "sensor", "class": "power", "unit": "W", "state": "measurement", "icon": "mdi:pump"},
    "FREQUENZ_VD": {"name": "Frequenz VD", "component": "sensor", "class": "frequency", "unit": "Hz", "state": "measurement", "icon": "mdi:sine-wave"},
    "EL_AUFNAHMELEISTUNG_WW_TAG_WH": {"name": "El. Aufn. WW Tag", "component": "sensor", "class": "energy", "unit": "Wh", "state": "total_increasing", "icon": "mdi:flash"},
    "EL_AUFNAHMELEISTUNG_WW_SUM_MWH": {"name": "El. Aufn. WW Summe", "component": "sensor", "class": "energy", "unit": "MWh", "state": "total_increasing", "icon": "mdi:flash"},
    "EL_AUFNAHMELEISTUNG_HEIZ_TAG_WH": {"name": "El. Aufn. Heiz Tag", "component": "sensor", "class": "energy", "unit": "Wh", "state": "total_increasing", "icon": "mdi:flash"},
    "EL_AUFNAHMELEISTUNG_HEIZ_SUM_MWH": {"name": "El. Aufn. Heiz Summe", "component": "sensor", "class": "energy", "unit": "MWh", "state": "total_increasing", "icon": "mdi:flash"},
    "WAERMEERTRAG_WW_TAG_WH": {"name": "Wärmeertrag WW Tag", "component": "sensor", "class": "energy", "unit": "Wh", "state": "total_increasing", "icon": "mdi:fire"},
    "WAERMEERTRAG_WW_SUM_MWH": {"name": "Wärmeertrag WW Summe", "component": "sensor", "class": "energy", "unit": "MWh", "state": "total_increasing", "icon": "mdi:fire"},
    "WAERMEERTRAG_HEIZ_TAG_WH": {"name": "Wärmeertrag Heiz Tag", "component": "sensor", "class": "energy", "unit": "Wh", "state": "total_increasing", "icon": "mdi:fire"},
    "WAERMEERTRAG_HEIZ_SUM_MWH": {"name": "Wärmeertrag Heiz Summe", "component": "sensor", "class": "energy", "unit": "MWh", "state": "total_increasing", "icon": "mdi:fire"},
    "WAERMEERTRAG_2WE_WW_SUM_MWH": {"name": "Wärmeertrag 2WE WW Summe", "component": "sensor", "class": "energy", "unit": "MWh", "state": "total_increasing", "icon": "mdi:fire"},
    "WAERMEERTRAG_2WE_HEIZ_SUM_MWH": {"name": "Wärmeertrag 2WE Heiz Summe", "component": "sensor", "class": "energy", "unit": "MWh", "state": "total_increasing", "icon": "mdi:fire"},
    
    # Runtime counters
    "LAUFZEIT_VERD_HEIZBETRIEB": {"name": "LZ Verd. Heizbetrieb", "component": "sensor", "class": "duration", "unit": "h", "state": "total_increasing", "icon": "mdi:timer"},
    "LAUFZEIT_VERD_WW_BETRIEB": {"name": "LZ Verd. WW Betrieb", "component": "sensor", "class": "duration", "unit": "h", "state": "total_increasing", "icon": "mdi:timer"},
    "IMPULSE_VERD_HEIZBETRIEB": {"name": "Impulse Verd. Heizbetrieb", "component": "sensor", "class": "", "unit": "", "state": "total_increasing", "icon": "mdi:counter"},
    "IMPULSE_VERD_WW_BETRIEB": {"name": "Impulse Verd. WW Betrieb", "component": "sensor", "class": "", "unit": "", "state": "total_increasing", "icon": "mdi:counter"},
}

def parse_entry(line):
    """Parse an ElsterTable entry line."""
    # Match: { "NAME", 0x1234, type_value } or { "NAME", 0x1234, et_type_name }
    # Handle both numeric types (0, 1, 2) and symbolic names (et_dec_val, et_cent_val)
    match = re.match(r'\s*\{\s*"([^"]+)"\s*,\s*(0x[0-9A-Fa-f]+)\s*,\s*([^}]+?)\s*\}', line)
    if match:
        type_val = match.group(3).strip()
        # Remove trailing comma if present
        type_val = type_val.rstrip(',')
        return {
            'name': match.group(1),
            'index': match.group(2),
            'type': type_val
        }
    return None

def convert_entry(entry):
    """Convert old format entry to new format with metadata."""
    name = entry['name']
    index = entry['index']
    type_val = entry['type']
    
    # Check if blacklisted
    is_blacklisted = name in BLACKLISTED
    
    # Check if has custom metadata
    has_metadata = name in ACTIVE_SIGNALS_METADATA
    
    if is_blacklisted:
        # Blacklisted: minimal metadata
        return f'  {{ "{name}", {index}, {type_val}, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, true, false }}'
    elif has_metadata:
        # Active signal: full metadata
        meta = ACTIVE_SIGNALS_METADATA[name]
        friendly = meta['name']
        component = meta['component']
        dev_class = meta['class']
        unit = meta['unit']
        state_class = meta['state']
        icon = meta['icon']
        
        return f'  {{ "{name}", {index}, {type_val}, "{friendly}", "{component}", "{dev_class}", "{unit}", "{state_class}", "{icon}", NULL, NULL, false, true }}'
    else:
        # Default signal: NULL metadata
        return f'  {{ "{name}", {index}, {type_val}, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, false, false }}'

def process_file(input_file, output_file):
    """Process ElsterTable.h file."""
    with open(input_file, 'r', encoding='utf-8') as f:
        lines = f.readlines()
    
    with open(output_file, 'w', encoding='utf-8') as f:
        in_table = False
        for line in lines:
            # Detect start of table
            if 'static const ElsterIndex ElsterTable[]' in line:
                in_table = True
                f.write(line)
                continue
            
            # Detect end of table
            if in_table and line.strip().startswith('};'):
                in_table = False
                f.write(line)
                continue
            
            # Process table entries
            if in_table:
                # Skip comment lines
                if line.strip().startswith('//'):
                    f.write(line)
                    continue
                
                # Try to parse entry
                entry = parse_entry(line)
                if entry:
                    new_line = convert_entry(entry)
                    # Preserve comma at end if present
                    if ',' in line and not new_line.endswith(','):
                        new_line += ','
                    f.write(new_line + '\n')
                else:
                    # Keep line as-is (might be empty, comment, etc.)
                    f.write(line)
            else:
                # Outside table: keep as-is
                f.write(line)

if __name__ == '__main__':
    input_file = 'esphome/ha-stiebel-control/elster/ElsterTable.h'
    output_file = 'esphome/ha-stiebel-control/elster/ElsterTable_new.h'
    
    print(f"Processing {input_file}...")
    process_file(input_file, output_file)
    print(f"Output written to {output_file}")
    print(f"\nStatistics:")
    print(f"  - Blacklisted signals: {len(BLACKLISTED)}")
    print(f"  - Active signals with metadata: {len(ACTIVE_SIGNALS_METADATA)}")
    print(f"\nNext steps:")
    print(f"  1. Review {output_file}")
    print(f"  2. Replace {input_file} with {output_file}")
    print(f"  3. Update consuming code in ha-stiebel-control.h")
