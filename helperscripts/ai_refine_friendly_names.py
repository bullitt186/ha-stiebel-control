#!/usr/bin/env python3
"""
Use OpenAI API to intelligently refine friendly names in ElsterTable.
The AI reviews each signal name and suggests improvements only when needed.
"""

import os
import re
import time
from openai import OpenAI

# Initialize OpenAI client
client = OpenAI(api_key=os.environ.get("OPENAI_API_KEY"))
SYSTEM_PROMPT = """You are a German-English technical translator specializing in heat pump terminology.

Your task: Review signal names and their friendly names. Suggest improvements ONLY when the friendly name has clear issues.

CRITICAL RULES - STRICTLY FOLLOW:
1. Signal names are in ALL CAPS and contain ABBREVIATIONS
2. Friendly names should EXPAND abbreviations to FULL WORDS (never shorten them!)
3. NEVER abbreviate full words: "Temperatur" is CORRECT, "Temp" is WRONG
4. NEVER add words not present in the signal name
5. Fix these specific issues ONLY:
   - Incorrect splits: "Betrieb Sstunden" → "Betriebsstunden"
   - Fragments: "Minimum Imalbegrenzung" → "Minimalbegrenzung"
   - Missing umlauts: "Staendige" → "Ständige"
   - Wrong casing: "Ww" → "WW", "Io" → "IO", "Can" → "CAN"
   - Roman numerals: "Ii" → "II", "Iii" → "III"
6. Keep compound words as ONE word when they are one word in signal name
7. Use spaces to separate meaningful parts only
8. If friendly name is already good (properly expanded), respond with just "OK"

EXAMPLES OF CORRECT EXPANSIONS:
- TEMP in signal → "Temperatur" in friendly (NOT "Temp")
- SOLL in signal → "Soll" in friendly (already expanded)
- IST in signal → "Ist" in friendly (already expanded)
- WW in signal → "Warmwasser" in friendly (expanded)
- KESSELSOLLTEMP → "Kessel Soll Temperatur" (all parts expanded)

Common abbreviations (expand these when found in signal names):
- AUFNAHMELEISTUNG = Aufnahmeleistung
- LUEFTUNGSSTUFE = Lüftungsstufe
- LEISTUNGSZWANG = Leistungszwang
- FEHLERMELDUNG = Fehlermeldung
- VOLUMENSTROM = Volumenstrom
- QUELLENPUMPE = Quellenpumpe
- STUETZSTELLE = Stützstelle
- HILFSKESSEL = Hilfskessel
- BETRIEBSART = Betriebsart
- VERDAMPFER = Verdampfer
- VERDICHTER = Verdichter
- DURCHFLUSS = Durchfluss
- TEMPERATUR = Temperatur
- TEMPORALE = Temporale
- RUECKLAUF = Rücklauf
- LAUFZEIT = Laufzeit
- EINSTELL = Einstellung
- LEISTUNG = Leistung
- KUEHLUNG = Kühlung
- BIVALENT = Bivalent
- PARALLEL = Parallel
- FREQUENZ = Frequenz
- DREHZAHL = Drehzahl
- SPEICHER = Speicher
- SPANNUNG = Spannung
- VORLAUF = Vorlauf
- SAMMLER = Sammler
- BETRIEB = Betrieb
- HEIZUNG = Heizung
- ERTRAG = Ertrag
- AUSSEN = Außen
- MINUTE = Minute
- SOCKEL = Sockel
- KESSEL = Kessel
- DAUER = Dauer
- DRUCK = Druck
- STROM = Strom
- LUEFT = Lüftung
- PUMPE = Pumpe
- VERD = Verdichter
- TEMP = Temperatur
- HEIZ = Heizung
- RAUM = Raum
- SOLL = Soll
- MAX = Maximum
- MIN = Minimum
- SUM = Summe
- TAG = Tag
- IST = Ist
- FKT = Funktion
- HZG = Heizung
- WW = Warmwasser
- WP = Wärmepumpe
- EL = Elektrisch
- LZ = Laufzeit

OUTPUT FORMAT - CRITICAL:
- If friendly name is correct: Respond with exactly "OK"
- If needs improvement: Output ONLY the corrected friendly name
- DO NOT include the signal name in your response
- DO NOT use arrows (→) or any formatting
- DO NOT repeat the signal name before the friendly name
- ONLY output the friendly name text itself

WRONG outputs:
- "SOLAR_GESAMTERTRAG_WH → Solar Gesamtertrag Wh" ❌ (contains arrow and signal name)
- "Signal: Solar Gesamtertrag Wh" ❌ (contains prefix)
- "Corrected: Solar Gesamtertrag Wh" ❌ (contains prefix)

CORRECT outputs:
- "OK" ✓ (if already good)
- "Solar Gesamtertrag Wh" ✓ (just the friendly name)
- "Kessel Soll Temperatur" ✓ (just the friendly name)
"""

def parse_entry(line):
    """Parse an ElsterTable entry and extract fields."""
    stripped = line.strip()
    if not (stripped.startswith('{') and stripped.endswith('},')):
        return None
    
    indent = line[:len(line) - len(line.lstrip())]
    content = stripped[1:-2].strip()
    
    # Split by comma, respecting quotes
    fields = []
    current = []
    in_quote = False
    escape = False
    
    for char in content:
        if escape:
            current.append(char)
            escape = False
        elif char == '\\':
            current.append(char)
            escape = True
        elif char == '"':
            in_quote = not in_quote
            current.append(char)
        elif char == ',' and not in_quote:
            fields.append(''.join(current).strip())
            current = []
        else:
            current.append(char)
    
    if current:
        fields.append(''.join(current).strip())
    
    # Extract signal name (field 0, remove quotes)
    if len(fields) > 0:
        signal_name = fields[0].strip('"')
    else:
        return None
    
    return indent, signal_name, fields

def ask_ai_for_batch_refinement(batch_items):
    """
    Ask OpenAI to review and improve a batch of friendly names.
    
    Args:
        batch_items: List of (signal_name, current_friendly) tuples
        
    Returns:
        List of (improved_name, changed) tuples in same order
    """
    if not batch_items:
        return []
    
    # Build batch prompt
    batch_text = "Review these signal names and their friendly names.\n\n"
    batch_text += "For each entry, respond on a NEW LINE with:\n"
    batch_text += "- The number followed by period and space\n"
    batch_text += "- Then either 'OK' if good, or the corrected friendly name\n"
    batch_text += "- Each response must be on its own line\n\n"
    batch_text += "Example format:\n"
    batch_text += "1. OK\n"
    batch_text += "2. Kessel Soll Temperatur\n"
    batch_text += "3. OK\n\n"
    batch_text += "Now review these:\n\n"
    
    for i, (signal, friendly) in enumerate(batch_items, 1):
        batch_text += f"{i}. Signal: {signal}, Current: {friendly}\n"
    
    try:
        response = client.chat.completions.create(
            model="gpt-4o-mini",
            messages=[
                {"role": "system", "content": SYSTEM_PROMPT},
                {"role": "user", "content": batch_text}
            ],
            temperature=0.3,
            max_tokens=2000
        )
        
        suggestion_text = response.choices[0].message.content.strip()
        
        # Parse the batch response - expecting numbered lines like "1. OK" or "2. Improved Name"
        lines = suggestion_text.split('\n')
        results = []
        
        for line in lines:
            line = line.strip()
            if not line:
                continue
            
            # Check if this is a numbered response (e.g., "1. OK" or "2. Kessel Soll Temperatur")
            if line and line[0].isdigit() and '. ' in line:
                # Extract everything after "N. "
                parts = line.split('. ', 1)
                if len(parts) > 1:
                    response_text = parts[1].strip()
                    # Remove any remaining "Response:" prefix
                    if response_text.lower().startswith('response:'):
                        response_text = response_text[9:].strip()
                    results.append(response_text)
        
        # Verify we got the right number of responses
        if len(results) != len(batch_items):
            print(f"  WARNING: Expected {len(batch_items)} responses, got {len(results)}")
            print(f"  Raw AI response: {suggestion_text[:200]}...")

        
        # Convert to (improved_name, changed) tuples
        final_results = []
        for i, (signal, friendly) in enumerate(batch_items):
            if i < len(results):
                suggestion = results[i]
                if suggestion.upper() == "OK":
                    final_results.append((friendly, False))
                else:
                    final_results.append((suggestion, True))
            else:
                # Fallback if parsing failed
                final_results.append((friendly, False))
        
        return final_results
        
    except Exception as e:
        print(f"  ERROR in batch processing: {e}")
        # Return unchanged for all items
        return [(friendly, False) for _, friendly in batch_items]

def ask_ai_for_refinement(signal_name, current_friendly):
    """
    Ask OpenAI to review and potentially improve the friendly name.
    Returns: (improved_name, changed) or (current_friendly, False) if no change
    
    Note: This is kept for compatibility but batching is preferred.
    """
    batch_result = ask_ai_for_batch_refinement([(signal_name, current_friendly)])
    return batch_result[0] if batch_result else (current_friendly, False)

def ai_refine_elster_table(input_file, output_file, max_entries=None, start_from=0):
    """
    Use AI to refine friendly names in ElsterTable.
    
    Args:
        max_entries: Maximum number of entries to process (None = all)
        start_from: Start processing from this entry number (for resuming)
    """
    print(f"Reading {input_file}...")
    print(f"Using OpenAI model: gpt-4o-mini")
    print(f"Batch size: 50 entries per API call")
    print()
    
    with open(input_file, 'r', encoding='utf-8') as f:
        lines = f.readlines()
    
    stats = {
        'total': 0,
        'processed': 0,
        'improved': 0,
        'unchanged': 0,
        'skipped': 0,
    }
    
    output_lines = []
    in_table = False
    examples = []
    entry_count = 0
    
    # Batch processing variables
    BATCH_SIZE = 50
    current_batch = []  # List of (line_index, indent, signal_name, fields, current_friendly)
    pending_lines = []  # Lines waiting to be processed
    
    for line_idx, line in enumerate(lines):
        if 'ElsterTable[]' in line:
            in_table = True
            output_lines.append(line)
            continue
        
        if in_table and line.strip() == '};':
            in_table = False
            
            # Process any remaining batch before closing
            if current_batch:
                print(f"\n[Batch] Processing final batch of {len(current_batch)} entries...")
                batch_items = [(item[2], item[4]) for item in current_batch]
                batch_results = ask_ai_for_batch_refinement(batch_items)
                
                # Apply batch results
                for (line_idx, indent, signal_name, fields, current_friendly), (improved_friendly, changed) in zip(current_batch, batch_results):
                    stats['processed'] += 1
                    entry_count = pending_lines[line_idx - len(output_lines)][0]
                    
                    if changed:
                        stats['improved'] += 1
                        fields[3] = f'"{improved_friendly}"'
                        if len(examples) < 30:
                            examples.append((signal_name, current_friendly, improved_friendly))
                        print(f"  [{entry_count}] {signal_name}: {current_friendly} → {improved_friendly}")
                    else:
                        stats['unchanged'] += 1
                    
                    new_line = indent + '{ ' + ', '.join(fields) + ' },\n'
                    output_lines.append(new_line)
                
                current_batch = []
            
            output_lines.append(line)
            continue
        
        if in_table and line.strip().startswith('{'):
            result = parse_entry(line)
            
            if result:
                indent, signal_name, fields = result
                stats['total'] += 1
                entry_count += 1
                
                # Skip entries before start_from
                if entry_count <= start_from:
                    stats['skipped'] += 1
                    output_lines.append(line)
                    continue
                
                # Stop if we've processed max_entries
                if max_entries and stats['processed'] >= max_entries:
                    stats['skipped'] += 1
                    output_lines.append(line)
                    continue
                
                # Field 3 is the friendly name
                if len(fields) > 3:
                    current_friendly = fields[3].strip('"')
                    
                    if current_friendly and current_friendly != 'NULL':
                        # Add to current batch
                        current_batch.append((line_idx, indent, signal_name, fields, current_friendly))
                        pending_lines.append((entry_count, signal_name))
                        
                        # Process batch when it reaches BATCH_SIZE
                        if len(current_batch) >= BATCH_SIZE:
                            print(f"\n[Batch] Processing entries {entry_count - BATCH_SIZE + 1} to {entry_count}...")
                            batch_items = [(item[2], item[4]) for item in current_batch]
                            batch_results = ask_ai_for_batch_refinement(batch_items)
                            
                            # Apply batch results
                            for (_, indent, signal_name, fields, current_friendly), (improved_friendly, changed) in zip(current_batch, batch_results):
                                stats['processed'] += 1
                                
                                if changed:
                                    stats['improved'] += 1
                                    fields[3] = f'"{improved_friendly}"'
                                    if len(examples) < 30:
                                        examples.append((signal_name, current_friendly, improved_friendly))
                                    print(f"  ✓ {signal_name}: {current_friendly} → {improved_friendly}")
                                else:
                                    stats['unchanged'] += 1
                                
                                new_line = indent + '{ ' + ', '.join(fields) + ' },\n'
                                output_lines.append(new_line)
                            
                            current_batch = []
                            pending_lines = []
                            time.sleep(1)  # Rate limiting between batches
                    else:
                        stats['skipped'] += 1
                        output_lines.append(line)
                else:
                    output_lines.append(line)
            else:
                output_lines.append(line)
        else:
            output_lines.append(line)
    
    # Write output
    print(f"\nWriting {output_file}...")
    with open(output_file, 'w', encoding='utf-8') as f:
        f.writelines(output_lines)
    
    # Show examples
    if examples:
        print(f"\nExamples of AI improvements:")
        for signal, old, new in examples[:20]:
            print(f"  {signal:35} | {old:35} → {new}")
    
    print(f"\nStatistics:")
    print(f"  Total entries in file: {stats['total']}")
    print(f"  Processed by AI: {stats['processed']}")
    print(f"  Improved: {stats['improved']}")
    print(f"  Unchanged (already good): {stats['unchanged']}")
    print(f"  Skipped (NULL or outside range): {stats['skipped']}")
    print(f"\nOutput written to {output_file}")

if __name__ == "__main__":
    import sys
    
    # Check for API key
    if not os.environ.get("OPENAI_API_KEY"):
        print("ERROR: OPENAI_API_KEY environment variable not set")
        print("Set it with: export OPENAI_API_KEY='your-api-key'")
        sys.exit(1)
    
    # Parse arguments
    # Detect if we're running from esphome/ directory or repo root
    script_dir = os.path.dirname(os.path.abspath(__file__))
    if os.path.basename(script_dir) == 'esphome':
        # Running from esphome directory
        default_input = "ha-stiebel-control/elster/ElsterTable.h"
    else:
        # Running from repo root
        default_input = "esphome/ha-stiebel-control/elster/ElsterTable.h"
    
    input_file = default_input
    max_entries = None
    start_from = 0
    
    if len(sys.argv) > 1:
        input_file = sys.argv[1]
    if len(sys.argv) > 2:
        max_entries = int(sys.argv[2])
    if len(sys.argv) > 3:
        start_from = int(sys.argv[3])
    
    output_file = input_file.replace('.h', '_ai_refined.h')
    
    print("="*70)
    print("AI-Powered Friendly Name Refinement")
    print("="*70)
    if max_entries:
        print(f"Processing: {max_entries} entries starting from entry #{start_from+1}")
    else:
        print(f"Processing: ALL entries starting from entry #{start_from+1}")
    print("="*70)
    print()
    
    ai_refine_elster_table(input_file, output_file, max_entries, start_from)
    
    print(f"\nTo apply changes:")
    print(f"  mv {input_file} {input_file}.before_ai_refine")
    print(f"  mv {output_file} {input_file}")
    print()
    print("To process in batches (recommended for cost control):")
    print(f"  python3 {sys.argv[0]} {input_file} 50    # Process 50 entries")
    print(f"  python3 {sys.argv[0]} {input_file} 50 50 # Process next 50 (51-100)")
