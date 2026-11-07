#!/usr/bin/env python3
"""
Fix all let_chains (&& let) in Rust 2021 edition for cfg crate.
Converts: cond1 && let PAT = EXPR && cond2
To: if let PAT = EXPR { cond1 && cond2 } else { false }
But since conditions span multiple lines, we restructure the entire if/while.
"""

import re, sys, os

def find_if_while_range(lines, start_idx):
    """Find the start of the if/while and the end of its body."""
    # Look back for if/while keyword
    kw_idx = None
    for j in range(start_idx, -1, -1):
        stripped = lines[j].strip()
        if stripped.startswith('if ') or stripped.startswith('while '):
            kw_idx = j
            break
        # Stop if we hit a closing brace that's not part of this chain
        if stripped == '}' or stripped.endswith('};'):
            break
    
    if kw_idx is None:
        return None, None
    
    # Find the body start (opening brace)
    body_start = None
    for j in range(start_idx, min(start_idx + 10, len(lines))):
        if '{' in lines[j]:
            body_start = j
            break
    
    if body_start is None:
        return kw_idx, None
    
    # Find matching closing brace
    depth = 0
    body_end = None
    for j in range(body_start, len(lines)):
        for ch in lines[j]:
            if ch == '{':
                depth += 1
            elif ch == '}':
                depth -= 1
                if depth == 0:
                    body_end = j
                    break
        if body_end is not None:
            break
    
    return kw_idx, body_end

def extract_let_pattern(line):
    """Extract PAT = EXPR from '&& let PAT = EXPR' or '&& let PAT = EXPR,'"""
    m = re.search(r'&&\s*let\s+(.+?)\s*=\s*(.+?)\s*(?:,|$)', line.strip())
    if m:
        return m.group(1).strip(), m.group(2).strip()
    return None, None

def fix_file(fpath):
    with open(fpath) as f:
        content = f.read()
    
    original_lines = content.split('\n')
    lines = list(original_lines)  # mutable copy
    i = 0
    changed = False
    
    while i < len(lines):
        line = lines[i]
        stripped = line.strip()
        
        if '&& let ' in stripped and not stripped.startswith('//'):
            pat, expr = extract_let_pattern(line)
            if pat is None:
                i += 1
                continue
            
            kw_idx, body_end = find_if_while_range(lines, i)
            if kw_idx is None or body_end is None:
                i += 1
                continue
            
            kw_line = lines[kw_idx].strip()
            kw = 'if' if kw_line.startswith('if ') else 'while'
            
            # Collect all lines from kw_idx to body_end
            block_lines = lines[kw_idx:body_end + 1]
            
            # Find the let line within the block
            let_rel_idx = None
            for j, bl in enumerate(block_lines):
                if '&& let ' in bl.strip() and not bl.strip().startswith('//'):
                    let_rel_idx = j
                    # Also check if this is the same let pattern
                    p, e = extract_let_pattern(bl)
                    if p == pat and e == expr:
                        break
                    let_rel_idx = None
            
            if let_rel_idx is None:
                i += 1
                continue
            
            # Split: before let, the let line, after let (to closing brace)
            before_let = block_lines[:let_rel_idx]
            let_line = block_lines[let_rel_idx]
            after_let = block_lines[let_rel_idx + 1:]
            
            # Extract conditions from before_let (after the if/while keyword)
            kw_and_cond = before_let[0].strip()
            # Remove 'if ' or 'while ' prefix
            if kw_and_cond.startswith('if '):
                cond_prefix = kw_and_cond[3:]
            elif kw_and_cond.startswith('while '):
                cond_prefix = kw_and_cond[6:]
            else:
                cond_prefix = kw_and_cond
            
            # cond_prefix may be split across multiple lines
            # Join all before_let lines except the first (which has the keyword)
            # and remove trailing && from the last line
            full_pre = []
            for bl in before_let[1:]:  # skip the keyword line
                full_pre.append(bl.strip().rstrip(','))
            
            # The condition before the let (may span multiple lines)
            pre_cond = cond_prefix
            for pl in full_pre:
                pre_cond += ' ' + pl
            
            # Remove trailing && from pre_cond
            pre_cond = re.sub(r'\s*&&\s*$', '', pre_cond)
            
            # The condition after the let (everything from let_rel_idx+1 to body_start)
            # Find body start within after_let (the line with {)
            body_idx_in_after = None
            for j, al in enumerate(after_let):
                if '{' in al:
                    body_idx_in_after = j
                    break
            
            if body_idx_in_after is None:
                i += 1
                continue
            
            post_lines = after_let[:body_idx_in_after]
            body_lines = after_let[body_idx_in_after:]
            
            # Join post condition
            post_cond = ''
            for pl in post_lines:
                post_cond += ' ' + pl.strip().rstrip(',')
            post_cond = post_cond.strip()
            # Remove leading &&
            post_cond = re.sub(r'^&&\s*', '', post_cond)
            
            # Build the replacement
            indent = len(lines[kw_idx]) - len(lines[kw_idx].lstrip())
            inner_indent = indent + 4
            
            new_block = []
            # let binding
            new_block.append(' ' * indent + f'let {pat} = {expr};')
            # if/while let wrapping
            new_block.append(' ' * indent + f'{kw} {pat} == {expr} {{'.replace(f'{pat} == {expr}', f'Pat &{pat}'))  # placeholder
            # Actually: if let PAT = EXPR { if COND { BODY } }
            # But PAT binds variables, so we need the actual pattern match
            
            # Better: use the actual pattern
            new_block = []
            new_block.append(' ' * indent + f'let {pat} = {expr};')
            # Now wrap: if true { pre_cond && post_cond } but we need the body
            # Actually restructure as:
            # if PRE_COND { if let PAT = EXPR { POST_COND BODY } }
            # Hmm, but pre_cond references things defined before the if.
            
            # Simplest correct approach: hoist the let, then use it in condition
            # But Rust 2021 doesn't allow let in if condition.
            # So: let v = match EXPR { PAT => true, _ => false };
            # Then the let-pattern variables are lost.
            
            # For variables that are used, we need to extract them:
            # let (var1, var2) = match EXPR { PAT => (var1, var2), _ => return };
            
            # This is getting very complex. Let me use a different strategy:
            # Replace the entire if/while block with a manually restructured version
            
            # Mark for manual fix and skip
            print(f"  NEEDS MANUAL FIX at line {i+1} in {fpath}: {stripped[:80]}")
            i += 1
            continue
        
        i += 1
    
    print(f"  Analyzed {fpath}: {sum(1 for l in original_lines if '&& let ' in l.strip() and not l.strip().startswith('//'))} let_chain occurrences")
    return changed

if __name__ == '__main__':
    for fpath in sys.argv[1:]:
        if os.path.exists(fpath):
            fix_file(fpath)
