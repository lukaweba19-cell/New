#!/usr/bin/env python3
import re, sys, os

def find_matching_brace(lines, start_idx):
    depth = 0
    found_open = False
    for i in range(start_idx, len(lines)):
        for ch in lines[i]:
            if ch == '{':
                depth += 1
                found_open = True
            elif ch == '}':
                depth -= 1
                if found_open and depth == 0:
                    return i
    return None

def fix_file(fpath):
    with open(fpath) as f:
        lines = f.readlines()
    
    let_lines = []
    for i, line in enumerate(lines):
        s = line.strip()
        if '&& let ' in s and not s.startswith('//'):
            let_lines.append(i)
    
    if not let_lines:
        return False
    
    let_lines.sort(reverse=True)
    
    for let_idx in let_lines:
        line = lines[let_idx]
        s = line.strip()
        
        m = re.search(r'&&\s*let\s+(.+?)\s*=\s*(.+?)\s*(?:,|$)', s)
        if not m:
            continue
        pat = m.group(1).strip()
        expr = m.group(2).strip()
        
        # Find enclosing if/while
        kw_idx = None
        kw_type = None
        for j in range(let_idx, -1, -1):
            js = lines[j].strip()
            if js.startswith('if ') or js.startswith('while '):
                kw_idx = j
                kw_type = 'if' if js.startswith('if ') else 'while'
                break
            if lines[j].strip() == '}':
                break
        
        if kw_idx is None:
            continue
        
        # Body start
        body_start = None
        for j in range(let_idx + 1, min(let_idx + 15, len(lines))):
            if '{' in lines[j]:
                body_start = j
                break
        if body_start is None:
            continue
        
        body_end = find_matching_brace(lines, body_start)
        if body_end is None:
            continue
        
        pre_lines = lines[kw_idx + 1:let_idx]
        post_lines = lines[let_idx + 1:body_start]
        body_lines = lines[body_start:body_end + 1]
        
        kw_rest = lines[kw_idx].strip()
        kw_rest = kw_rest[3:] if kw_type == 'if' else kw_rest[6:]
        
        pre = ' '.join([kw_rest] + [l.strip().rstrip(',') for l in pre_lines])
        pre = re.sub(r'\s*&&\s*$', '', pre).strip()
        
        post = ' '.join([l.strip().rstrip(',') for l in post_lines])
        post = re.sub(r'^&&\s*', '', post).strip()
        
        indent = len(lines[kw_idx]) - len(lines[kw_idx].lstrip())
        inner = indent + 4
        
        combined = pre + (' && ' + post if post else '')
        
        new_lines = []
        new_lines.append(' ' * indent + f'if let {pat} = {expr} {{\n')
        new_lines.append(' ' * inner + f'if {combined} {{\n')
        for bl in body_lines:
            new_lines.append(' ' * (inner + 4) + bl)
        new_lines.append(' ' * inner + '}\n')
        new_lines.append(' ' * indent + '}\n')
        
        lines[kw_idx:body_end + 1] = new_lines
    
    with open(fpath, 'w') as f:
        f.writelines(lines)
    print(f"Fixed {fpath}: {len(let_lines)} converted")
    return True

for f in sys.argv[1:]:
    if os.path.exists(f):
        fix_file(f)
