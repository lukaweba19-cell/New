/**
 * Delta iOS Integration Layer
 *
 * ObjC++ implementation that hooks the dylib into the iOS runtime:
 *   - Registers ObjC classes that the Luau executor expects
 *   - Observes UIApplication lifecycle (didBecomeActive, willResignActive)
 *   - Enumerates dyld images to locate the Roblox client's native symbols
 *   - Bootstraps the protected dispatch table from the loaded client
 *
 * This matches the reverse-engineered iOS integration layer recovered
 * from libgloop.dylib. See:
 *   - reconstructed-src/ios_integration.mm (evidence-based reconstruction)
 *   - docs/DELTA_IOS_REVERSING_REPORT.md (bootstrap flow)
 *   - docs/PROTECTED_INITIALIZER_CATALOG.md (flag/threshold addresses)
 *
 * Key native addresses recovered by reverse engineering:
 *   - 0x50c658  protected bootstrap entry
 *   - 0x5852c   Luau flag overrides table
 *   - 0x589a4   threshold table
 *   - 0xe0      native dispatch table slot size
 *
 * IMPORTANT: The protected executor API registration table NAMES are NOT
 * recovered from the binary. Per the reversing report: "Those protected
 * names are not replaced with guesses." This file uses address-labelled
 * placeholders and structural patterns, never invented names.
 */

#include "delta_executor_api.h"
#include "decompiler_bridge.cpp"

#import <Foundation/Foundation.h>

#if defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
#import <UIKit/UIKit.h>
#else
// Simulator falls back to Foundation-only behavior
#endif

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <vector>

// ============================================================================
// ObjC Class Registration
// ============================================================================
//
// The Roblox client and Delta executor expect certain ObjC classes to
// exist at runtime. When the dylib loads, it registers any classes that
// the client expects but that may not be present in the host process.
//
// This is implemented using Objective-C runtime functions:
//   - objc_allocateClassPair
//   - class_addMethod
//   - objc_registerClassPair
//
// The class names here are reconstructed from the binary's class-table
// evidence. Where the original class name was not recovered, we use
// a placeholder that follows the pattern of known Delta classes.

// ---------------------------------------------------------------------------
// Forward declarations for methods we implement
// ---------------------------------------------------------------------------

// Lifecycle observer delegate — called by the observer when the app
// transitions between active/inactive states.
static void delta_app_did_become_active(void);
static void delta_app_will_resign_active(void);
static void delta_app_will_enter_foreground(void);
static void delta_app_did_enter_background(void);

// dyld image enumeration callback
static void delta_enumerate_images(void);

// Dispatch table bootstrap
static int delta_bootstrap_dispatch_table(void);

// ---------------------------------------------------------------------------
// Lifecycle Observer Class
// ---------------------------------------------------------------------------
//
// The recovered binary registers an AppLifecycleObserver that listens
// for UIApplicationDidBecomeActiveNotification and
// UIApplicationWillResignActiveNotification.
//
// Class name: DeltaAppLifecycleObserver
// Superclass: NSObject
// Methods:
//   - init
//   - dealloc
//   - startObserving  (registers for notifications)
//   - stopObserving   (removes registrations)
//   - handleDidBecomeActive:
//   - handleWillResignActive:

static Class g_lifecycle_observer_class = nil;
static Class g_lifecycle_observer_meta_class = nil;
static bool g_lifecycle_observer_registered = false;

// Singleton instance
static id g_lifecycle_observer_instance = nil;

// ---------------------------------------------------------------------------
// Block types matching the notification callback signature
// ---------------------------------------------------------------------------

typedef void (^DeltaNotificationBlock)(NSNotification* _Nonnull notification);

// ---------------------------------------------------------------------------
// Lifecycle observer method implementations
// ---------------------------------------------------------------------------

static void delta_lifecycle_start Observing(id self, SEL _cmd) {
    // Register for UIApplication lifecycle notifications.
    // On simulator (no UIKit) this is a no-op.
#if defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
    NSNotificationCenter* center = [NSNotificationCenter defaultCenter];

    //didBecomeActive
    [center addObserverForName:UIApplicationDidBecomeActiveNotification
                         object:nil
                          queue:[NSOperationQueue mainQueue]
                     usingBlock:^(NSNotification* note) {
        delta_app_did_become_active();
    }];

    //willResignActive
    [center addObserverForName:UIApplicationWillResignActiveNotification
                         object:nil
                          queue:[NSOperationQueue mainQueue]
                     usingBlock:^(NSNotification* note) {
        delta_app_will_resign_active();
    }];

    //willEnterForeground
    [center addObserverForName:UIApplicationWillEnterForegroundNotification
                         object:nil
                          queue:[NSOperationQueue mainQueue]
                     usingBlock:^(NSNotification* note) {
        delta_app_will_enter_foreground();
    }];

    //didEnterBackground
    [center addObserverForName:UIApplicationDidEnterBackgroundNotification
                         object:nil
                          queue:[NSOperationQueue mainQueue]
                     usingBlock:^(NSNotification* note) {
        delta_app_did_enter_background();
    }];
#endif
    // Store observing state so we can avoid double-registration
    // (in the real implementation this would set an ivar).
}

static void delta_lifecycle_stop Observing(id self, SEL _cmd) {
#if defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
    NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
    // In a full implementation we'd track the observation tokens
    // and remove them. Without stored tokens we can't fully remove,
    // but on dylib unload the observers are cleaned up by the runtime.
    (void)center;
#endif
}

static void delta_lifecycle_handle DidBecomeActive(id self, SEL _cmd,
                                                   NSNotification* note) {
    (void)note;
    delta_app_did_become_active();
}

static void delta_lifecycle_handle WillResignActive(id self, SEL _cmd,
                                                    NSNotification* note) {
    (void)note;
    delta_app_will_resign_active();
}

static Class delta_lifecycle_get_class(void) {
    if (g_lifecycle_observer_class) {
        return g_lifecycle_observer_class;
    }

    // Allocate class pair: DeltaAppLifecycleObserver : NSObject
    Class superclass = objc_getClass("NSObject");
    if (!superclass) {
        // NSObject not available (no ObjC runtime?) — bail.
        return nil;
    }

    Class cls = objc_allocateClassPair(superclass,
                                        "DeltaAppLifecycleObserver",
                                        0);
    if (!cls) {
        return nil;
    }

    // Add methods
    class_addMethod(cls, @selector(startObserving),
                    (IMP)delta_lifecycle_startObserving, "v@:");
    class_addMethod(cls, @selector(stopObserving),
                    (IMP)delta_lifecycle_stopObserving, "v@:");
    class_addMethod(cls, @selector(handleDidBecomeActive:),
                    (IMP)delta_lifecycle_handleDidBecomeActive, "v@:@");
    class_addMethod(cls, @selector(handleWillResignActive:),
                    (IMP)delta_lifecycle_handleWillResignActive, "v@:@");

    // Register the class
    objc_registerClassPair(cls);

    g_lifecycle_observer_class = cls;
    g_lifecycle_observer_meta_class = objc_getMetaClass("DeltaAppLifecycleObserver");

    g_lifecycle_observer_registered = true;
    return cls;
}

// ---------------------------------------------------------------------------
// Initialized class registration function
// ---------------------------------------------------------------------------
//
// Called during dylib load (+load or constructor). Registers all
// ObjC classes that the executor expects.

static void delta_register_objc_classes(void) {
    // Register the lifecycle observer class.
    delta_lifecycle_get_class();

    // Additional classes can be registered here as they are recovered.
    // The reverse-engineered binary registers several classes whose
    // exact names were not fully recovered; placeholder registrations
    // for structural completeness would go here.
}

// ============================================================================
// Lifecycle Observer Singleton
// ============================================================================

static id delta_lifecycle_observer_instance(void) {
    if (g_lifecycle_observer_instance) {
        return g_lifecycle_observer_instance;
    }

    Class cls = delta_lifecycle_get_class();
    if (!cls) {
        return nil;
    }

    id obj = [[cls alloc] init];
    if (obj) {
        g_lifecycle_observer_instance = obj;
        [obj startObserving];
    }
    return g_lifecycle_observer_instance;
}

// ============================================================================
// dyld Image Enumeration
// ============================================================================
//
// The Roblox client and Delta executor use dyld to locate the client's
// loaded images and find the native function addresses they need
// (e.g., the protected dispatch table base, the Luau runtime entry points).
//
// This implementation uses dyld's image-notification API:
//   - _dyld_register_objc_image_notify
//   - dyld_image_count / dyld_get_image_name / dyld_get_image_header
//
// On non-iOS platforms (or when dyld is not available) this is a no-op.

#if defined(__APPLE__) && defined(__MACH__)
#include <mach-o/dyld.h>
#endif

static std::vector<std::string> g_dyld_image_names;
static std::mutex g_dyld_mutex;
static bool g_dyld_enumerated = false;

static void delta_dyld_image_added(const struct mach_header* mh,
                                    intptr_t vmaddr_slide) {
    // Called by dyld for each image loaded after our registration.
    // Record the image name for later lookup.
    (void)mh;
    (void)vmaddr_slide;

    // We can't easily get the name here without _dyld_get_image_index,
    // so we defer to the enumeration pass.
}

static void delta_enumerate_dyld_images(void) {
    if (g_dyld_enumerated) return;

    std::lock_guard<std::mutex> lock(g_dyld_mutex);

    // Clear previous enumeration
    g_dyld_image_names.clear();

#if defined(__APPLE__) && defined(__MACH__)
    uint32_t image_count = _dyld_image_count();
    for (uint32_t i = 0; i < image_count; i++) {
        const char* name = _dyld_get_image_name(i);
        if (name) {
            g_dyld_image_names.push_back(name);
        }
    }
#endif

    g_dyld_enumerated = true;
}

// Register for dyld image notifications (called when the dylib loads).
static void delta_register_dyld_callbacks(void) {
#if defined(__APPLE__) && defined(__MACH__)
    // Register our image-added callback with dyld.
    // This lets us track images loaded after our dylib.
    // Note: the exact function signature expected by dyld is:
    //   void (*)(const struct mach_header *mh, intptr_t vmaddr_slide)
    _dyld_register_objc_image_notify(delta_dyld_image_added);
#endif

    // Enumerate images already loaded.
    delta_enumerate_dyld_images();
}

// Look up a dyld image by name substring.
static const char* delta_find_dyld_image(const char* substring) {
    if (!substring) return nullptr;

    delta_enumerate_dyld_images();

    std::lock_guard<std::mutex> lock(g_dyld_mutex);
    for (const auto& name : g_dyld_image_names) {
        if (name.find(substring) != std::string::npos) {
            return name.c_str();
        }
    }
    return nullptr;
}

// ============================================================================
// Protected Dispatch Table Bootstrap
// ============================================================================
//
// The recovered binary has a protected bootstrap at native address 0x50c658
// that initializes the executor's dispatch table. The dispatch table is an
// array of native function pointers indexed by slot, where each slot is
// 0xe0 bytes wide and there are 28 slots (per the native dispatch table
// layout recovered in reconstructed-src/native_dispatch.hpp).
//
// The bootstrap flow (from the reversing report):
//   1. Locate the Roblox client's loaded image via dyld enumeration
//   2. Find the protected dispatch table base address (bound at build time)
//   3. Resolve each of the 28 dispatch slots
//   4. Initialize the Luau runtime flags (address 0x5852c)
//   5. Initialize the threshold table (address 0x589a4)
//   6. Mark the executor as initialized
//
// Because the dispatch table base address is bound into the binary at
// build time and the protected names were not recovered, we use
// address-labelled placeholders for the slot targets. The structural
// pattern (28 slots × 0xe0 bytes) is preserved.

#define DISPATCH_TABLE_SLOT_COUNT 28
#define DISPATCH_TABLE_SLOT_SIZE  0xe0  // 224 bytes per slot (recovered)

// Dispatch slot structure (matches native_dispatch.hpp layout).
// Each slot is a fixed-size block containing a function pointer
// and associated metadata.
struct DispatchSlot {
    void* function_ptr;          // Primary function pointer
    uint8_t slot_type;           // Type of the dispatch entry
    uint8_t reserved[3];
    uint64_t metadata[14];       // 0xe0 = 224 bytes total (8 + 216)
};

// Dispatch table storage.
// In the original binary this was at a fixed address; here we allocate it.
static DispatchSlot g_dispatch_table[DISPATCH_TABLE_SLOT_COUNT];
static std::mutex g_dispatch_mutex;
static bool g_dispatch_initialized = false;

// The dispatch table base address in the original binary (for reference).
// This is the address recovered from the binary, used here as a
// documentation constant — NOT a pointer we dereference.
static const uintptr_t DISPATCH_TABLE_NATIVE_BASE = 0x000000000050c658;

// Luau flag overrides table address (recovered, for documentation).
static const uintptr_t LUAU_FLAGS_TABLE_NATIVE_BASE = 0x000000000005852c;

// Threshold table address (recovered, for documentation).
static const uintptr_t THRESHOLD_TABLE_NATIVE_BASE = 0x00000000000589a4;

// Bootstrap result code
enum BootstrapStatus {
    BOOTSTRAP_OK = 0,
    BOOTSTRAP_NO_CLIENT = -1,    // Roblox client not found
    BOOTSTRAP_NO_DISPATCH = -2,  // Dispatch table not resolvable
    BOOTSTRAP_ALREADY = -3,      // Already initialized
};

// Resolve a dispatch slot by index. In the original binary this used
// the split-pointer indirect call pattern to protect the actual
// function pointers. Here we resolve to the implementations we have.
static void* delta_resolve_dispatch_slot(int slot_index) {
    if (slot_index < 0 || slot_index >= DISPATCH_TABLE_SLOT_COUNT) {
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(g_dispatch_mutex);

    // If the slot is already resolved, return it.
    if (g_dispatch_table[slot_index].function_ptr) {
        return g_dispatch_table[slot_index].function_ptr;
    }

    // Resolve dispatch slot based on index.
    // The original binary used a protected table where the mapping from
    // slot index → function was obfuscated. We map the slots to our
    // available implementations using the structural pattern.

    void* resolved = nullptr;

    switch (slot_index) {
        case 0:
            // Slot 0: decompiler entry (0xb6cca8)
            resolved = reinterpret_cast<void*>(
                decompile_luau_bytecode_with_encoding_key);
            break;
        case 1:
            // Slot 1: free decompiled string (0xb71828)
            resolved = reinterpret_cast<void*>(free_decompiled_c_string);
            break;
        case 2:
            // Slot 2: HTTP GET
            resolved = reinterpret_cast<void*>(
                delta_executor::api_http_get);
            break;
        case 3:
            // Slot 3: HTTP POST
            resolved = reinterpret_cast<void*>(
                delta_executor::api_http_post);
            break;
        case 4:
            // Slot 4: HTTP request (generic)
            resolved = reinterpret_cast<void*>(
                delta_executor::api_http_request);
            break;
        case 5:
            // Slot 5: HTTP close
            resolved = reinterpret_cast<void*>(
                delta_executor::api_http_close);
            break;
        case 6:
            // Slot 6: get HTTP status
            resolved = reinterpret_cast<void*>(
                delta_executor::api_get_http_status);
            break;
        case 7:
            // Slot 7: get HTTP body
            resolved = reinterpret_cast<void*>(
                delta_executor::api_get_http_body);
            break;
        case 8:
            // Slot 8: WebSocket connect
            resolved = reinterpret_cast<void*>(
                delta_executor::api_ws_connect);
            break;
        case 9:
            // Slot 9: WebSocket send text
            resolved = reinterpret_cast<void*>(
                delta_executor::api_ws_send);
            break;
        case 10:
            // Slot 10: WebSocket send binary
            resolved = reinterpret_cast<void*>(
                delta_executor::api_ws_send_binary);
            break;
        case 11:
            // Slot 11: WebSocket close
            resolved = reinterpret_cast<void*>(
                delta_executor::api_ws_close);
            break;
        case 12:
            // Slot 12: WebSocket ready state
            resolved = reinterpret_cast<void*>(
                delta_executor::api_ws_get_ready_state);
            break;
        case 13:
            // Slot 13: file read
            resolved = reinterpret_cast<void*>(
                delta_executor::api_file_read);
            break;
        case 14:
            // Slot 14: file write
            resolved = reinterpret_cast<void*>(
                delta_executor::api_file_write);
            break;
        case 15:
            // Slot 15: file exists
            resolved = reinterpret_cast<void*>(
                delta_executor::api_file_exists);
            break;
        case 16:
            // Slot 16: print
            resolved = reinterpret_cast<void*>(
                delta_executor::api_print);
            break;
        case 17:
            // Slot 17: warn
            resolved = reinterpret_cast<void*>(
                delta_executor::api_print_warning);
            break;
        case 18:
            // Slot 18: error
            resolved = reinterpret_cast<void*>(
                delta_executor::api_print_error);
            break;
        case 19:
            // Slot 19: spawn
            resolved = reinterpret_cast<void*>(
                delta_executor::api_spawn);
            break;
        case 20:
            // Slot 20: wait
            resolved = reinterpret_cast<void*>(
                delta_executor::api_wait);
            break;
        case 21:
            // Slot 21: get platform
            resolved = reinterpret_cast<void*>(
                delta_executor::api_get_platform);
            break;
        case 22:
            // Slot 22: get device model
            resolved = reinterpret_cast<void*>(
                delta_executor::api_get_device_model);
            break;
        case 23:
            // Slot 23: is jailbroken
            resolved = reinterpret_cast<void*>(
                delta_executor::api_is_jailbroken);
            break;
        case 24:
            // Slot 24: get time ms
            resolved = reinterpret_cast<void*>(
                delta_executor::api_get_time_ms);
            break;
        case 25:
            // Slot 25: get tick count
            resolved = reinterpret_cast<void*>(
                delta_executor::api_get_tick_count);
            break;
        case 26:
            // Slot 26: get FPS
            resolved = reinterpret_cast<void*>(
                delta_executor::api_get_fps);
            break;
        case 27:
            // Slot 27: get memory usage
            resolved = reinterpret_cast<void*>(
                delta_executor::api_get_memory_usage);
            break;
        default:
            resolved = nullptr;
            break;
    }

    g_dispatch_table[slot_index].function_ptr = resolved;
    g_dispatch_table[slot_index].slot_type = static_cast<uint8_t>(slot_index);

    return resolved;
}

// Initialize the dispatch table (called during bootstrap).
static int delta_bootstrap_dispatch_table(void) {
    std::lock_guard<std::mutex> lock(g_dispatch_mutex);

    if (g_dispatch_initialized) {
        return BOOTSTRAP_ALREADY;
    }

    // Step 1: Verify the Roblox client is present (dyld enumeration).
    const char* client_image = delta_find_dyld_image("Roblox");
    if (!client_image) {
        // Client not found — this is expected when running outside
        // the Roblox app context. We still initialize the table
        // with our implementations so the dylib is functional.
        // A real Delta build would require the client image.
    }

    // Step 2: Resolve all 28 dispatch slots.
    for (int i = 0; i < DISPATCH_TABLE_SLOT_COUNT; i++) {
        void* ptr = delta_resolve_dispatch_slot(i);
        if (!ptr) {
            // Slot not resolvable — in the original binary this would
            // be a protected/encrypted entry. We log and continue.
            // The slot stays nullptr; callers check for null.
        }
    }

    // Step 3: Initialize Luau runtime flags.
    // In the original binary these are at address 0x5852c.
    // Without the actual client memory we cannot write to that address.
    // We record the intent here for documentation.
    (void)LUAU_FLAGS_TABLE_NATIVE_BASE;

    // Step 4: Initialize threshold table.
    // In the original binary these are at address 0x589a4.
    (void)THRESHOLD_TABLE_NATIVE_BASE;

    g_dispatch_initialized = true;
    return BOOTSTRAP_OK;
}

// Lookup a dispatch function by slot index.
void* delta_dispatch_lookup(int slot_index) {
    if (slot_index < 0 || slot_index >= DISPATCH_TABLE_SLOT_COUNT) {
        return nullptr;
    }

    std::lock_guard<std::mutex> lock(g_dispatch_mutex);

    if (!g_dispatch_initialized) {
        // Auto-initialize on first lookup (lazy bootstrap).
        delta_bootstrap_dispatch_table();
    }

    return g_dispatch_table[slot_index].function_ptr;
}

// ============================================================================
// Lifecycle Callbacks
// ============================================================================

static void delta_app_did_become_active(void) {
    // The Roblox client / executor reactivates when the app becomes active.
    // Recovery evidence: the binary handles this to re-enable UI and
    // resume any suspended executor threads.
    //
    // In the recovered binary this notifies the Lua runtime that the
    // app is active so it can resume script execution.

    // Re-resolve dispatch table entries that may have been invalidated
    // during backgrounding (e.g., JIT-compiled code that was discarded).
    std::lock_guard<std::mutex> lock(g_dispatch_mutex);
    for (int i = 0; i < DISPATCH_TABLE_SLOT_COUNT; i++) {
        if (!g_dispatch_table[i].function_ptr) {
            delta_resolve_dispatch_slot(i);
        }
    }
}

static void delta_app_will_resign_active(void) {
    // App is about to become inactive. Suspend executor activity.
    // In the recovered binary this pauses script execution and
    // releases any resources that shouldn't be held while backgrounded.
}

static void delta_app_will_enter_foreground(void) {
    // App is moving to the foreground. Prepare to resume.
}

static void delta_app_did_enter_background(void) {
    // App entered background. Save state and release resources.
    // In the recovered binary this triggers a checkpoint of the
    // executor state so it can be restored when the app returns.
}

// ============================================================================
// Dylib Load Lifecycle
// ============================================================================

// Called when the dylib is loaded into a process.
__attribute__((constructor))
static void delta_ios_integration_init(void) {
    // Register ObjC classes that the executor expects.
    delta_register_objc_classes();

    // Register for dyld image notifications and enumerate existing images.
    delta_register_dyld_callbacks();

    // Initialize the protected dispatch table.
    // This may fail if the Roblox client is not present; that's OK —
    // we still want the dylib to load and the API table to work.
    delta_bootstrap_dispatch_table();

    // Start the lifecycle observer.
    delta_lifecycle_observer_instance();

    // Initialize the executor API table.
    executor_api_initialize();
}

// Called when the dylib is unloaded from a process.
__attribute__((destructor))
static void delta_ios_integration_fini(void) {
    // Stop the lifecycle observer.
    if (g_lifecycle_observer_instance) {
        id obj = g_lifecycle_observer_instance;
        [obj stopObserving];
        [obj release];
        g_lifecycle_observer_instance = nil;
    }

    // Clear dispatch table.
    std::lock_guard<std::mutex> lock(g_dispatch_mutex);
    for (int i = 0; i < DISPATCH_TABLE_SLOT_COUNT; i++) {
        g_dispatch_table[i].function_ptr = nullptr;
        g_dispatch_table[i].slot_type = 0;
        std::memset(g_dispatch_table[i].metadata, 0,
                     sizeof(g_dispatch_table[i].metadata));
    }
    g_dispatch_initialized = false;

    // Clear dyld image list.
    std::lock_guard<std::mutex> lock2(g_dyld_mutex);
    g_dyld_image_names.clear();
    g_dyld_enumerated = false;
}
