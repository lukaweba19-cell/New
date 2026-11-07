#!/usr/bin/env python3
"""
Generate minimal valid Luau bytecode chunks for testing the decompiler.

Luau bytecode format (versions 4-9):
  - version: u8 (4-9 supported by Delta fork)
  - types_version: u8 (0 for old, 3 for typed)
  - string_table: ULEB128 count + parse_string entries
  - types (v3 only): ULEB128 entries terminated by \0
  - functions: ULEB128 count + Function entries
  - main: ULEB128 function index

Function:
  - max_stack_size: u8
  - num_parameters: u8
  - num_upvalues: u8
  - is_vararg: u8 (0/1)
  - flags: u8
  - p_f_ints: ULEB128 list of u8
  - instructions: ULEB128 count + u32 entries
  - constants: ULEB128 count + Constant entries
  - functions: ULEB128 count + ULEB128 child indices
  - line_defined: ULEB128
  - function_name: ULEB128 (string table index)
  - has_line_info: u8
  - line_gap_log2: u8 (if has_line_info)
  - line_info_delta: ULEB128 list (if has_line_info)
  - abs_line_info_delta: ULEB128 list (if has_line_info)
  - debug info (skipped if has_line_info=0)
"""

import struct
import sys
import os

def write_uleb128(value):
    """Encode an unsigned integer as ULEB128."""
    result = bytearray()
    while True:
        byte = value & 0x7F
        value >>= 7
        if value != 0:
            byte |= 0x80
        result.append(byte)
        if value == 0:
            break
    return bytes(result)

def make_chunk(version=4, types_version=0, string_table=None, functions=None, main=0):
    """Build a Luau bytecode chunk."""
    if string_table is None:
        string_table = []
    if functions is None:
        functions = []

    data = bytearray()
    data.append(version)
    data.append(types_version)

    # String table
    data += write_uleb128(len(string_table))
    for s in string_table:
        data += write_uleb128(len(s))
        data += s.encode('utf-8')

    # Types (v3 only)
    if types_version == 3:
        for _ in string_table:
            data += write_uleb128(0)
        data.append(0x00)  # terminator

    # Functions
    data += write_uleb128(len(functions))
    for fn in functions:
        data += fn

    # Main function index
    data += write_uleb128(main)

    return bytes(data)

def make_function(max_stack=2, num_params=0, num_upvalues=0, is_vararg=0,
                  instructions=None, constants=None, child_indices=None,
                  line_defined=0, function_name_idx=0, has_line_info=0):
    """Build a Luau function body."""
    if instructions is None:
        instructions = []
    if constants is None:
        constants = []
    if child_indices is None:
        child_indices = []

    data = bytearray()
    data.append(max_stack)
    data.append(num_params)
    data.append(num_upvalues)
    data.append(is_vararg)
    data.append(0)  # flags

    # p_f_ints (list of u8)
    data += write_uleb128(len([]))  # empty list

    # Instructions (u32 each, preceded by count as ULEB128)
    data += write_uleb128(len(instructions))
    for insn in instructions:
        data += struct.pack('<I', insn)

    # Constants
    data += write_uleb128(len(constants))
    for const in constants:
        data += const

    # Child function indices
    data += write_uleb128(len(child_indices))
    for idx in child_indices:
        data += write_uleb128(idx)

    data += write_uleb128(line_defined)
    data += write_uleb128(function_name_idx)
    data.append(has_line_info)

    if has_line_info:
        data.append(0)  # line_gap_log2
        data += write_uleb128(0)  # line_info_delta count
        data += write_uleb128(0)  # abs_line_info_delta count

    return bytes(data)

def make_constant_string(s):
    """Make a string constant (type tag + data)."""
    data = bytearray()
    data.append(3)  # type tag for string
    data += write_uleb128(len(s))
    data += s.encode('utf-8')
    return bytes(data)

def make_constant_number(n):
    """Make a number constant (type tag + f64)."""
    data = bytearray()
    data.append(1)  # type tag for number
    data += struct.pack('<d', n)
    return bytes(data)

def make_constant_nil():
    """Make a nil constant."""
    return bytes([0, 0x00])  # type 0 (nil) + ULEB128 0

def make_constant_boolean(b):
    """Make a boolean constant."""
    return bytes([2, 1 if b else 0])

# opcodes (Luau, encode_key=1)
LOP_NOP     = 0
LOP_BREAK   = 1
LOP_LOADNIL = 2
LOP_LOADB   = 3
LOP_LOADN   = 4
LOP_LOADK   = 5
LOP_MOVE    = 6
LOP_GETGLOBAL = 7
LOP_SETGLOBAL = 8
LOP_GETUPVAL  = 9
LOP_SETUPVAL  = 10
LOP_CLOSEUPVALS = 11
LOP_GETIMPORT  = 12
LOP_GETTABLE   = 13
LOP_SETTABLE   = 14
LOP_GETTABLEKS = 15
LOP_SETTABLEKS = 16
LOP_GETTABLEN  = 17
LOP_SETTABLEN  = 18
LOP_NEWCLOSURE = 19
LOP_NAMECALL   = 20
LOP_CALL       = 21
LOP_RETURN     = 22
LOP_JUMP       = 23
LOP_JUMPBACK   = 24
LOP_JUMPIF     = 25
LOP_JUMPIFNOT  = 26
LOP_JUMPIFEQ   = 27
LOP_JUMPIFLE   = 28
LOP_JUMPIFLT   = 29
LOP_JUMPIFNOTEQ = 30
LOP_JUMPIFNOTLE = 31
LOP_JUMPIFNOTLT = 32
LOP_ADD  = 33
LOP_SUB  = 34
LOP_MUL  = 35
LOP_DIV  = 36
LOP_MOD  = 37
LOP_POW  = 38
LOP_ADDK = 39
LOP_SUBK = 40
LOP_MULK = 41
LOP_DIVK = 42
LOP_MODK = 43
LOP_POWK = 44
LOP_AND  = 45
LOP_OR   = 46
LOP_ANDK = 47
LOP_ORK  = 48
LOP_CONCAT = 49
LOP_NOT    = 50
LOP_MINUS  = 51
LOP_LENGTH = 52
LOP_NEWTABLE = 53
LOP_DUPTABLE = 54
LOP_SETLIST  = 55
LOP_FORNPREP    = 56
LOP_FORNLOOP    = 57
LOP_FORGLOOP    = 58
LOP_FORGPREP_INEXT = 59
LOP_FORGLOOP_INEXT = 60  # was FORGPREP_NEXT in some versions
LOP_FORGPREP_NEXT  = 60
LOP_FASTCALL3  = 61
LOP_NATIVECALL = 62
LOP_GETVARARGS = 63
LOP_DUPCLOSURE = 64
LOP_PREPVARARGS = 65
LOP_LOADKX = 66
LOP_JUMPX = 67
LOP_FASTCALL = 68
LOP_COVERAGE = 69
LOP_CAPTURE = 70
LOP_JUMPIFEQK    = 71
LOP_JUMPIFNOTEQK = 72
LOP_FASTCALL1 = 73
LOP_FASTCALL2 = 74
LOP_FASTCALL2K = 75
LOP_FORGPREP = 76
LOP_JUMPXEQKNIL = 77
LOP_JUMPXEQKB   = 78
LOP_JUMPXEQKN   = 79
LOP_JUMPXEQKS   = 80
LOP_IDIV   = 81
LOP_IDIVK  = 82
LOP_SUBRK  = 83
LOP_DIVRK  = 84
LOP_FORGLOOP_NEXT = 85  # some versions
LOP__COUNT = 86

def encode_bc(opcode, a=0, b=0, c=0, aux=0):
    """Encode a BC (3-register) instruction."""
    insn = opcode & 0xFF
    insn |= (a & 0xFF) << 8
    insn |= (b & 0xFF) << 16
    insn |= (c & 0xFF) << 24
    # aux is encoded in the next instruction word for some ops,
    # but for simplicity we pack it here only when needed.
    # The deserializer handles aux from the next word.
    return struct.pack('<I', insn)

def encode_ad(opcode, a=0, d=0):
    """Encode an AD (register + delta) instruction."""
    insn = opcode & 0xFF
    insn |= (a & 0xFF) << 8
    # d is a signed 16-bit value in bits 16-31
    insn |= (d & 0xFFFF) << 16
    return struct.pack('<I', insn)

def encode_e(opcode, e=0):
    """Encode an E (extended jump) instruction."""
    insn = opcode & 0xFF
    insn |= (e & 0xFFFFFF) << 8
    return struct.pack('<I', insn)

def simple_print_function():
    """A function that does: print('hello')"""
    # LOP_LOADK  R0 = "hello" (const index 0)
    # LOP_GETGLOBAL R1 = print (const index 1)
    # LOP_CALL    R1(print), args=[R0], results=0 (multret tail)
    # LOP_RETURN R0, 1
    instrs = [
        encode_bc(LOP_LOADK, a=0, d=0),       # R0 = const[0] ("hello")
        encode_bc(LOP_GETGLOBAL, a=1, c=0, aux=1),  # R1 = global "print" (const[1])
        encode_bc(LOP_CALL, a=1, b=2, c=0),  # call R1 with 1 arg (R0), multret
        encode_bc(LOP_RETURN, a=0, b=1),     # return R0, 1 result
    ]
    # Add NOP aux words for instructions that need them
    # LOP_LOADK with aux: no, it's AD format
    # Actually LOP_LOADK is AD format (opcode 5 is in AD range)
    # Let me fix: LOP_LOADK = 5, which is in the AD range (4,5,12,19,23..=32,...)
    # The public baseline uses AD format for opcode 5.

    # For the baseline, LOP_LOADK uses AD encoding with d = constant index
    instrs = [
        encode_ad(LOP_LOADK, a=0, d=0),       # R0 = const[0]
        encode_bc(LOP_GETGLOBAL, a=1, c=0, aux=1),  # R1 = global print
        encode_bc(LOP_CALL, a=1, b=2, c=0),
        encode_bc(LOP_RETURN, a=0, b=1),
    ]
    # GETGLOBAL is opcode 7 (BC range), SETGLOBAL is 8 (BC range)
    # CALL is 21, RETURN is 22 -> both BC
    # GETGLOBAL aux = constant index

    fn = make_function(
        max_stack=2,
        num_params=0,
        num_upvalues=0,
        instructions=instrs,
        constants=[
            make_constant_string("hello"),
            make_constant_string("print"),
        ],
        function_name_idx=0,
    )
    return fn

def make_hello_chunk():
    """Chunk: main prints 'hello' via print()"""
    fn = simple_print_function()
    # String table: ["main", "hello", "print"]
    return make_chunk(
        version=4,
        string_table=["main", "hello", "print"],
        functions=[fn],
        main=0,
    )

def make_add_function():
    """A function: function add(a, b) return a + b end
    Then calls: local x = add(1, 2)
    """
    # add function body:
    # R0 = param a, R1 = param b
    # R2 = R0 + R1
    # return R2
    add_instrs = [
        encode_bc(LOP_ADD, a=2, b=0, c=1),    # R2 = R0 + R1
        encode_bc(LOP_RETURN, a=2, b=1),       # return R2
    ]
    add_fn = make_function(
        max_stack=3,
        num_params=2,
        num_upvalues=0,
        instructions=add_instrs,
        function_name_idx=1,  # "add"
    )

    # main function:
    # R0 = closure of add (NEWCLOSURE)
    # R1 = 1 (LOADK)
    # R2 = 2 (LOADK)
    # R3 = R0(1, 2) via FASTCALL or CALL
    # capture result
    main_instrs = [
        encode_ad(LOP_NEWCLOSURE, a=0, d=0),   # R0 = closure of function[0] (add)
        # CAPTURE instructions for upvalues (none needed for add since no upvalues)
        encode_bc(LOP_LOADK, a=1, d=0) if False else encode_ad(LOP_LOADK, a=1, d=1),  # R1 = const[1] (1.0)
        encode_ad(LOP_LOADK, a=2, d=2),         # R2 = const[2] (2.0)
        encode_bc(LOP_CALL, a=0, b=3, c=1),     # R0 = add(1, 2), 1 result
        encode_bc(LOP_RETURN, a=0, b=1),        # return R0
    ]
    main_fn = make_function(
        max_stack=4,
        num_params=0,
        num_upvalues=0,
        instructions=main_instrs,
        constants=[
            make_constant_number(1.0),
            make_constant_number(2.0),
        ],
        child_indices=[0],  # add is function 0
        function_name_idx=0,  # "main"
    )

    return make_chunk(
        version=4,
        string_table=["main", "add"],
        functions=[add_fn, main_fn],
        main=1,  # main is the second function (index 1)
    )

if __name__ == '__main__':
    out_dir = sys.argv[1] if len(sys.argv) > 1 else '/tmp'
    os.makedirs(out_dir, exist_ok=True)

    hello = make_hello_chunk()
    with open(os.path.join(out_dir, 'test_hello.bin'), 'wb') as f:
        f.write(hello)
    print(f"Generated test_hello.bin: {len(hello)} bytes (version={hello[0]})")

    add = make_add_function()
    with open(os.path.join(out_dir, 'test_add.bin'), 'wb') as f:
        f.write(add)
    print(f"Generated test_add.bin: {len(add)} bytes (version={add[0]})")

    # Also generate a version 9 chunk (Delta supports 4-9)
    add_v9 = bytearray(add)
    add_v9[0] = 9  # upgrade version
    with open(os.path.join(out_dir, 'test_add_v9.bin'), 'wb') as f:
        f.write(bytes(add_v9))
    print(f"Generated test_add_v9.bin: {len(add_v9)} bytes (version=9)")
