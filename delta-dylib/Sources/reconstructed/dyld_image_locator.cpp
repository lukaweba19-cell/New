// Control-flow-normalized reconstruction of native 0x3a8258.
// Evidence-based pseudocode; not original vendor source.

#include <cstdint>
#include <cstring>

// In the native function these callbacks are loaded through the 0xe0-byte
// dispatch block at global 0xf0d3e0, and the protected arithmetic used to
// address the fields has been collapsed here.
using ImageCount = std::uint32_t (*)();
using ImageName = const char* (*)(std::uint32_t);

// Global dispatch built by build_native_dispatch() at 0xf0d3e0
extern NativeDispatch* g_native_dispatch;

// Native 0x3a8258: find_loaded_image
// Searches loaded images for one whose name contains the needle string.
// The native function takes the image_count and image_name callbacks as
// arguments (loaded from the dispatch block through protected arithmetic),
// but we collapse that here to read from g_native_dispatch.
const char* find_loaded_image(const char* needle) {
    if (!needle || !g_native_dispatch)
        return nullptr;

    ImageCount image_count =
        reinterpret_cast<ImageCount>(g_native_dispatch->dyld_image_count_fn);
    ImageName image_name =
        reinterpret_cast<ImageName>(g_native_dispatch->dyld_get_image_name_fn);

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
// ASLR slide via dyld_get_image_vmaddr_slide.
long image_slide_for_path(const char* path) {
    if (!path || !g_native_dispatch)
        return 0;

    using ImageSlide = std::intptr_t (*)(std::uint32_t);
    ImageCount image_count =
        reinterpret_cast<ImageCount>(g_native_dispatch->dyld_image_count_fn);
    ImageSlide image_slide =
        reinterpret_cast<ImageSlide>(g_native_dispatch->dyld_get_image_slide_fn);
    ImageName image_name =
        reinterpret_cast<ImageName>(g_native_dispatch->dyld_get_image_name_fn);

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
