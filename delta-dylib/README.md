# Delta Decompiler iOS Dylib

iOS dynamic library that embeds the Luau decompiler (Medal lineage, Delta-fork)
and exposes it to a Roblox client process via `extern "C"` entry points matching
the interface recovered from `libgloop.dylib`.

## What this builds

A single iOS arm64 dylib (`libDeltaDecompiler.dylib`) that provides:

```
char* decompile_luau_bytecode_with_encoding_key(const uint8_t* bytecode, size_t length, uint8_t encode_key)
void  free_decompiled_c_string(char* result)
char* decompile_roblox_bytecode(const uint8_t* bytecode, size_t length)
const char* decompiler_version_string(void)
```

These match the native entry points at `0xb6cca8` and `0xb71828` in the
reverse-engineered Delta binary.

## Architecture

```
Roblox Process (host)
    │
    ▼  (dylib injected / loadable)
libDeltaDecompiler.dylib  ── iOS arm64 dynamic library
    │
    ├── decompiler_bridge.cpp          C++ extern "C" entry points
    │
    └── libluau_lifter.a              Rust static library (from medal2-baseline)
            │
            ├── luau-lifter/src/lib.rs     decompile_bytecode() — Rust core
            ├── luau-lifter/src/lifter.rs  bytecode → AST lifting
            ├── luau-lifter/src/instruction.rs  ARM64→Luau opcode decoding
            ├── luau-lifter/src/deserializer/  bytecode parsing
            └── ... (ast/, cfg/, restructure/)
```

## What's included from the reverse engineering

| Component | Source | Status |
|-----------|--------|--------|
| Luau decompiler core (Rust) | `delta-rust/` (Medal baseline + Delta delta) | Buildable |
| Bytecode version support 4–9 | Delta delta applied to `bytecode.rs` | Done |
| `extern "C"` bridge matching 0xb6cca8/0xb71828 | `decompiler_bridge.cpp` | Done |
| Rust C FFI exports (`rust_decompile_luau`) | Added to `luau-lifter/src/lib.rs` | Done |
| XcodeGen project spec | `project.yml` | Done |
| GitHub Actions workflow | `.github/workflows/build.yml` | Done |
| Test bytecode generators | `gen_test_bytecode.py` | Done |
| Recovered UI assets | `../iOS/iOS/assets/` | Available |
| Reconstructed semantics (bootstrap, dyld, etc.) | `../iOS/iOS/reconstructed-src/` | Reference only |

## What's NOT included (per requirement)

- Keychain / authentication system
- Network transport (HTTP/WebSocket)
- Protected executor API registration table (names not recovered)
- Anti-analysis / decoy loader code
- iOS integration layer (ObjC class registration, lifecycle observer)

## Building locally (on macOS with Xcode)

### Prerequisites

- macOS
- Xcode 15+ with iOS 14.0+ SDK
- Rust stable toolchain with `aarch64-apple-ios` target
- XcodeGen (`brew install xcodegen`)

### Steps

```bash
# 1. Install Rust
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y
source "$HOME/.cargo/env"
rustup target add aarch64-apple-ios

# 2. Build the Rust static library for iOS
cd delta-dylib/delta-rust
cargo build --release --lib -p luau-lifter --target aarch64-apple-ios
cd ../..

# 3. Generate Xcode project (requires XcodeGen)
cd delta-dylib
xcodegen generate
cd ..

# 4. Build the dylib
cd delta-dylib
xcodebuild archive \
  -project DeltaDylib.xcodeproj \
  -scheme DeltaDecompiler \
  -configuration Release \
  -destination generic/platform=iOS \
  -archivePath build/DeltaDecompiler.xcarchive \
  CODE_SIGN_IDENTITY="" CODE_SIGNING_REQUIRED=NO CODE_SIGNING_ALLOWED=NO \
  ONLY_ACTIVE_ARCH=NO

# The output is at:
# build/DeltaDecompiler.xcarchive/Products/Library/Frameworks/DeltaDecompiler.framework/DeltaDecompiler
```

### Alternative: direct clang build (no Xcode project)

```bash
RUST_LIB="delta-dylib/delta-rust/target/aarch64-apple-ios/release/libluau_lifter.a"
SDK_PATH="$(xcrun --sdk iphoneos --show-sdk-path)"

clang -dynamiclib \
  -target aarch64-apple-ios \
  -arch arm64 \
  -isysroot "$SDK_PATH" \
  -fmodules -fcxx-modules -stdlib=libc++ \
  -miphoneos-version-min=14.0 \
  -O3 \
  -o delta-dylib/build/libDeltaDecompiler.dylib \
  delta-dylib/Sources/delta_decompiler/decompiler_bridge.cpp \
  -L"$(dirname "$RUST_LIB")" -lluau_lifter \
  -lstdc++ -lc++ \
  -force_load "$RUST_LIB"
```

## Using the decompiler in a Roblox client

The dylib exposes `decompile_roblox_bytecode()` which uses the Roblox client
encoding key (203). To use it from a Roblox process:

1. **Inject/load the dylib** — via `dlopen` from a loader, or through the
   Roblox `libgloop.dylib` loading mechanism if you have that in place.

2. **Call the entry point:**
   ```cpp
   const uint8_t* bytecode = /* Luau bytecode from Roblox script */;
   size_t length = /* bytecode length */;

   char* source = decompile_roblox_bytecode(bytecode, length);
   if (source) {
       // source is a NUL-terminated C string with the decompiled Luau code
       printf("%s\n", source);
       free_decompiled_c_string(source);
   }
   ```

3. **Encoding key:** Roblox client bytecode uses key 203. The native
   `decompile_luau_bytecode_with_encoding_key` multiplies each encoded opcode
   byte by the key (wrapping u8 arithmetic) to recover the real opcode.

## What's missing / needs work

The following components need to be completed to get a fully working executor:

1. **Dylib loading/injection into Roblox** — How the dylib gets loaded into
   the Roblox process. This is app-specific and not covered here.

2. **Roblox script bytecode extraction** — Getting the raw Luau bytecode from
   the Roblox client. The Roblox app caches scripts; the path to extract them
   is platform-specific.

3. **Executor API registration** — The protected table that maps Luau-facing
   API names to native functions. The plaintext names were not recovered; only
   the pattern (split-pointer indirect calls) is known.

4. **UI layer** — The recovered assets (`assets/` from `gloop/dlt.zip`) give
   the artwork, but no UI code is reconstructed.

5. **Network/transport** — If the executor needs to fetch scripts from a
   server, the WebSocket/HTTP transport (ixwebsocket, cpp-httplib) from the
   original binary would need to be reimplemented.

6. **Lua 5.1 deserializer** — The `lua51-deserializer/` crate exists in the
   workspace but is for Lua 5.1 bytecode (older format), not Luau. The main
   decompiler is `luau-lifter/`.

## Test bytecode

Generate test Luau bytecode chunks:

```bash
cd delta-dylib/delta-rust
python3 gen_test_bytecode.py /tmp
```

This creates:
- `test_hello.bin` — a chunk that calls `print("hello")`
- `test_add.bin` — a chunk defining `add(a,b)` and calling it
- `test_add_v9.bin` — same but with version byte set to 9 (Delta supports 4-9)

Test with the built binary:

```bash
./target/release/luau-lifter /tmp/test_hello.bin --key 1
./target/release/luau-lifter /tmp/test_add.bin --key 1
./target/release/luau-lifter /tmp/test_add_v9.bin --key 1
```

## License

The `delta-rust/` directory includes the MIT-licensed Medal `luau-lifter`
baseline (`recovered-open-source/medal2-baseline/LICENSE.txt`). The Delta-specific
changes (bytecode version 4-9 support, iOS FFI bridge) are new work layered on
top. The reconstructed pseudocode and analysis artifacts in `../iOS/` are
evidence-based reconstructions, not original source.

## References

- Full reversing report: `../iOS/iOS/docs/DELTA_IOS_REVERSING_REPORT.md`
- Source lift status: `../iOS/iOS/docs/SOURCE_LIFT_STATUS.md`
- Function index: `../iOS/iOS/analysis/function_source_index.csv`
- Medal baseline: `delta-rust/` (copied from `recovered-open-source/medal2-baseline/`)
- Delta differences: `../iOS/iOS/recovered-open-source/DELTA_MATCH_NOTES.md`
