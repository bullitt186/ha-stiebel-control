#!/usr/bin/env python3
"""
Refine friendly names in ElsterTable by fixing expansion oddities.
Rules:
1. Don't add new words not in signal name
2. Fix double letters from bad splits (e.g., "Betrieb Sstunden" → "Betriebsstunden")
3. Preserve roman numerals as uppercase (I, II, III, IV)
4. Fix known abbreviations (WW, CAN, ID, etc.)
5. Fix compound words that were split incorrectly
"""

import re

# Known abbreviations that should stay uppercase
UPPERCASE_ABBREVS = {
    'Ww': 'WW',
    'Id': 'ID', 
    'Can': 'CAN',
    'Pc': 'PC',
    'Usb': 'USB',
    'Io': 'IO',
    'Ii': 'II',
    'Iii': 'III',
    'Iv': 'IV',
    'Vi': 'VI',
    'Vii': 'VII',
    'Viii': 'VIII',
    'Ix': 'IX',
    'Xram': 'XRAM',
    'Iram': 'IRAM',
}

def fix_double_letters(friendly_name, signal_name):
    """
    Fix cases where splitting created double letters.
    E.g., "Betrieb Sstunden" → "Betriebsstunden"
    """
    words = friendly_name.split()
    fixed_words = []
    
    i = 0
    while i < len(words):
        word = words[i]
        
        # Check if this word starts with a doubled letter from previous word
        if i > 0 and len(word) > 1:
            prev_word = fixed_words[-1]
            # If previous word ends with same letter that this word starts with
            if prev_word and word[0].lower() == prev_word[-1].lower():
                # Check if in signal name these are together
                search_str = (prev_word + word[1:]).upper()
                if search_str in signal_name.upper():
                    # Merge them
                    fixed_words[-1] = prev_word + word[1:].lower()
                    i += 1
                    continue
        
        fixed_words.append(word)
        i += 1
    
    return ' '.join(fixed_words)

def fix_split_compounds(friendly_name, signal_name):
    """
    Fix compound words that were incorrectly split due to bad abbreviation matching.
    Only merge when there's clear evidence of incorrect split (lowercase start, fragment pattern).
    E.g., "Minimum Imalbegrenzung" → "Minimalbegrenzung"
    E.g., "Betrieb Sstunden" → "Betriebsstunden"
    """
    words = friendly_name.split()
    fixed_words = []
    
    i = 0
    while i < len(words):
        word = words[i]
        
        # Only merge if next word looks like a fragment (lowercase start or double letter)
        if i < len(words) - 1:
            next_word = words[i + 1]
            
            # Case 1: Next word starts with lowercase (clear fragment)
            if len(next_word) > 1 and next_word[0].islower():
                # Check if combining them creates a word in signal name
                combined = (word + next_word).upper()
                if combined in signal_name.upper():
                    fixed_words.append(word + next_word.lower())
                    i += 2
                    continue
            
            # Case 2: Next word starts with doubled letter from end of current word
            # E.g., "Betrieb" + "Sstunden" where word ends with 's' and next starts with 'S'
            if len(next_word) > 1 and len(word) > 0:
                if next_word[0].upper() == word[-1].upper() and next_word[0].isupper():
                    # This looks like a bad split, merge them
                    combined = (word + next_word[1:]).upper()
                    if combined in signal_name.upper():
                        fixed_words.append(word + next_word[1:].lower())
                        i += 2
                        continue
        
        fixed_words.append(word)
        i += 1
    
    return ' '.join(fixed_words)

def fix_abbreviations(friendly_name):
    """
    Fix known abbreviations to proper casing.
    """
    words = friendly_name.split()
    fixed_words = []
    
    for word in words:
        # Check if this word should be uppercase
        if word in UPPERCASE_ABBREVS:
            fixed_words.append(UPPERCASE_ABBREVS[word])
        else:
            fixed_words.append(word)
    
    return ' '.join(fixed_words)

def fix_roman_numerals(friendly_name):
    """
    Ensure roman numerals are uppercase.
    """
    # Pattern for roman numerals at word boundaries
    pattern = r'\b(i|ii|iii|iv|v|vi|vii|viii|ix|x)\b'
    
    def replace_roman(match):
        return match.group(0).upper()
    
    return re.sub(pattern, replace_roman, friendly_name, flags=re.IGNORECASE)

def fix_german_characters(friendly_name, signal_name):
    """
    Fix German characters that should use umlauts.
    E.g., "Staendige" → "Ständige" if "STAENDIGE" in signal name
    """
    replacements = [
        ('Staendige', 'Ständige', 'STAENDIGE'),
        ('Staendig', 'Ständig', 'STAENDIG'),
    ]
    
    result = friendly_name
    for old, new, signal_check in replacements:
        if signal_check in signal_name.upper() and old in result:
            result = result.replace(old, new)
    
    return result

def refine_friendly_name(signal_name, current_friendly):
    """
    Refine a friendly name by applying various fixes.
    """
    if not current_friendly or current_friendly == 'NULL':
        return current_friendly
    
    refined = current_friendly
    
    # Apply fixes in order
    refined = fix_double_letters(refined, signal_name)
    refined = fix_split_compounds(refined, signal_name)
    refined = fix_abbreviations(refined)
    refined = fix_roman_numerals(refined)
    refined = fix_german_characters(refined, signal_name)
    
    return refined

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

def refine_elster_table(input_file, output_file):
    """Refine all friendly names in ElsterTable."""
    print(f"Reading {input_file}...")
    
    with open(input_file, 'r', encoding='utf-8') as f:
        lines = f.readlines()
    
    stats = {
        'total': 0,
        'refined': 0,
        'unchanged': 0,
    }
    
    output_lines = []
    in_table = False
    examples = []
    
    for line in lines:
        if 'ElsterTable[]' in line:
            in_table = True
            output_lines.append(line)
            continue
        
        if in_table and line.strip() == '};':
            in_table = False
            output_lines.append(line)
            continue
        
        if in_table and line.strip().startswith('{'):
            result = parse_entry(line)
            
            if result:
                indent, signal_name, fields = result
                stats['total'] += 1
                
                # Field 3 is the friendly name
                if len(fields) > 3:
                    current_friendly = fields[3].strip('"')
                    
                    if current_friendly and current_friendly != 'NULL':
                        refined_friendly = refine_friendly_name(signal_name, current_friendly)
                        
                        if refined_friendly != current_friendly:
                            stats['refined'] += 1
                            fields[3] = f'"{refined_friendly}"'
                            
                            # Collect examples
                            if len(examples) < 20:
                                examples.append((signal_name, current_friendly, refined_friendly))
                        else:
                            stats['unchanged'] += 1
                        
                        # Rebuild line
                        new_line = indent + '{ ' + ', '.join(fields) + ' },\n'
                        output_lines.append(new_line)
                    else:
                        stats['unchanged'] += 1
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
        print(f"\nExamples of refinements:")
        for signal, old, new in examples:
            print(f"  {signal:35} | {old:40} → {new}")
    
    print(f"\nStatistics:")
    print(f"  Total entries: {stats['total']}")
    print(f"  Refined: {stats['refined']}")
    print(f"  Unchanged: {stats['unchanged']}")
    print(f"\nOutput written to {output_file}")

if __name__ == "__main__":
    import sys
    
    if len(sys.argv) > 1:
        input_file = sys.argv[1]
    else:
        input_file = "esphome/ha-stiebel-control/elster/ElsterTable.h"
    
    output_file = input_file.replace('.h', '_refined.h')
    
    refine_elster_table(input_file, output_file)
    
    print(f"\nTo apply changes:")
    print(f"  mv {input_file} {input_file}.before_refine")
    print(f"  mv {output_file} {input_file}")
