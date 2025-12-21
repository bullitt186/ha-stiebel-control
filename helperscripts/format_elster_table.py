#!/usr/bin/env python3
"""
Format ElsterTable.h to align columns for better readability.
"""

import re

def parse_entry_fields(line):
    """
    Parse an ElsterTable entry line and extract all fields.
    Returns a list of field strings or None if not an entry line.
    """
    # Match the pattern: { "NAME", 0xINDEX, TYPE, ...fields... },
    stripped = line.strip()
    if not (stripped.startswith('{') and stripped.endswith('},')):
        return None
    
    # Get indentation
    indent = line[:len(line) - len(line.lstrip())]
    
    # Remove { and },
    content = stripped[1:-2].strip()
    
    # Split by comma, but respect quoted strings
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
    
    # Add last field
    if current:
        fields.append(''.join(current).strip())
    
    return indent, fields


def format_elster_table(input_file, output_file):
    """
    Format the ElsterTable.h file with aligned columns.
    """
    print(f"Reading {input_file}...")
    
    with open(input_file, 'r', encoding='utf-8') as f:
        lines = f.readlines()
    
    # First pass: collect all entries and find max widths
    entries = []
    max_widths = [0] * 13  # 13 fields per entry
    in_table = False
    
    for i, line in enumerate(lines):
        if 'ElsterTable[]' in line:
            in_table = True
            entries.append((i, None))
            continue
        
        if in_table and line.strip() == '};':
            in_table = False
            entries.append((i, None))
            continue
        
        if in_table and line.strip().startswith('{'):
            result = parse_entry_fields(line)
            if result:
                indent, fields = result
                entries.append((i, (indent, fields)))
                
                # Update max widths for each field
                for j, field in enumerate(fields):
                    if j < len(max_widths):
                        max_widths[j] = max(max_widths[j], len(field))
            else:
                entries.append((i, None))
        elif in_table:
            entries.append((i, None))
    
    print(f"Found {sum(1 for _, e in entries if e is not None)} entries")
    print(f"Max widths: {max_widths}")
    
    # Second pass: format entries
    output_lines = lines.copy()
    
    for line_idx, entry_data in entries:
        if entry_data is None:
            continue
        
        indent, fields = entry_data
        
        # Format each field with appropriate padding
        formatted_fields = []
        for j, field in enumerate(fields):
            if j < len(max_widths):
                # Right-align NULL and boolean values, left-align everything else
                if field in ('NULL', 'true', 'false'):
                    formatted_fields.append(field.rjust(max_widths[j]))
                else:
                    formatted_fields.append(field.ljust(max_widths[j]))
            else:
                formatted_fields.append(field)
        
        # Reconstruct the line with proper spacing
        formatted_line = indent + '{ ' + ', '.join(formatted_fields) + ' },\n'
        output_lines[line_idx] = formatted_line
    
    # Write output
    print(f"Writing {output_file}...")
    with open(output_file, 'w', encoding='utf-8') as f:
        f.writelines(output_lines)
    
    print("Done!")


if __name__ == "__main__":
    import sys
    
    if len(sys.argv) > 1:
        input_file = sys.argv[1]
    else:
        input_file = "esphome/ha-stiebel-control/elster/ElsterTable.h"
    
    output_file = input_file.replace('.h', '_formatted.h')
    
    format_elster_table(input_file, output_file)
    
    print(f"\nTo apply changes:")
    print(f"  mv {input_file} {input_file}.before_format")
    print(f"  mv {output_file} {input_file}")
