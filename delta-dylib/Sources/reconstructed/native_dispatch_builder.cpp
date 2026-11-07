// Implementation of build_native_dispatch() (native 0x2b21ec).
// Constructs the 0xe0-byte (28-slot x 8-byte) NativeDispatch table.
// Evidence-based reconstruction; not vendor original source.

#include "native_dispatch.hpp"

#include <cstdlib>

// The dispatch table is populated with function pointers resolved from the
// loaded process. In the native function, these are looked up via dladdr /
// dlsym against known symbols or through the dyld introspection hooks.
//
// The protected arithmetic that selects each slot has been collapsed here;
// slot assignment is based on the recovered layout in native_dispatch.hpp.

namespace {

// Forward declarations for the functions we resolve. In the actual binary
// these are resolved at runtime via dlsym/dladdr; here we rely on the
// linker providing them.

extern "C" {
void* dyld_stub_binder(void); // placeholder
}

// Provide stub implementations for functions that may not be available
// as real symbols during linking. The native dispatch stores raw function
// pointers; if a symbol is unavailable the slot may hold nullptr or a
// resolver stub.

static void* resolve_symbol(const char* name) {
    // In the native path this uses dlsym(RTLD_DEFAULT, name) after
    // resolving the handle through dyld_image_count/dyld_get_image_name
    // iteration. For the reconstruction we use RTLD_DEFAULT directly.
    void* ptr = nullptr;
#if defined(__APPLE__)
    // On iOS the symbol must be available; RTLD_DEFAULT works for
    // exported symbols in the main executable and loaded images.
    dlopen("/dev/null", RTLD_NOLOAD); // ensure RTLD_DEFAULT is initialized
    ptr = dlsym(RTLD_DEFAULT, name);
#endif
    return ptr;
}

} // namespace

NativeDispatch* build_native_dispatch() {
    // Allocate the dispatch table (0xe0 = 224 bytes).
    // In the native function this uses the malloc slot from a previously
    // built dispatch or falls back to a direct allocation.
    NativeDispatch* dispatch = static_cast<NativeDispatch*>(std::calloc(1, sizeof(NativeDispatch)));
    if (!dispatch)
        return nullptr;

    // Populate the 28 slots. The order matches the recovered layout in
    // native_dispatch.hpp. Functions that are not independently resolved
    // remain nullptr (the native code handles missing slots defensively).

    // Slot 0x00 — signal
    dispatch->signal_fn = resolve_symbol("__signal");
    // Slot 0x08 — realloc
    dispatch->realloc_fn = resolve_symbol("realloc");
    // Slot 0x10 — ftell
    dispatch->ftell_fn = resolve_symbol("ftell");
    // Slot 0x18 — reserved
    // Slot 0x20 — pread
    dispatch->pread_fn = resolve_symbol("pread");
    // Slot 0x28 — dlopen
    dispatch->dlopen_fn = resolve_symbol("dlopen");
    // Slot 0x30 — dladdr
    dispatch->dladdr_fn = resolve_symbol("dladdr");
    // Slot 0x38 — reserved
    // Slot 0x40 — mprotect
    dispatch->mprotect_fn = resolve_symbol("mprotect");
    // Slot 0x48 — memcpy
    dispatch->memcpy_fn = resolve_symbol("memcpy");
    // Slot 0x50 — memset
    dispatch->memset_fn = resolve_symbol("memset");
    // Slot 0x58 — fclose
    dispatch->fclose_fn = resolve_symbol("fclose");
    // Slot 0x60 — sysconf
    dispatch->sysconf_fn = resolve_symbol("sysconf");
    // Slot 0x68 — remove
    dispatch->remove_fn = resolve_symbol("remove");
    // Slot 0x70 — fread
    dispatch->fread_fn = resolve_symbol("fread");
    // Slot 0x78 — malloc
    dispatch->malloc_fn = resolve_symbol("malloc");
    // Slot 0x80 — dyld_image_count
    dispatch->dyld_image_count_fn = resolve_symbol("dyld_image_count");
    // Slot 0x88 — chmod
    dispatch->chmod_fn = resolve_symbol("chmod");
    // Slot 0x90 — stat
    dispatch->stat_fn = resolve_symbol("stat");
    // Slot 0x98 — dlsym
    dispatch->dlsym_fn = resolve_symbol("dlsym");
    // Slot 0xa0 — fgets
    dispatch->fgets_fn = resolve_symbol("fgets");
    // Slot 0xa8 — fseek
    dispatch->fseek_fn = resolve_symbol("fseek");
    // Slot 0xb0 — fwrite
    dispatch->fwrite_fn = resolve_symbol("fwrite");
    // Slot 0xb8 — fopen
    dispatch->fopen_fn = resolve_symbol("fopen");
    // Slot 0xc0 — dyld_get_image_name
    dispatch->dyld_get_image_name_fn = resolve_symbol("dyld_get_image_name");
    // Slot 0xc8 — free
    dispatch->free_fn = resolve_symbol("free");
    // Slot 0xd0 — dyld_get_image_slide
    dispatch->dyld_get_image_slide_fn = resolve_symbol("dyld_get_image_slide");
    // Slot 0xd8 — mmap
    dispatch->mmap_fn = resolve_symbol("mmap");

    return dispatch;
}
