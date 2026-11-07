// Implementation of build_native_dispatch() (native 0x2b21ec).
// Constructs the 0xe0-byte (28-slot x 8-byte) NativeDispatch table.
// Evidence-based reconstruction; not vendor original source.
//
// Native behavior (from 0x50c658 trace):
//   1. mmap a 224-byte (0xe0) RW block
//   2. fill it with a 28-entry system dispatch table
//   3. store the pointer at global 0xf0d3e0
//
// The dispatch table entries are resolved at runtime from the loaded
// process. In the native function these go through the protected address
// arithmetic; here we collapse that to direct dlsym lookups.

#include "native_dispatch.hpp"

#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <sys/mman.h>

// Native 0x2b21ec allocates the dispatch block with mmap, then fills it.
// We replicate the allocation shape but use calloc for the filling step.

static NativeDispatch* allocate_dispatch_block() {
    // Native: mmap(nullptr, 0xe0, PROT_READ|PROT_WRITE,
    //             MAP_PRIVATE|MAP_ANONYMOUS, -1, 0)
    void* block = mmap(nullptr, sizeof(NativeDispatch),
                       PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (block == MAP_FAILED)
        return nullptr;
    std::memset(block, 0, sizeof(NativeDispatch));
    return static_cast<NativeDispatch*>(block);
}

// Resolve a symbol from the loaded process. In the native path these are
// addressed through the dispatch table's own slots (built up incrementally);
// here we use RTLD_DEFAULT directly.
static void* resolve_symbol(const char* name) {
    if (!name)
        return nullptr;
    dlopen("/dev/null", RTLD_NOLOAD);
    return dlsym(RTLD_DEFAULT, name);
}

NativeDispatch* build_native_dispatch() {
    NativeDispatch* dispatch = allocate_dispatch_block();
    if (!dispatch)
        return nullptr;

    // Populate the 28 slots in the recovered order from 0x50c658 trace:
    //   0x00 signal      0x70 fread
    //   0x08 realloc     0x78 malloc
    //   0x10 ftell       0x80 dyld_image_count
    //   0x18 reserved    0x88 chmod
    //   0x20 pread       0x90 stat
    //   0x28 dlopen      0x98 dlsym
    //   0x30 dladdr      0xa0 fgets
    //   0x38 reserved    0xa8 fseek
    //   0x40 mprotect    0xb0 fwrite
    //   0x48 memcpy      0xb8 fopen
    //   0x50 memset      0xc0 dyld_get_image_name
    //   0x58 fclose      0xc8 free
    //   0x60 sysconf     0xd0 dyld_get_image_slide
    //   0x68 remove      0xd8 mmap

    dispatch->signal_fn                 = resolve_symbol("__signal");
    dispatch->realloc_fn                = resolve_symbol("realloc");
    dispatch->ftell_fn                  = resolve_symbol("ftell");
    dispatch->reserved_18               = nullptr;
    dispatch->pread_fn                  = resolve_symbol("pread");
    dispatch->dlopen_fn                 = resolve_symbol("dlopen");
    dispatch->dladdr_fn                 = resolve_symbol("dladdr");
    dispatch->reserved_38               = nullptr;
    dispatch->mprotect_fn               = resolve_symbol("mprotect");
    dispatch->memcpy_fn                 = resolve_symbol("memcpy");
    dispatch->memset_fn                 = resolve_symbol("memset");
    dispatch->fclose_fn                 = resolve_symbol("fclose");
    dispatch->sysconf_fn                = resolve_symbol("sysconf");
    dispatch->remove_fn                 = resolve_symbol("remove");
    dispatch->fread_fn                  = resolve_symbol("fread");
    dispatch->malloc_fn                 = resolve_symbol("malloc");
    dispatch->dyld_image_count_fn       = resolve_symbol("dyld_image_count");
    dispatch->chmod_fn                  = resolve_symbol("chmod");
    dispatch->stat_fn                   = resolve_symbol("stat");
    dispatch->dlsym_fn                  = resolve_symbol("dlsym");
    dispatch->fgets_fn                 = resolve_symbol("fgets");
    dispatch->fseek_fn                  = resolve_symbol("fseek");
    dispatch->fwrite_fn                 = resolve_symbol("fwrite");
    dispatch->fopen_fn                  = resolve_symbol("fopen");
    dispatch->dyld_get_image_name_fn    = resolve_symbol("dyld_get_image_name");
    dispatch->free_fn                   = resolve_symbol("free");
    dispatch->dyld_get_image_slide_fn   = resolve_symbol("dyld_get_image_vmaddr_slide");
    dispatch->mmap_fn                   = resolve_symbol("mmap");

    return dispatch;
}
