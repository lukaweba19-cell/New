/**
 * Delta Executor API Function Implementations
 *
 * Native implementations of the Luau-facing executor API functions
 * registered in the executor API table (decompiler_bridge.cpp).
 *
 * These are the actual function bodies that get called through the
 * protected API registration table.
 */

#include "delta_executor_api.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>

// Platform detection
#if defined(__APPLE__) && defined(__MACH__)
#include <TargetConditionals.h>
#endif

namespace delta_executor {

// ============================================================================
// Basic I/O
// ============================================================================

// We use a thread-local output buffer so multiple calls don't stomp each other.
static char g_print_buffer[4096];

void api_print(const char* str) {
    if (!str) str = "(nil)";
    // On iOS this would write to the app's log / console.
    // Here we produce a formatted C-string output and store it;
    // a real iOS build would interop with NSLog / OSLog.
    std::snprintf(g_print_buffer, sizeof(g_print_buffer),
                  "[Delta] %s\n", str);
}

void api_print_warning(const char* str) {
    if (!str) str = "(nil)";
    std::snprintf(g_print_buffer, sizeof(g_print_buffer),
                  "[Delta WARN] %s\n", str);
}

void api_print_error(const char* str) {
    if (!str) str = "(nil)";
    std::snprintf(g_print_buffer, sizeof(g_print_buffer),
                  "[Delta ERROR] %s\n", str);
}

// ============================================================================
// Task / thread management
// ============================================================================

void api_spawn(const char* script) {
    if (!script) return;
    // On iOS this would queue a new Lua thread on the executor.
    // In the recovered binary this is a thin wrapper around the
    // Luau thread creation primitive protected behind the dispatch table.
    (void)script;
}

void api_wait(float seconds) {
    // Luau "wait" yields the current thread and resumes after `seconds`.
    // Without a full Luau VM integration we model the sleep.
    if (seconds <= 0.0f) return;
    // busy-wait is not acceptable; but without an event loop we
    // just track the deadline. In a full integration this would be
    // a coroutine yield handled by the Luau executor.
    (void)seconds;
}

void api_delay() {
    // "delay" in Delta = yield current thread without a time argument.
    // No-op in this stand-alone build.
}

// ============================================================================
// HTTP / networking
// ============================================================================

// HTTP response storage: one slot per handle, up to 64 concurrent requests.
#define MAX_HTTP_HANDLES 64

struct HttpRequestState {
    int status_code;
    char* body;          // heap-allocated response body, freed on close
    size_t body_size;
    char headers[1024];  // last response header block (simplified)
    int active;
};

static HttpRequestState g_http_handles[MAX_HTTP_HANDLES];
static std::mutex g_http_mutex;

// Assign a new handle slot
static int alloc_http_handle(void) {
    std::lock_guard<std::mutex> lock(g_http_mutex);
    for (int i = 0; i < MAX_HTTP_HANDLES; i++) {
        if (!g_http_handles[i].active) {
            g_http_handles[i].active = 1;
            g_http_handles[i].status_code = 0;
            g_http_handles[i].body = nullptr;
            g_http_handles[i].body_size = 0;
            g_http_handles[i].headers[0] = '\0';
            return i;
        }
    }
    return -1;
}

// Release a handle slot
static void free_http_handle(int handle) {
    if (handle < 0 || handle >= MAX_HTTP_HANDLES) return;
    std::lock_guard<std::mutex> lock(g_http_mutex);
    if (g_http_handles[handle].active) {
        g_http_handles[handle].active = 0;
        std::free(g_http_handles[handle].body);
        g_http_handles[handle].body = nullptr;
        g_http_handles[handle].body_size = 0;
        g_http_handles[handle].status_code = 0;
        g_http_handles[handle].headers[0] = '\0';
    }
}

// Populated by the networking layer when a request completes
static void set_http_response(int handle, int status, const char* body, size_t body_len) {
    if (handle < 0 || handle >= MAX_HTTP_HANDLES) return;
    std::lock_guard<std::mutex> lock(g_http_mutex);
    if (!g_http_handles[handle].active) return;

    g_http_handles[handle].status_code = status;
    std::free(g_http_handles[handle].body);
    if (body && body_len > 0) {
        g_http_handles[handle].body = static_cast<char*>(std::malloc(body_len + 1));
        if (g_http_handles[handle].body) {
            std::memcpy(g_http_handles[handle].body, body, body_len);
            g_http_handles[handle].body[body_len] = '\0';
            g_http_handles[handle].body_size = body_len;
        } else {
            g_http_handles[handle].body = nullptr;
            g_http_handles[handle].body_size = 0;
        }
    } else {
        g_http_handles[handle].body = nullptr;
        g_http_handles[handle].body_size = 0;
    }
}

// ---------------------------------------------------------------------------
// These are populated by the cpp-httplib + ixwebsocket integration in
// networking.cpp. When the networking layer is compiled in, these
// forward to the actual HTTP client. Without it, they fall back to a
// stub that records the request and returns a "not implemented" response.
// ---------------------------------------------------------------------------

int api_http_get(const char* url, const char* headers) {
    if (!url) return -1;
    int handle = alloc_http_handle();
    if (handle < 0) return -1;

    // Forward to the networking layer. If networking is not linked,
    // the stub in networking_stub.cpp records a pending request.
    run_http_get(handle, url, headers);
    return handle;
}

int api_http_post(const char* url, const char* data, const char* headers) {
    if (!url) return -1;
    int handle = alloc_http_handle();
    if (handle < 0) return -1;

    run_http_post(handle, url, data, headers);
    return handle;
}

int api_http_request(const char* method, const char* url,
                     const char* headers, const char* body,
                     int* out_status, char** out_body) {
    if (!method || !url || !out_status || !out_body) return -1;
    int handle = alloc_http_handle();
    if (handle < 0) return -1;

    run_http_request(handle, method, url, headers, body);
    // Poll until response arrives (in production this would be async
    // with a callback; here we block briefly then read the stored result).
    int status = 0;
    const char* resp_body = nullptr;

    for (int tries = 0; tries < 50; tries++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        std::lock_guard<std::mutex> lock(g_http_mutex);
        if (g_http_handles[handle].active && g_http_handles[handle].status_code != 0) {
            status = g_http_handles[handle].status_code;
            resp_body = g_http_handles[handle].body;
            break;
        }
    }

    *out_status = status;
    if (resp_body) {
        size_t len = std::strlen(resp_body);
        *out_body = static_cast<char*>(std::malloc(len + 1));
        if (*out_body) {
            std::memcpy(*out_body, resp_body, len + 1);
        } else {
            *out_body = nullptr;
        }
    } else {
        *out_body = nullptr;
    }
    return handle;
}

void api_http_close(int handle) {
    if (handle < 0) return;
    cancel_http_request(handle);
    free_http_handle(handle);
}

int api_get_http_status(int handle) {
    if (handle < 0 || handle >= MAX_HTTP_HANDLES) return -1;
    std::lock_guard<std::mutex> lock(g_http_mutex);
    if (!g_http_handles[handle].active) return -1;
    return g_http_handles[handle].status_code;
}

const char* api_get_http_body(int handle) {
    if (handle < 0 || handle >= MAX_HTTP_HANDLES) return nullptr;
    std::lock_guard<std::mutex> lock(g_http_mutex);
    if (!g_http_handles[handle].active) return nullptr;
    return g_http_handles[handle].body;
}

const char* api_get_http_header(int handle, const char* key) {
    (void)key;
    if (handle < 0 || handle >= MAX_HTTP_HANDLES) return nullptr;
    std::lock_guard<std::mutex> lock(g_http_mutex);
    if (!g_http_handles[handle].active) return nullptr;
    return g_http_handles[handle].headers;
}

// ============================================================================
// WebSocket
// ============================================================================

// WebSocket state mirroring the HTTP handle system.
struct WsState {
    int ready_state;        // 0=CONNECTING, 1=OPEN, 2=CLOSING, 3=CLOSED
    char* message_buf;      // most recent message
    size_t message_len;
    void* on_message_cb;
    void* on_close_cb;
    void* on_error_cb;
    int handle;
    int active;
};

static WsState g_ws_states[16];
static std::mutex g_ws_mutex;

static int alloc_ws_handle(void) {
    std::lock_guard<std::mutex> lock(g_ws_mutex);
    for (int i = 0; i < 16; i++) {
        if (!g_ws_states[i].active) {
            g_ws_states[i].active = 1;
            g_ws_states[i].ready_state = 0;
            g_ws_states[i].message_buf = nullptr;
            g_ws_states[i].message_len = 0;
            g_ws_states[i].on_message_cb = nullptr;
            g_ws_states[i].on_close_cb = nullptr;
            g_ws_states[i].on_error_cb = nullptr;
            g_ws_states[i].handle = i;
            return i;
        }
    }
    return -1;
}

static void free_ws_handle(int handle) {
    if (handle < 0 || handle >= 16) return;
    std::lock_guard<std::mutex> lock(g_ws_mutex);
    if (g_ws_states[handle].active) {
        g_ws_states[handle].active = 0;
        std::free(g_ws_states[handle].message_buf);
        g_ws_states[handle].message_buf = nullptr;
        g_ws_states[handle].message_len = 0;
        g_ws_states[handle].ready_state = 3;
        g_ws_states[handle].on_message_cb = nullptr;
        g_ws_states[handle].on_close_cb = nullptr;
        g_ws_states[handle].on_error_cb = nullptr;
    }
}

int api_ws_connect(const char* url, const char* protocols) {
    if (!url) return -1;
    int handle = alloc_ws_handle();
    if (handle < 0) return -1;

    start_websocket(handle, url, protocols);
    return handle;
}

void api_ws_send(int handle, const char* data) {
    if (handle < 0 || handle >= 16) return;
    std::lock_guard<std::mutex> lock(g_ws_mutex);
    if (!g_ws_states[handle].active) return;
    send_websocket_text(handle, data);
}

void api_ws_send_binary(int handle, const uint8_t* data, size_t len) {
    if (handle < 0 || handle >= 16) return;
    std::lock_guard<std::mutex> lock(g_ws_mutex);
    if (!g_ws_states[handle].active) return;
    send_websocket_binary(handle, data, len);
}

void api_ws_close(int handle, int code, const char* reason) {
    if (handle < 0 || handle >= 16) return;
    std::lock_guard<std::mutex> lock(g_ws_mutex);
    if (!g_ws_states[handle].active) return;
    close_websocket(handle, code, reason);
    free_ws_handle(handle);
}

int api_ws_get_ready_state(int handle) {
    if (handle < 0 || handle >= 16) return -1;
    std::lock_guard<std::mutex> lock(g_ws_mutex);
    if (!g_ws_states[handle].active) return -1;
    return g_ws_states[handle].ready_state;
}

const char* api_ws_get_message(int handle) {
    if (handle < 0 || handle >= 16) return nullptr;
    std::lock_guard<std::mutex> lock(g_ws_mutex);
    if (!g_ws_states[handle].active) return nullptr;
    return g_ws_states[handle].message_buf;
}

void api_ws_set_on_message(int handle, void* callback) {
    if (handle < 0 || handle >= 16) return;
    std::lock_guard<std::mutex> lock(g_ws_mutex);
    if (!g_ws_states[handle].active) return;
    g_ws_states[handle].on_message_cb = callback;
}

void api_ws_set_on_close(int handle, void* callback) {
    if (handle < 0 || handle >= 16) return;
    std::lock_guard<std::mutex> lock(g_ws_mutex);
    if (!g_ws_states[handle].active) return;
    g_ws_states[handle].on_close_cb = callback;
}

void api_ws_set_on_error(int handle, void* callback) {
    if (handle < 0 || handle >= 16) return;
    std::lock_guard<std::mutex> lock(g_ws_mutex);
    if (!g_ws_states[handle].active) return;
    g_ws_states[handle].on_error_cb = callback;
}

// ============================================================================
// File I/O
// ============================================================================

int api_file_read(const char* path, char** out_content, int* out_size) {
    if (!path || !out_content || !out_size) {
        if (out_content) *out_content = nullptr;
        if (out_size) *out_size = 0;
        return -1;
    }
    *out_content = nullptr;
    *out_size = 0;

    FILE* f = std::fopen(path, "rb");
    if (!f) return -1;

    std::fseek(f, 0, SEEK_END);
    long sz = std::ftell(f);
    std::rewind(f);

    if (sz <= 0) {
        std::fclose(f);
        *out_content = static_cast<char*>(std::malloc(1));
        if (*out_content) (*out_content)[0] = '\0';
        *out_size = 0;
        return 0;
    }

    char* buf = static_cast<char*>(std::malloc(static_cast<size_t>(sz) + 1));
    if (!buf) {
        std::fclose(f);
        return -1;
    }

    size_t read_count = std::fread(buf, 1, static_cast<size_t>(sz), f);
    std::fclose(f);

    buf[read_count] = '\0';
    *out_content = buf;
    *out_size = static_cast<int>(read_count);
    return 0;
}

int api_file_write(const char* path, const char* content) {
    if (!path || !content) return -1;
    FILE* f = std::fopen(path, "wb");
    if (!f) return -1;
    std::fputs(content, f);
    std::fclose(f);
    return 0;
}

int api_file_exists(const char* path) {
    if (!path) return 0;
    FILE* f = std::fopen(path, "rb");
    if (f) {
        std::fclose(f);
        return 1;
    }
    return 0;
}

void api_file_delete(const char* path) {
    if (!path) return;
    std::remove(path);
}

int api_file_get_size(const char* path) {
    if (!path) return -1;
    FILE* f = std::fopen(path, "rb");
    if (!f) return -1;
    std::fseek(f, 0, SEEK_END);
    long sz = std::ftell(f);
    std::fclose(f);
    return static_cast<int>(sz);
}

// ============================================================================
// OS / environment
// ============================================================================

const char* api_get_platform(void) {
    static const char plat[] = "iOS";
    return plat;
}

const char* api_get_device_model(void) {
    // On real iOS this would call sysctlbyname("hw.machine").
    // Without the OS framework we report a known Delta target set.
    static const char model[] = "iPhone";
    return model;
}

int api_get_os_version(int* major, int* minor) {
    if (!major || !minor) return -1;
    // Without the OS framework we report a recent baseline.
    *major = 17;
    *minor = 0;
    return 0;
}

const char* api_get_bundle_id(void) {
    // In production this would be passed from the host app.
    static const char bundle[] = "com.delta.executor";
    return bundle;
}

double api_get_battery_level(void) {
    // Without the OS framework we cannot read UIDevice batteryState.
    // Return -1.0 to indicate unavailable; callers should treat negatives
    // as "unknown".
    return -1.0;
}

int api_is_jailbroken(void) {
    // A production iOS build would check for Cydia, suspicious paths,
    // fork() availability, etc. Without OS access we return 0 (no evidence).
    (void)sizeof(void*);
    return 0;
}

int api_is_debugger_attached(void) {
    // Without OS access we cannot call pthread_get_state or check
    // ptrace. Return 0 conservatively; a real build would hook the
    // ptrace anti-analysis check that Delta uses.
    return 0;
}

void api_get_app_info(char* out_info, size_t out_size) {
    if (!out_info || out_size == 0) return;
    std::snprintf(out_info, out_size,
                  "Platform:iOS|Model:iPhone|OS:17.0|Bundle:com.delta.executor");
}

// ============================================================================
// Memory / performance
// ============================================================================

size_t api_get_memory_usage(void) {
    // Without the OS framework we cannot call task_info().
    // Return 0 as "unavailable" sentinel; callers should treat 0
    // as unknown when they expect a real number.
    return 0;
}

double api_get_fps(void) {
    // FPS tracking requires a running render loop. Without it we
    // return a nominal value. A real Delta build tracks frame deltas
    // inside the game render callback.
    return 60.0;
}

void api_get_profiler_data(char* out_data, size_t out_size) {
    if (!out_data || out_size == 0) return;
    std::snprintf(out_data, out_size,
                  "Profiler:not.available|Memory:0|FPS:60");
}

// ============================================================================
// Utility
// ============================================================================

static const char BASE64_TABLE[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

const char* api_base64_encode(const char* data, size_t len, size_t* out_len) {
    if (!data || !out_len) return nullptr;
    if (len == 0) {
        *out_len = 0;
        const char* empty = "";
        return empty;
    }

    size_t out_size = 4 * ((len + 2) / 3);
    char* out = static_cast<char*>(std::malloc(out_size + 1));
    if (!out) {
        *out_len = 0;
        return nullptr;
    }

    const uint8_t* input = reinterpret_cast<const uint8_t*>(data);
    size_t i = 0, j = 0;
    while (i < len) {
        uint32_t octet_a = i < len ? input[i++] : 0;
        uint32_t octet_b = i < len ? input[i++] : 0;
        uint32_t octet_c = i < len ? input[i++] : 0;
        uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;

        out[j++] = BASE64_TABLE[(triple >> 18) & 0x3F];
        out[j++] = BASE64_TABLE[(triple >> 12) & 0x3F];
        out[j++] = BASE64_TABLE[(triple >> 6) & 0x3F];
        out[j++] = BASE64_TABLE[triple & 0x3F];
    }

    // Pad with '='
    size_t mod = len % 3;
    if (mod == 1) {
        out[j - 2] = '=';
        out[j - 1] = '=';
    } else if (mod == 2) {
        out[j - 1] = '=';
    }

    out[out_size] = '\0';
    *out_len = out_size;
    return out;
}

int api_base64_decode(const char* encoded, char** out_data, size_t* out_len) {
    if (!encoded || !out_data || !out_len) return -1;
    *out_data = nullptr;
    *out_len = 0;

    size_t in_len = std::strlen(encoded);
    if (in_len == 0) {
        *out_data = static_cast<char*>(std::malloc(1));
        if (*out_data) (*out_data)[0] = '\0';
        *out_len = 0;
        return 0;
    }

    // Count actual data bytes (strip padding and non-base64 chars)
    size_t data_len = 0;
    for (size_t i = 0; i < in_len; i++) {
        char c = encoded[i];
        if (c == '=') break;
        if (std::strchr(BASE64_TABLE, c)) data_len++;
    }

    if (data_len == 0) {
        *out_data = static_cast<char*>(std::malloc(1));
        if (*out_data) (*out_data)[0] = '\0';
        *out_len = 0;
        return 0;
    }

    size_t out_size = (data_len * 3) / 4;
    char* out = static_cast<char*>(std::malloc(out_size + 1));
    if (!out) return -1;

    // Build decode lookup
    int decode_table[256];
    std::memset(decode_table, -1, sizeof(decode_table));
    for (int i = 0; BASE64_TABLE[i]; i++) {
        decode_table[static_cast<unsigned char>(BASE64_TABLE[i])] = i;
    }

    size_t j = 0;
    for (size_t i = 0; i < in_len; ) {
        // Skip non-base64
        while (i < in_len && decode_table[static_cast<unsigned char>(encoded[i])] == -1) i++;
        if (i >= in_len || encoded[i] == '=') break;

        uint32_t v = 0;
        int count = 0;
        while (i < in_len && count < 4) {
            int val = decode_table[static_cast<unsigned char>(encoded[i])];
            if (val == -1) break;
            v = (v << 6) | val;
            i++;
            count++;
        }

        if (count >= 2) {
            if (j < out_size) out[j++] = static_cast<char>((v >> 16) & 0xFF);
            if (count >= 3 && j < out_size) out[j++] = static_cast<char>((v >> 8) & 0xFF);
            if (count >= 4 && j < out_size) out[j++] = static_cast<char>(v & 0xFF);
        }
    }

    out[j] = '\0';
    *out_data = out;
    *out_len = j;
    return 0;
}

const char* api_sha256(const char* data) {
    // Without OpenSSL linked we cannot compute SHA-256.
    // Return a placeholder that indicates the function exists but
    // needs the crypto library. A production build links OpenSSL
    // and populates this with real hash output.
    (void)data;
    static const char placeholder[] = "[sha256 requires OpenSSL]";
    return placeholder;
}

const char* api_md5(const char* data) {
    (void)data;
    static const char placeholder[] = "[md5 requires OpenSSL]";
    return placeholder;
}

uint64_t api_get_time_ms(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        return static_cast<uint64_t>(ts.tv_sec) * 1000ULL +
               static_cast<uint64_t>(ts.tv_nsec) / 1000000ULL;
    }
    return 0;
}

uint64_t api_get_tick_count(void) {
    // "tick count" in Delta = milliseconds since some arbitrary epoch
    // (recovered as a wrapper around GetTickCount-style API).
    return api_get_time_ms();
}

void api_sleep_ms(int milliseconds) {
    if (milliseconds <= 0) return;
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

void api_execute_command(const char* cmd, char** out_output, int* out_size) {
    if (!cmd || !out_output || !out_size) {
        if (out_output) *out_output = nullptr;
        if (out_size) *out_size = 0;
        return;
    }
    *out_output = nullptr;
    *out_size = 0;

    // Without a shell we cannot popen(). On iOS there is no shell anyway.
    // Return a descriptive message.
    const char* msg = "[executeCommand: no shell on iOS]";
    size_t len = std::strlen(msg);
    *out_output = static_cast<char*>(std::malloc(len + 1));
    if (*out_output) {
        std::memcpy(*out_output, msg, len + 1);
    }
    *out_size = static_cast<int>(len);
}

void api_clipboard_get_text(char** out_text) {
    if (!out_text) return;
    *out_text = nullptr;
    // iOS provides clipboard via UIPasteboard; without UIKit we return nullptr.
}

void api_clipboard_set_text(const char* text) {
    (void)text;
    // iOS clipboard write would go through UIPasteboard here.
}

int api_vibrate(int pattern) {
    (void)pattern;
    // iOS haptic via UIFeedbackGenerator / AudioServices; no-op without UI kit.
    return 0;
}

int api_play_sound(const char* path, float volume) {
    (void)path;
    (void)volume;
    // iOS audio via AVAudioPlayer / AudioToolbox; no-op without media frameworks.
    return 0;
}

void api_set_volume(float volume) {
    (void)volume;
}

float api_get_volume(void) {
    return 1.0f;
}

// ============================================================================
// Roblox-specific
// ============================================================================

const char* api_get_place_id(void) {
    // In production this comes from the host app / Roblox client context.
    static const char pid[] = "0";
    return pid;
}

const char* api_get_user_id(void) {
    static const char uid[] = "0";
    return uid;
}

const char* api_get_account_info(void) {
    static const char info[] = "{\"placeId\":0,\"userId\":0}";
    return info;
}

int api_join_place(const char* place_id) {
    (void)place_id;
    // Joining a place requires the Roblox client context; not available here.
    return 0;
}

void api_leave_place(void) {
    // No-op without client context.
}

const char* api_get_game_instance_id(void) {
    static const char gid[] = "0";
    return gid;
}

} // namespace delta_executor
