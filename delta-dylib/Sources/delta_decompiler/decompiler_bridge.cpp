/**
 * Delta Decompiler Dylib Bridge
 *
 * Wraps the Rust luau-lifter (Medal lineage, Delta-fork with bytecode
 * versions 4-9 support) inside a C++ dylib that exposes the same
 * extern "C" interface recovered from libgloop.dylib:
 *
 *   char* decompile_luau_bytecode(const uint8_t* bytecode, size_t length, uint8_t encode_key)
 *   void  free_decompiled_c_string(char* result)
 *
 * These correspond to native addresses:
 *   0xb6cca8  decompile_luau_bytecode_with_encoding_key
 *   0xb71828  free_decompiled_c_string
 *
 * The third argument is the Luau opcode encoding key:
 *   - Ordinary Luau bytecode: 1
 *   - Roblox client bytecode:  203
 *
 * The returned string is a NUL-terminated heap allocation. The caller
 * must free it with free_decompiled_c_string().
 *
 * This bridges to the Rust `decompile_bytecode(bytecode: &[u8], encode_key: u8) -> String`
 * function from the medal2-baseline luau-lifter crate.
 */

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

// Rust FFI: the luau-lifter exposes its decompiler through this interface.
// The medal2-baseline luau-lifter crate has a pub fn decompile_bytecode
// in its lib.rs. When linked as a static lib, the symbol is:
//   _ZN12luau_lifter17decompile_bytecodeE
extern "C" {
    // decompile_bytecode(bytecode: &[u8], encode_key: u8) -> String
    // Rust ABI: (ptr: *const u8, len: usize, key: u8) -> *mut c_char
    // The Rust String is converted to a C string by the bridge.
    const char* rust_decompile_luau(
        const uint8_t* bytecode,
        size_t length,
        uint8_t encode_key
    );
}

/**
 * Native entry point recovered from libgloop.dylib 0xb6cca8.
 *
 * Native signature (from reverse engineering):
 *   char* decompile_luau_bytecode_with_encoding_key(const uint8_t* bytecode,
 *                                                   size_t length,
 *                                                   uint8_t encode_key)
 *
 * The native function:
 *   1. Validates bytecode != null && length > 0
 *   2. Reads bytecode[0] as version, requires 4..=9 (Delta fork; public
 *      baseline only supports 4..=6)
 *   3. Passes (bytecode, length, encode_key) through the Rust lifter
 *   4. Returns a NUL-terminated heap-allocated C string, or null on failure
 *
 * Failure paths return specific error strings:
 *   "-- deserialization failed"
 *   "-- decompilation panicked"
 *   "-- failed to decompile"
 *   "Unsupported bytecode version: N"
 */
extern "C" char* decompile_luau_bytecode_with_encoding_key(
    const uint8_t* bytecode,
    size_t length,
    uint8_t encode_key
) {
    if (!bytecode || length == 0) {
        return nullptr;
    }

    // Validate bytecode version byte
    const uint8_t version = bytecode[0];
    if (version < 4 || version > 9) {
        // Match the native error format
        const char* msg = "-- unsupported bytecode version";
        char* out = (char*)std::malloc(std::strlen(msg) + 1);
        if (out) {
            std::strcpy(out, msg);
        }
        return out;
    }

    // Call the Rust decompiler. The Rust function returns a C-string pointer
    // (the Rust side allocates with malloc and returns a *const c_char).
    const char* result = rust_decompile_luau(bytecode, length, encode_key);

    if (!result) {
        const char* msg = "-- deserialization failed";
        char* out = (char*)std::malloc(std::strlen(msg) + 1);
        if (out) {
            std::strcpy(out, msg);
        }
        return out;
    }

    // The Rust side returns a malloc'd C string. We return it directly.
    // The caller must free with free_decompiled_c_string().
    if (result[0] == '\0') {
        // Empty result -> treat as failure
        std::free((void*)result);
        const char* msg = "-- failed to decompile";
        char* out = (char*)std::malloc(std::strlen(msg) + 1);
        if (out) {
            std::strcpy(out, msg);
        }
        return out;
    }

    return (char*)result;
}

/**
 * Native entry point recovered from libgloop.dylib 0xb71828.
 *
 * Native behavior:
 *   1. strlen(result)
 *   2. result[0] = '\0'
 *   3. if length != SIZE_MAX: free(result)
 *
 * This is the paired destructor for the string returned by
 * decompile_luau_bytecode_with_encoding_key.
 */
extern "C" void free_decompiled_c_string(char* result) {
    if (!result) {
        return;
    }

    // Measure and clear the string (native behavior fidelity)
    const size_t length = std::strlen(result);
    result[0] = '\0';

    // Free the allocation (skip on SIZE_MAX, which shouldn't happen for
    // a real strlen result, but preserved for binary fidelity)
    if (length != static_cast<size_t>(-1)) {
        std::free(result);
    }
}

/**
 * Convenience wrapper: decompile Luau bytecode with the Roblox client
 * encoding key (203). This is what a Roblox executor would use when
 * decompiling scripts downloaded from the Roblox network.
 */
extern "C" char* decompile_roblox_bytecode(const uint8_t* bytecode, size_t length) {
    return decompile_luau_bytecode_with_encoding_key(bytecode, length, 203);
}

/**
 * Version query: returns the supported bytecode version range.
 * Format: "4-9" (Delta fork supports versions 4 through 9).
 */
extern "C" const char* decompiler_version_string(void) {
    static const char version[] = "Delta Luau Decompiler v1.0 (bytecode 4-9, encode_key multiplier)";
    return version;
}
