// Control-flow-normalized reconstruction of native 0x3a8258 (find_loaded_image)
// and native image_slide_for_path. Evidence-based; not vendor original source.

#include "native_dispatch.hpp"

#include <cstdint>
#include <cstring>

// Global dispatch table built by build_native_dispatch() at 0xf0d3e0
extern NativeDispatch* g_native_dispatch;

// dyld function typedefs
using ImageCountFn = std::uint32_t (*)();
using ImageNameFn = const char* (*)(std::uint32_t);
using ImageSlideFn = std::intptr_t (*)(std::uint32_t);

// Native 0x3a8258: find_loaded_image
// Searches loaded images for one whose name contains the needle string.
// Uses the dispatch table's dyld_image_count and dyld_get_image_name slots.
const char* find_loaded_image(const char* needle) {
    if (!g_native_dispatch || !needle)
        return nullptr;

    ImageCountFn image_count = reinterpret_cast<ImageCountFn>(g_native_dispatch->dyld_image_count_fn);
    ImageNameFn image_name = reinterpret_cast<ImageNameFn>(g_native_dispatch->dyld_get_image_name_fn);

    if (!image_count || !image_name)
        return nullptr;

    const std::uint32_t count = image_count();
    for (std::uint32_t index = 0; index < count; ++index) {
        const char* path = image_name(index);
        if (path && std::strstr(path, needle))
            return path;
    }
    return nullptr;
}

// Native image_slide_for_path: given a loaded image path, returns its
// ASLR slide via dyld_get_image_slide.
long image_slide_for_path(const char* path) {
    if (!g_native_dispatch || !path)
        return 0;

    ImageCountFn image_count = reinterpret_cast<ImageCountFn>(g_native_dispatch->dyld_image_count_fn);
    ImageSlideFn image_slide = reinterpret_cast<ImageSlideFn>(g_native_dispatch->dyld_get_image_slide_fn);
    ImageNameFn image_name = reinterpret_cast<ImageNameFn>(g_native_dispatch->dyld_get_image_name_fn);

    if (!image_count || !image_slide || !image_name)
        return 0;

    const std::uint32_t count = image_count();
    for (std::uint32_t index = 0; index < count; ++index) {
        const char* image_path = image_name(index);
        if (image_path && std::strcmp(image_path, path) == 0)
            return static_cast<long>(image_slide(index));
    }
    return 0;
}
