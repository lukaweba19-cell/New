// Delta Executor bootstrap — real initializer, no protector stubs.
// Builds the native dispatch, initializes Luau runtime state, and loads
// the Lua UI from Resources/ui.lua.

#include "native_dispatch.hpp"

#include <cstdlib>
#include <cstring>

// From the executor main (Obj-C++) — the host creates the lua_State and
// exposes the UNC API table. Here we just trigger the bootstrap sequence.
extern void delta_executor_init(void);

// Global dispatch built by build_native_dispatch().
NativeDispatch* g_native_dispatch = nullptr;

// Forward declarations for runtime init helpers.
extern void reconstructed_set_luau_flag_override(const char*, std::uint32_t);
struct ThresholdLookupTable;
extern void reconstructed_build_threshold_lookup(ThresholdLookupTable&);

void reconstructed_bootstrap() {
    // 1. Build the native dispatch table — resolves function pointers from
    //    the loaded process via dlsym against known symbols.
    g_native_dispatch = build_native_dispatch();

    // 2. Initialize Luau runtime globals.
    reconstructed_set_luau_flag_override("LuauGcTraceUdata", 2);

    // 3. Build the threshold lookup table used by the runtime.
    struct ThresholdLookupTable table;
    reconstructed_build_threshold_lookup(table);
    (void)table;

    // 4. Run the executor C++ initialization (UNC API table, UI loading).
    delta_executor_init();
}
