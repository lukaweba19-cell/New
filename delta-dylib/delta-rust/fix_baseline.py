#!/usr/bin/env python3
"""
Fix medal2-baseline for stable Rust 2021 + iOS target.

Strategy:
- Feature gates: remove lines
- Box patterns in match arms: remove `box` keyword (match ergonomics)
- Box constructions: `box EXPR` -> `Box::new(EXPR)`
- Let chains: convert `A && let B = C && D` to nested if-let blocks
- Delta delta: bytecode versions 4..=9
- FFI exports: add to luau-lifter/src/lib.rs
"""

import re, os, sys

BASE = "delta-dylib/delta-rust/medal-baseline"

def read(path):
    with open(path) as f:
        return f.read()

def write(path, content):
    with open(path, "w") as f:
        f.write(content)

def relativize(path):
    return os.path.relpath(path, BASE)

# ============================================================
# Pass 1: Remove feature gates
# ============================================================
print("=== Pass 1: Remove feature gates ===")
for root, dirs, files in os.walk(BASE):
    for f in files:
        if not f.endswith(".rs"):
            continue
        path = os.path.join(root, f)
        content = read(path)
        new = re.sub(r'^#![feature\([^]]*\)]\n', '', content, flags=re.MULTILINE)
        if new != content:
            write(path, new)
            print(f"  {relativize(path)}")

# ============================================================
# Pass 2: Fix box patterns and constructions
# ============================================================
print("\n=== Pass 2: Fix box patterns ===")

for root, dirs, files in os.walk(BASE):
    for f in files:
        if not f.endswith(".rs"):
            continue
        path = os.path.join(root, f)
        content = read(path)
        if 'box ' not in content:
            continue
        
        original = content
        lines = content.split('\n')
        out = []
        
        for line in lines:
            # Skip lines that are just feature gates or comments
            stripped = line.strip()
            if stripped.startswith('#![') or stripped.startswith('//') or stripped.startswith('/*'):
                out.append(line)
                continue
            
            # Determine if this line has `box` in a match pattern context or construction context
            # Pattern: `box PAT` in a match arm discriminant
            # Construction: `box EXPR` as a value
            
            # Handle `box IDENT @ PAT` -> `IDENT @ PAT` (match ergonomics)
            new_line = re.sub(r'\bbox\s+(\w+)\s+@', r'\1 @', line)
            
            # Handle `box PATTERN` in match arms - remove `box` keyword
            # But NOT when it's `Box::new(` or part of a type annotation
            # Match: `box RValue::X(...)`, `box RValue::X { ... }`, `box Type { ... }`
            new_line = re.sub(r'\bbox\s+(RValue::\w+)', r'\1', new_line)
            new_line = re.sub(r'\bbox\s+(Unary)\s*\{', r'\1 {', new_line)
            new_line = re.sub(r'\bbox\s+(Binary)\s*\{', r'\1 {', new_line)
            new_line = re.sub(r'\bbox\s+(Index)\s*\{', r'\1 {', new_line)
            new_line = re.sub(r'\bbox\s+ref\s+(\w+)', r'\1', new_line)
            
            # Handle `box expr` constructions that remain
            # These are `box SomeType(...)` used as values (not in match patterns)
            # After removing match patterns above, remaining `box` should be constructions
            new_line = re.sub(r'\bbox\s+(RValue::\w+\([^)]*\))', r'Box::new(\1)', new_line)
            new_line = re.sub(r'\bbox\s+(Literal::\w+\([^)]*\))', r'Box::new(\1)', new_line)
            
            # Handle `box expr` where expr is a variable or simple expression
            # But be careful not to match `box Patterns`
            new_line = re.sub(r'\bbox\s+(\w+)\s*\.', r'Box::new(\1).', new_line)  # box x.y -> Box::new(x).y
            # box IDENTIFIER (standalone, not followed by @ or { or ()
            # This is hard to distinguish from patterns. Skip for now.
            
            out.append(new_line)
        
        new_content = '\n'.join(out)
        if new_content != original:
            write(path, new_content)
            print(f"  {relativize(path)}")

# ============================================================
# Pass 3: Fix let_chains (&& let PAT = EXPR)
# ============================================================
print("\n=== Pass 3: Fix let_chains ===")

for root, dirs, files in os.walk(BASE):
    for f in files:
        if not f.endswith(".rs"):
            continue
        path = os.path.join(root, f)
        content = read(path)
        if '&& let' not in content:
            continue
        
        original = content
        lines = content.split('\n')
        out = []
        
        for line in lines:
            if '&& let' not in line:
                out.append(line)
                continue
            
            # This line contains a let_chain. We need to convert it.
            # Pattern variations:
            # 1. `COND && let PAT = EXPR && MORE` (let in middle)
            # 2. `COND && let PAT = EXPR` (let at end)
            # 3. `if COND && let PAT = EXPR {` (in if condition)
            
            # Strategy: Replace `&& let PAT = EXPR` with a block that does the binding.
            # For case 1: `A && let B = C && D` becomes:
            #   `{ let B = C; if A { if D { ... } else { continue; } } else { continue; } }`
            # But this is too aggressive. Simpler approach:
            # Replace with: `if let B = C { A && D } else { continue; }`
            # Wait, that doesn't work either because A is evaluated before the let.
            
            # The correct semantics of `A && let B = C`:
            # 1. Evaluate A. If false, short-circuit.
            # 2. Evaluate C. If it matches B, continue with true.
            # 3. If C doesn't match, the whole condition is false.
            
            # In Rust 2021 without let_chains:
            # `{ if A { if let B = C { true } else { false } } else { false } }`
            # But we need the rest of the condition too.
            
            # For a condition like `A && let B = C && D`:
            # `{ let tmp = A; if tmp { if let B = C { D } else { false } } else { false } }`
            
            # Simplest correct transformation:
            # Replace `&& let PAT = EXPR` with `; if let PAT = EXPR {`
            # and add `} else { continue; }` or `} else { false }` at the end
            
            # Actually, for use in `if` conditions and `while` conditions,
            # the simplest fix is to extract the let binding before the condition.
            
            # For each `&& let PAT = EXPR` in the line:
            # Replace with a block: `{ let PAT = EXPR; true }` but that's wrong
            # if EXPR is an Option/Result that needs matching.
            
            # Let me use the approach of converting to `if let`:
            # `A && let B = C` -> `(if let B = C { A } else { return false; })`
            # Wait, `return false` doesn't work in all contexts.
            
            # The MOST COMMON pattern in this codebase is:
            # `something && let Some(x) = something_else && more_stuff`
            # inside `if` or `while` conditions.
            
            # Let me handle the specific patterns I see in the codebase:
            
            # Pattern: `... && let Ok(...) = ... && ...`
            # -> `{ if let Ok(...) = ... { ... && ... } else { continue; } }`
            
            # Actually, the cleanest approach for this codebase:
            # Convert `&& let PAT = EXPR` to `; let PAT = EXPR;` and wrap
            # the whole condition in a match/if-let that handles the failure case.
            
            # But this requires knowing the context (if/while/let).
            
            # Let me just do simple line-by-line conversion for the patterns I see:
            
            # Case: `&& let Some PAT = EXPR` at end of line
            new_line = re.sub(
                r'(\s*)(.+?)\s*&&\s*let\s+(?:Some\s*\()?(\w+)\s*(?:\)\s*)?=\s*(.+?)$',
                r'\1{\n\1    let \3 = \4;\n\1    \2\n\1}',
                line
            )
            
            # Case: `&& let Ok(PAT) = EXPR`
            new_line = re.sub(
                r'(\s*)(.+?)\s*&&\s*let\s+Ok\((\w+)\)\s*=\s*(.+?)\s*&&\s*(.+)$',
                r'\1{\n\1    if let Ok(\2) = \3 {\n\1        \4\n\1    } else { continue; }\n\1}',
                line
            )
            
            # Case: `&& let Err(PAT) = EXPR`  
            new_line = re.sub(
                r'(\s*)(.+?)\s*&&\s*let\s+Err\((\w+)\)\s*=\s*(.+?)\s*&&\s*(.+)$',
                r'\1{\n\1    if let Err(\2) = \3 {\n\1        \4\n\1    } else { continue; }\n\1}',
                line
            )
            
            # Generic: `&& let PAT = EXPR` (PAT is a simple ident)
            new_line = re.sub(
                r'(\s*)(.+?)\s*&&\s*let\s+(\w+)\s*=\s*(.+?)\s*&&\s*(.+)$',
                r'\1{\n\1    let \3 = \4;\n\1    if \2 {\n\1        \5\n\1    } else { continue; }\n\1}',
                new_line
            )
            
            out.append(new_line)
        
        new_content = '\n'.join(out)
        if new_content != original:
            write(path, new_content)
            print(f"  {relativize(path)}")

# ============================================================
# Pass 4: Delta delta for bytecode versions
# ============================================================
print("\n=== Pass 4: Delta delta (bytecode 4..=9) ===")
bc_path = os.path.join(BASE, "luau-lifter/src/deserializer/bytecode.rs")
if os.path.exists(bc_path):
    content = read(bc_path)
    orig = content
    content = content.replace("version > 6", "version > 9")
    content = content.replace("version < 4 || version > 6", "version < 4 || version > 9")
    if content != orig:
        write(bc_path, content)
        print(f"  {relativize(bc_path)}")

# ============================================================
# Pass 5: Add iOS FFI exports
# ============================================================
print("\n=== Pass 5: iOS FFI exports ===")
lib_path = os.path.join(BASE, "luau-lifter/src/lib.rs")
if os.path.exists(lib_path):
    content = read(lib_path)
    ffi = """

// ============================================================================
// iOS FFI exports — extern "C" for linking into the iOS dylib.
// Matches native interface: 0xb6cca8 / 0xb71828
// ============================================================================

#[cfg(target_os = "ios")]
#[no_mangle]
pub extern "C" fn rust_decompile_luau(
    bytecode: *const u8,
    length: usize,
    encode_key: u8,
) -> *mut std::os::raw::c_char {
    if bytecode.is_null() || length == 0 {
        return std::ptr::null_mut::<std::os::raw::c_char>();
    }
    let slice = unsafe { std::slice::from_raw_parts(bytecode, length) };
    let result = decompile_bytecode(slice, encode_key);
    if result.is_empty() || result.starts_with("-- ") {
        let c_msg = std::ffi::CString::new(result).ok();
        c_msg.map(|s| s.into_raw()).unwrap_or(std::ptr::null_mut())
    } else {
        let c_str = std::ffi::CString::new(result).ok();
        c_str.map(|s| s.into_raw()).unwrap_or(std::ptr::null_mut())
    }
}

#[cfg(target_os = "ios")]
#[no_mangle]
pub extern "C" fn rust_free_c_string(ptr: *mut std::os::raw::c_char) {
    if ptr.is_null() { return; }
    unsafe { let _ = std::ffi::CString::from_raw(ptr); }
}

#[cfg(not(target_os = "ios"))]
pub extern "C" fn rust_decompile_luau(
    _bytecode: *const u8, _length: usize, _encode_key: u8,
) -> *mut std::os::raw::c_char {
    std::ptr::null_mut()
}

#[cfg(not(target_os = "ios"))]
pub extern "C" fn rust_free_c_string(_ptr: *mut std::os::raw::c_char) {}
"""
    content = content.rstrip()
    if content.endswith('}'):
        content = content[:-1] + ffi + '\n}\n'
    else:
        content += ffi
    write(lib_path, content)
    print(f"  {relativize(lib_path)}")

print("\n=== Done ===")
