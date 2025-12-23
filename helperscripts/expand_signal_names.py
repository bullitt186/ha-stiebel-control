#!/usr/bin/env python3
"""
Expand signal names in ElsterTable using recursive abbreviation matching.

Algorithm:
1. Search for longest matching fragment in abbrevList
2. Replace it with friendly name
3. Recursively process left and right remaining fragments
4. Continue until fragments are 1 character long
5. Combine into friendly German name with title case
"""

import re
from typing import List, Tuple, Optional

# Abbreviation list sorted by length (longest first)
ABBREV_LIST = [
    ("AUFNAHMELEISTUNG", "Aufnahmeleistung"),  # 16 chars
    ("LUEFTUNGSSTUFE", "Lüftungsstufe"),  # 14 chars
    ("LEISTUNGSZWANG", "Leistungszwang"),   # 14 chars
    ("FEHLERMELDUNG", "Fehlermeldung"),# 13 chars
    ("VOLUMENSTROM", "Volumenstrom"),  # 12 chars
    ("QUELLENPUMPE", "Quellenpumpe"),  # 12 chars
    ("STUETZSTELLE", "Stützstelle"),   # 12 chars
    ("HILFSKESSEL", "Hilfskessel"),    # 11 chars
    ("BETRIEBSART", "Betriebsart"),    # 11 chars
    ("VERDAMPFER", "Verdampfer"),      # 10 chars
    ("VERDICHTER", "Verdichter"),      # 10 chars
    ("DURCHFLUSS", "Durchfluss"),      # 10 chars
    ("TEMPERATUR", "Temperatur"),      # 10 chars
    ("TEMPORALE", "Temporale"),        # 9 chars
    ("RUECKLAUF", "Rücklauf"),         # 9 chars
    ("LAUFZEIT", "Laufzeit"),          # 8 chars
    ("EINSTELL", "Einstellung"),       # 8 chars
    ("LEISTUNG", "Leistung"),          # 8 chars
    ("KUEHLUNG", "Kühlung"),           # 8 chars
    ("BIVALENT", "Bivalent"),          # 8 chars
    ("PARALLEL", "Parallel"),          # 8 chars
    ("FREQUENZ", "Frequenz"),          # 8 chars
    ("DREHZAHL", "Drehzahl"),          # 8 chars
    ("SPEICHER", "Speicher"),          # 8 chars
    ("SPANNUNG", "Spannung"),          # 8 chars
    ("VORLAUF", "Vorlauf"),            # 7 chars
    ("SAMMLER", "Sammler"),            # 7 chars
    ("BETRIEB", "Betrieb"),            # 7 chars
    ("HEIZUNG", "Heizung"),            # 7 chars
    ("ERTRAG", "Ertrag"),              # 6 chars
    ("AUSSEN", "Außen"),               # 6 chars
    ("MINUTE", "Minute"),               # 6 chars
    ("SOCKEL", "Sockel"),              # 6 chars
    ("KESSEL", "Kessel"),              # 6 chars
    ("DAUER", "Dauer"),                # 5 chars
    ("DRUCK", "Druck"),                # 5 chars
    ("STROM", "Strom"),                # 5 chars
    ("LUEFT", "Lüftung"),              # 5 chars
    ("PUMPE", "Pumpe"),                # 5 chars
    ("VERD", "Verdichter"),            # 4 chars
    ("TEMP", "Temperatur"),            # 4 chars
    ("HEIZ", "Heizung"),               # 4 chars
    ("RAUM", "Raum"),                  # 4 chars
    ("SOLL", "Soll"),                  # 4 chars
    ("MAX", "Maximum"),                # 3 chars
    ("MIN", "Minimum"),                # 3 chars
    ("SUM", "Summe"),                  # 3 chars
    ("TAG", "Tag"),                    # 3 chars
    ("IST", "Ist"),                    # 3 chars
    ("FKT", "Funktion"),               # 3 chars
    ("HZG", "Heizung"),                # 3 chars
    ("WW", "Warmwasser"),              # 2 chars
    ("WP", "Wärmepumpe"),              # 2 chars
    ("EL", "Elektrisch"),              # 2 chars
    ("LZ", "Laufzeit")                 # 2 chars
]

def split_fragment(fragment: str) -> str:
    """
    Recursively split a signal name fragment using abbrevList.
    Returns expanded friendly name with spaces between recognized parts.
    """
    # Stop if fragment is too short or empty
    if len(fragment) <= 1:
        return fragment
    
    # Convert to uppercase for case-insensitive search
    upper_fragment = fragment.upper()
    
    # Try each abbreviation (already sorted longest first)
    for abbrev, full in ABBREV_LIST:
        pos = upper_fragment.find(abbrev)
        if pos != -1:
            # Found! Split into left, match, right
            left = fragment[:pos]
            match = full  # Use friendly name
            right = fragment[pos + len(abbrev):]
            
            # Recursively process left and right parts
            processed_left = split_fragment(left) if left else ""
            processed_right = split_fragment(right) if right else ""
            
            # Combine with spaces
            result_parts = []
            if processed_left:
                result_parts.append(processed_left)
            result_parts.append(match)
            if processed_right:
                result_parts.append(processed_right)
            
            return " ".join(result_parts)
    
    # No match found, return fragment as-is
    return fragment

def expand_signal_name(signal_name: str) -> str:
    """
    Expand a signal name by splitting concatenated abbreviations.
    E.g., "WPVORLAUFIST" -> "Wärmepumpe Vorlauf Ist"
    """
    if not signal_name:
        return signal_name
    
    # Step 1: Replace underscores with spaces
    name = signal_name.replace('_', ' ')
    
    # Step 2: Process each space-separated token
    tokens = name.split()
    processed_tokens = []
    
    for token in tokens:
        # Recursively split this token
        expanded = split_fragment(token)
        processed_tokens.append(expanded)
    
    # Step 3: Join with spaces
    result = " ".join(processed_tokens)
    
    # Step 4: Clean up excess whitespace
    result = " ".join(result.split())
    
    # Step 5: Replace German umlaut combinations with proper umlauts
    # Do this before title casing to handle both upper and lower case
    result = result.replace('Ue', 'Ü').replace('ue', 'ü')
    result = result.replace('Ae', 'Ä').replace('ae', 'ä')
    result = result.replace('Oe', 'Ö').replace('oe', 'ö')
    
    # Step 6: Convert to title case (each word capitalized)
    # Preserve umlauts correctly
    words = result.split()
    title_cased = []
    for word in words:
        if word:
            # Capitalize first letter, lowercase rest
            title_cased.append(word[0].upper() + word[1:].lower())
    
    return " ".join(title_cased)

def parse_elster_entry(line: str) -> Optional[Tuple[str, str]]:
    """
    Parse an ElsterTable entry line and extract signal name.
    Returns (full_line, signal_name) or None if not an entry.
    """
    # Match lines like: { "SIGNAL_NAME", 0x1234, et_type, ...
    match = re.match(r'\s*\{\s*"([^"]+)",', line)
    if match:
        return (line, match.group(1))
    return None

def update_entry_with_friendly_name(line: str, signal_name: str, friendly_name: str) -> str:
    """
    Update an ElsterTable entry with the friendly name.
    Replaces the friendlyName field (4th field after Type).
    This will OVERWRITE existing friendly names.
    """
    # If friendly name is same as signal name, skip
    if friendly_name.upper() == signal_name.upper():
        return line
    
    # Check if this is a blacklisted entry (has ", true, false" near end)
    if ', true, false' in line:
        return line  # Skip blacklisted entries
    
    import re
    # Pattern 1: Match NULL in friendlyName position: et_TYPE, NULL, or 0, NULL,
    pattern_null = r'(,\s*(?:et_\w+|\d+),\s*)NULL(,)'
    # Pattern 2: Match existing friendly name: et_TYPE, "something", or 0, "something",
    pattern_existing = r'(,\s*(?:et_\w+|\d+),\s*)\"[^\"]*\"(,)'
    
    # Replace NULL or existing friendly name with new expanded name
    if re.search(pattern_null, line):
        new_line = re.sub(pattern_null, rf'\1"{friendly_name}"\2', line, count=1)
        return new_line
    elif re.search(pattern_existing, line):
        new_line = re.sub(pattern_existing, rf'\1"{friendly_name}"\2', line, count=1)
        return new_line
    
    return line  # Can't find pattern, return unchanged


def process_elster_table(input_file: str, output_file: str):
    """
    Process ElsterTable.h and add friendly names to all entries.
    """
    print(f"Processing {input_file}...")
    
    stats = {
        'total': 0,
        'updated': 0,
        'skipped_blacklisted': 0,
        'skipped_has_name': 0,
        'skipped_same': 0,
        'unchanged': 0  # Already has correct friendly name
    }
    
    with open(input_file, 'r', encoding='utf-8') as f:
        lines = f.readlines()
    
    output_lines = []
    
    for line in lines:
        parsed = parse_elster_entry(line)
        
        if parsed:
            full_line, signal_name = parsed
            stats['total'] += 1
            
            # Check if blacklisted
            if ', true, false' in line:
                stats['skipped_blacklisted'] += 1
                output_lines.append(line)
                continue
            
            # Expand the signal name
            friendly_name = expand_signal_name(signal_name)
            
            # Skip if friendly name is same as original (avoid redundancy)
            if friendly_name.upper() == signal_name.upper():
                stats['skipped_same'] += 1
                output_lines.append(line)
                continue
            
            # Update the entry
            new_line = update_entry_with_friendly_name(line, signal_name, friendly_name)
            
            if new_line != line:
                stats['updated'] += 1
                if stats['updated'] <= 10:  # Show first 10 examples
                    print(f"  {signal_name} -> {friendly_name}")
            else:
                stats['unchanged'] += 1
            
            output_lines.append(new_line)
        else:
            # Not an entry line, keep as-is
            output_lines.append(line)
    
    # Write output
    with open(output_file, 'w', encoding='utf-8') as f:
        f.writelines(output_lines)
    
    print(f"\nStatistics:")
    print(f"  Total entries: {stats['total']}")
    print(f"  Updated with friendly names: {stats['updated']}")
    print(f"  Unchanged (already correct): {stats['unchanged']}")
    print(f"  Skipped (blacklisted): {stats['skipped_blacklisted']}")
    print(f"  Skipped (already has name): {stats['skipped_has_name']}")
    print(f"  Skipped (same as original): {stats['skipped_same']}")
    print(f"\nOutput written to {output_file}")

if __name__ == "__main__":
    import sys
    
    if len(sys.argv) > 1:
        input_file = sys.argv[1]
    else:
        input_file = "esphome/ha-stiebel-control/elster/ElsterTable.h"
    
    output_file = input_file.replace('.h', '_expanded.h')
    
    process_elster_table(input_file, output_file)
    
    print(f"\nTo apply changes:")
    print(f"  mv {input_file} {input_file}.backup")
    print(f"  mv {output_file} {input_file}")
