// Bridge to Rust Luau decompiler FFI.
// Calls rust_decompile_luau() / rust_free_c_string() from the linked
// libluau_lifter.a. Evidence-based reconstruction; not vendor original source.

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

// FFI declarations matching delta-rust/medal-baseline/luau-lifter/src/lib.rs
extern "C" {
    char* rust_decompile_luau(const unsigned char* bytecode, std::size_t length, unsigned char encode_key);
    void  rust_free_c_string(char* ptr);
}

extern "C" char* reconstructed_decompile_luau(const std::uint8_t* bytecode,
                                               std::size_t length,
                                               std::uint8_t encode_key) {
    if (!bytecode || !length)
        return nullptr;

    const std::uint8_t version = bytecode[0];
    if (version < 4 || version > 9) {
        // Unsupported bytecode version — return an error string consistent
        // with the native deserialization error path.
        const char* err = "Unsupported bytecode version";
        std::size_t len = std::strlen(err);
        char* buf = static_cast<char*>(std::malloc(len + 1));
        if (buf) {
            std::memcpy(buf, err, len + 1);
        }
        return buf;
    }

    // Delegate to the Rust decompiler (Medal baseline + Delta delta).
    return rust_decompile_luau(bytecode, length, encode_key);
}

extern "C" void reconstructed_free_decompile_result(char* result) {
    if (!result)
        return;

    // Call Rust's free helper, which handles the Rust-allocated string.
    rust_free_c_string(result);
}
