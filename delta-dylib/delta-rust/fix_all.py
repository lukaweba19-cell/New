#!/usr/bin/env python3
"""
Comprehensive fix for medal2-baseline Rust code.

Handles:
1. Remove all #![feature(...)] gates
2. Replace `box PATTERN` match arms with match ergonomics (remove `box` keyword)
3. Replace `box EXPR` constructions with `Box::new(EXPR)`
4. Replace `&& let PAT = EXPR` (let_chains) with nested if-let blocks
5. Apply Delta delta: bytecode versions 4..=9
6. Add iOS FFI exports to luau-lifter/src/lib.rs
"""

import re, os, sys

BASE = "delta-dylib/delta-rust/medal-baseline"

def read(path):
    with open(path) as f:
        return f.read()

def write(path, content):
    with open(path, "w") as f:
        f.write(content)

# ============================================================
# Step 1: Remove all #![feature(...)] gates
# ============================================================
for root, dirs, files in os.walk(BASE):
    for f in files:
        if f.endswith(".rs"):
            path = os.path.join(root, f)
            content = read(path)
            new_content = re.sub(r'#!\[feature\([^]]*\)\]\n', '', content)
            if new_content != content:
                write(path, new_content)
                print(f"Removed feature gates: {path}")

# ============================================================
# Step 2: Replace `box PATTERN` in match arms with match ergonomics
# ============================================================
# In Rust 2021, `box PATTERN` is not available.
// Match ergonomics means we can just use the pattern directly
// and bind by reference. So `box RValue::Literal(...)` becomes just
// `RValue::Literal(...)` and the bound variable is a reference.
// But we need to handle the case where the code constructs a Box:
// `box expr` -> `Box::new(expr)`

def fix_file(path):
    content = read(path)
    original = content

    # --- Replace `box IDENTIFIER @ PATTERN` in match arms ---
    # `box value @ RValue::Unary(...)` -> `value @ RValue::Unary(...)`
    # (match ergonomics handles the ref binding)
    content = re.sub(r'\bbox\s+(\w+)\s+@', r'\1 @', content)

    # --- Replace `box PATTERN` where PATTERN is a struct pattern ---
    # `box RValue::Literal(...)` in match arm -> `RValue::Literal(...)`
    content = re.sub(r'\bbox\s+(RValue::\w+\s*\([^)]*\))', r'\1', content)
    content = re.sub(r'\bbox\s+(RValue::\w+\s*\{[^}]*\})', r'\1', content)
    content = re.sub(r'\bbox\s+(Unary\s*\{[^}]*\})', r'\1', content)
    content = re.sub(r'\bbox\s+(Binary\s*\{[^}]*\})', r'\1', content)
    content = re.sub(r'\bbox\s+(Index\s*\{[^}]*\})', r'\1', content)
    content = re.sub(r'\bbox\s+(Global\s*\([^)]*\))', r'\1', content)
    content = re.sub(r'\bbox\s+(Local\s*\([^)]*\))', r'\1', content)
    content = re.sub(r'\bbox\s+(\w+)', r'\1', content)  # box ident -> ident

    # --- Replace `box EXPR` constructions (not in match patterns) ---
    # These are `box RValue::Literal(...)` used as function arguments or assignments
    # Need to distinguish from match patterns. We do this after the pattern replacement.
    # Any remaining `box <expr>` that looks like a construction:
    content = re.sub(r'\bbox\s+(RValue::\w+\s*\([^)]*\))', r'Box::new(\1)', content)
    content = re.sub(r'\bbox\s+(RValue::\w+\s*\{[^}]*\})', r'Box::new(\1)', content)
    content = re.sub(r'\bbox\s+(Unary\s*\{[^}]*\})', r'Box::new(\1)', content)
    content = re.sub(r'\bbox\s+(Literal::\w+\s*\([^)]*\))', r'Box::new(\1)', content)
    content = re.sub(r'\bbox\s+ref\s+(\w+)', r'\1', content)

    # ============================================================
    # Step 3: Replace `&& let PAT = EXPR` (let_chains) 
    # ============================================================
    # Strategy: Convert `COND1 && let PAT = EXPR && COND2` into
    # a block: `{ let PAT = EXPR; if COND1 { if COND2 { ... } else { continue; } } else { continue; } }`
    # But this is complex. Simpler: For each `&& let` occurrence, we need to
    // wrap the surrounding condition in an if-let.
    
    # Find lines with `&& let` and fix them
    lines = content.split('\n')
    fixed_lines = []
    i = 0
    while i < len(lines):
        line = lines[i]
        if '&& let' in line:
            # This line has a let_chain. We need to convert it.
            # Strategy: extract the let binding, wrap in if let, 
            # and keep the rest of the condition.
            
            # Match: `... && let PAT = EXPR && ...`
            # or: `... && let PAT = EXPR` (at end)
            # or: `if ... && let PAT = EXPR && ... {`
            
            # Simple approach: replace `&& let PAT = EXPR` with `;` and
            # wrap in if let before the line.
            
            # Find all `&& let PAT = EXPR` occurrences in this line
            # and convert them one by one
            
            new_line = line
            # Replace `&& let PAT = EXPR &&` with `; let PAT = EXPR; if `
            # This is tricky because we need to handle precedence.
            
            # For now, just flag these lines for manual review
            # Actually, let's do a simpler conversion:
            # `A && let B = C` -> `{ if let B = C { A } else { continue; } }`
            
            # Match the pattern at the end of a condition
            new_line = re.sub(
                r'(\s*)(.+?)\s*&&\s*let\s+(\w+(?::\w+)?)\s*=\s*([^&\n]+?)\s*&&\s*(.+)$',
                r'\1{\n\1    let \3 = \4;\n\1    if \2 {\n\1        if \5 {\n\1            \2\n\1        } else { continue; }\n\1    } else { continue; }\n\1}',
                new_line
            )
            
            # Match `&& let PAT = EXPR` at end of line (no trailing &&)
            new_line = re.sub(
                r'(\s*)(.+?)\s*&&\s*let\s+(\w+(?::\w+)?)\s*=\s*(.+?)$',
                r'\1{\n\1    let \3 = \4;\n\1    if \2 {\n\1        \2\n\1    } else { continue; }\n\1}',
                new_line
            )
            
            fixed_lines.append(new_line)
        else:
            fixed_lines.append(line)
        i += 1
    
    content = '\n'.join(fixed_lines)

    if content != original:
        write(path, content)
        print(f"Fixed: {path}")
        return True
    return False

# Apply to all Rust files
fixed_count = 0
for root, dirs, files in os.walk(BASE):
    for f in files:
        if f.endswith(".rs"):
            path = os.path.join(root, f)
            if fix_file(path):
                fixed_count += 1

print(f"\nFixed {fixed_count} files")

# ============================================================
# Step 4: Apply Delta delta to bytecode.rs
# ============================================================
bytecode_path = os.path.join(BASE, "luau-lifter/src/deserializer/bytecode.rs")
if os.path.exists(bytecode_path):
    content = read(bytecode_path)
    # The Delta fork supports bytecode versions 4-9 (public Medal baseline only 4-6)
    # Find version checks and extend the upper bound
    content = re.sub(r'version\s*>\s*6', 'version > 9', content)
    content = re.sub(r'version < 4 \|\| version > 6', 'version < 4 || version > 9', content)
    # Also check for array/slice bounds like [0..6] or similar
    content = re.sub(r'\.\.6\b', '..9', content)
    write(bytecode_path, content)
    print("Applied Delta delta to bytecode.rs")

# ============================================================
# Step 5: Add iOS FFI exports
# ============================================================
lib_path = os.path.join(BASE, "luau-lifter/src/lib.rs")
if os.path.exists(lib_path):
    content = read(lib_path)
    ffi = """

// ============================================================================
// iOS FFI — extern "C" exports for linking into the iOS dylib.
// Matches native interface recovered from libgloop.dylib:
//   0xb6cca8  decompile_luau_bytecode_with_encoding_key
//   0xb71828  free_decompiled_c_string
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
    print("Added iOS FFI exports to luau-lifter/src/lib.rs")

print("\nDone.")
